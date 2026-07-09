# DSP threading and peripheral architecture

No prior document covered this — `.github/copilot-instructions.md` lists the peripherals
that exist (`## Architecture` → "Peripherals: ESAI, HDI08") and the ESSI1 ring bus for
voice expansion, but nothing maps peripherals to threads or describes how the "external
interrupt" story actually works end to end. This fills that gap.

**Status as of 2026-07-08: two independent attempts at a WAIT-state idle-sleep mechanism
both caused intermittent audible crackling in live Bitwig testing and were reverted. See
"Case study #1" and "Case study #2" below before attempting a third.** The current working
hypothesis, after both attempts, is that the common factor - not the specific mechanism
each one used to decide *when* to sleep - is the actual cause: see "What both attempts had
in common" at the end.

Scope: this describes the `virusLib`/Osirus/OsTIrus path in detail, since that's what was
traced empirically. The other synth libs (`mqLib`, `xtLib`, `nord/n2x`, `ronaldo/je8086`)
share the same `dsp56kEmu` core (`DSPThread`, `IPeripherals`, `Audio`, `HDI08`) and the same
structural issue applies to all of them, but their host-communication glue differs (some use
a real emulated MC68K via `mc68k/`, virusLib's "Microcontroller" is a plain C++ helper class,
not an emulated CPU).

## Thread inventory (Osirus/OsTIrus)

| Thread | Created by | Runs | Throttle |
|---|---|---|---|
| Host audio thread | The DAW (JUCE plugin host process) | `Device::sendMidi()`, `Device::processAudio()` | Paced by the host's own audio callback cadence |
| `DSPThread` ("DSP A", and "DSP B" for TI2) | `DspSingle::startDSPThread()` ([dspSingle.cpp:69](../source/virusLib/dspSingle.cpp#L69)) | `dsp56k::DSP::exec()` in batches of 128, forever | **None** — see below |

That's it — there is no separate MC68K thread for Virus/OsTIrus (unlike microQ/Xenia/Nord,
which emulate a real 68000 host controller via `mc68k/`). `virusLib::Microcontroller` is a
plain object, not a thread; its `process()` method runs *inside* the DSP thread's call stack
(see below), and its `sendMIDI()`/`sendSysex()` methods run on the host audio thread.

## `DSPThread::threadFunc()` — the core loop

[dspthread.cpp:93](../source/dsp56300/source/dsp56kEmu/dspthread.cpp#L93):

```cpp
while(m_runThread)
{
    Guard g(m_mutex);
    for(size_t i=0; i<128; i += 8)
        { m_dsp.exec(); ... }  // x8, unrolled
    m_callback(...);
}
```

No sleep, no wait, no yield. Runs at Mach `THREAD_TIME_CONSTRAINT_POLICY` (realtime scheduling
class — [threadtools.cpp:183](../source/dsp56300/source/dsp56kBase/threadtools.cpp#L183),
`ThreadPriority::Highest`). Measured on the live system: this thread sits at ~99% `Running`
scheduler occupancy whether or not any note is active, because it is *also* the thread that
owns the audio-tick cadence (see "Why op_Wait can't fix this alone" below).

## Peripherals: what they are, and who drives them

All of these are serviced from `Peripherals56362::exec()` (or `Peripherals56303::exec()` for
Model A) — called from *inside* `DSP::execPeripherals()`, which is called from *inside*
`DSPThread`'s `exec()` loop. **Every peripheral below is serviced on the DSP thread**, no
matter how "external" the thing it represents is.

- **ESAI / ESSI** (`esai.h`/`essi.h`, paced by `EsaiClock`/`EssiClock` in `esaiclock.cpp`) —
  the audio serial ports. This is the DSP's own simulated clock ticking at
  `~m_cyclesPerSample` (a few thousand simulated cycles), and it fires *whether or not a note
  is playing*, because the host expects a continuous PCM stream (silence is still a
  deliverable). On TI2, ESSI1 doubles as the inter-DSP ring bus between "DSP A" and "DSP B"
  (see the Voice Expansion section of `.github/copilot-instructions.md` for the microQ/Xenia
  equivalent). **This clock is internally generated, not externally triggered** — it advances
  purely as a function of the DSP's own instruction counter.
- **HDI08** (`hdi08.h`/`.cpp`) — the actual external boundary. `HDI08::writeRX()`
  ([hdi08.cpp:152](../source/dsp56300/source/dsp56kEmu/hdi08.cpp#L152)) is called from the
  **host audio thread** (via `Device::sendMidi()` → `Microcontroller::sendMIDI()`/`sendSysex()`
  → eventually `writeRX`), pushing into `m_dataRX`, a `RingBuffer<TWord, 8192, true>`
  ([hdi08.h:184](../source/dsp56300/source/dsp56kEmu/hdi08.h#L184)) — `Lock=true` means a real
  `SpscSemaphoreWithCount` (condition_variable-backed), so this is a genuine, safe cross-thread
  queue. Critically, `writeRX` also calls `m_periph.setDelayCycles(0)` — this forces
  `IPeripherals::m_targetClock` to "now", which is the actual mechanism that wakes a
  WAIT-converged DSP thread promptly when the host injects MIDI. **This is the one peripheral
  that's genuinely externally triggered**, and the wake mechanism for it already exists and
  already works — it doesn't need a new manager thread.
- **Timers** (`timers.h`) — periodic hardware timers, entirely internally generated, used by
  firmware for LFO/envelope stepping, MIDI clock, housekeeping. Mirrors real silicon; fires
  regardless of note activity by design. Not a candidate for "sleep more" — it's supposed to
  tick.
- **DMA** (`dma.h`), **GPIO** (`gpio.h`) — register/state bookkeeping, low activity in
  practice.
- **`HDI08Queue`** (`hdi08queue.h`) — a *different* class from `HDI08` itself, used for
  distributing the boot command stream across multiple DSPs; has its own `std::mutex`. Not
  part of the steady-state audio path.

### The `Microcontroller::process()` wrinkle

`Device::onAudioWritten()` ([device.cpp:597](../source/virusLib/device.cpp#L597)) calls
`m_mc->process()`, which calls `m_hdi08.exec()` *again* (in addition to the normal
`Peripherals56362::exec()` → `HDI08::exec()` path). This callback is wired via
`Audio::setCallback()` ([device.cpp:31](../source/virusLib/device.cpp#L31)) to
`Audio::m_writeTxCallback` ([audio.cpp:18](../source/dsp56300/source/dsp56kEmu/audio.cpp#L18)),
which fires *synchronously inside* `Esai::execTX()` — i.e. **on the DSP thread**, once per
output audio frame. It looks like a second thread's worth of housekeeping (preset
write/read-confirmation handshakes, timeouts — see
[microcontroller.cpp:1079](../source/virusLib/microcontroller.cpp#L1079)) but it isn't; it's
nested nested inside the same `DSPThread::exec()` call stack. It's guarded by
`Microcontroller`'s own `m_mutex`, which *is* real cross-thread protection against the host
thread's concurrent `sendMIDI()`/`sendSysex()` calls — that part is correctly synchronized,
just easy to miss because the mutex acquisition is buried three calls deep in a peripheral
callback.

## The real synchronization points (the whole picture)

```
Host audio thread                          DSPThread
──────────────────                         ─────────
Device::sendMidi()
  → Microcontroller::sendMIDI()
    → HDI08::writeRX()  ────────────►  RingBuffer<TWord,8192,true>  ────────────►  HDI08::exec()
                                        (real semaphore, cross-thread safe)         (drains, raises IRQ)

Device::processAudio()
  → Audio::processAudioInput()  ──────►  RingBuffer<RxFrame,32768,true>  ────────►  Esai::execRX()
  → Audio::processAudioOutput()  ◄──────  RingBuffer<TxFrame,32768,true>  ◄────────  Esai::execTX()
    (blocks on semaphore if empty)         (blocks producer if full)                  → Microcontroller::process()
                                                                                          (same thread, nested)
```

The host↔DSP boundary is real and correctly synchronized (semaphores, one mutex). What's
*not* separated is **audio-tick pacing vs. instruction execution vs. everything-else
peripheral servicing** — all three live in the one `DSPThread` loop with no throttle.

## Why `op_Wait` can't fix this alone

`DSP::op_Wait()` ([dsp.cpp:1460](../source/dsp56300/source/dsp56kEmu/dsp.cpp#L1460))
fast-forwards `m_instructions`/`m_cycles` — the DSP's *simulated* clock — directly to the next
scheduled peripheral event, in a handful of loop iterations. It costs almost nothing in *real*
host time, because it's pure arithmetic, not per-cycle simulation. The problem: nothing
downstream of it ever converts "the simulated clock says wait until tick N" into an actual
`sleep_for`. `DSPThread::threadFunc()`'s outer loop has no idea `op_Wait` just fast-forwarded
through a gap — it just calls `exec()` again immediately, which re-enters `op_Wait`, which
converges again instantly, forever. The audio-tick cadence (`EsxiClock`, a few thousand
simulated cycles ≈ tens of microseconds of *real* time at typical clock speeds) repeats often
enough, and each repetition does *some* real work (the ISR that actually produces the sample),
that the thread stays close to 100% "Running" occupancy independent of whether notes are
active — confirmed empirically via Instruments captures of both states.

This directly answers the "why is this handled in the same thread" question: **the audio
clock is the DSP's own simulated instruction counter.** It isn't an external interrupt source
that a separate thread could watch and signal — it only exists/advances while `DSPThread` is
running. You cannot cleanly hand "wait for the next audio tick" off to another thread without
that thread essentially re-implementing `EsxiClock`'s cycle bookkeeping, at which point you've
just moved the problem, not solved it.

What *can* be handed off (and already is, correctly) is HDI08/MIDI — the one genuinely
external, asynchronous source, and it already has a real semaphore-based queue with a working
wake path (`setDelayCycles(0)`).

## Case study #1: shared-clock-state prototype (2026-07-08)

A prototype was built and reverted this session; documenting it here so it isn't
re-attempted the same way.

**What it did:** `EsxiClock::exec()` was changed to return a large delay (65536 cycles)
instead of its normal ~2000-cycle re-arm when no ESAI/ESSI channel had a transmitter/receiver
enabled. `DSPThread` gained an opt-in `idleSleepEnabled` mode: after each batch of 128
`exec()` calls, if `DSP::getRemainingPeripheralsCycles()` showed a gap ≥32768 cycles, it
blocked on a `condition_variable` in bounded 250µs chunks instead of spinning.

**Measured result:** no CPU improvement (expected — in the tested scenario the plugin was
loaded and unmuted in Bitwig, so the output channel was genuinely enabled and streaming the
whole time; the "nothing enabled" branch never fired for the *intended* case). **Unexpected
result: intermittent audio crackling.**

**Root cause (identified, not fully proven, but plausible enough to revert on):**
`EsxiClock::getRemainingInstructionsForFrameSync()`
([esaiclock.cpp:120](../source/dsp56300/source/dsp56kEmu/esaiclock.cpp#L120)) — which feeds
`skipToFrameSync` in the JIT ([jitops_jmp.cpp](../source/dsp56300/source/dsp56kEmu/jitops_jmp.cpp)),
used on *every* WAIT-adjacent JIT block, not just idle ones — also reads
`getRemainingPeripheralsCycles()`. If the "nothing enabled" branch fired even briefly during a
channel reconfiguration, it could make the JIT frame-sync convergence helper skip further
ahead than it should before re-checking the transmit-frame-sync flag, plausibly mistiming or
dropping a sample. **Lesson: `IPeripherals::m_targetClock`/`m_delayCycles` is shared,
multi-consumer state — `getRemainingPeripheralsCycles()` is read by both the idle-detection
code and the always-hot frame-sync JIT helper. Any change to what that value means affects
both, even though only one of them was intended to change.**

A second, independent risk (not proven to have contributed, but real): `DSPThread` runs under
Mach `THREAD_TIME_CONSTRAINT_POLICY`. Voluntarily blocking a realtime-scheduled thread on a
plain `std::condition_variable` is a known source of unpredictable wake latency — generic
condvars aren't realtime-aware and the thread may not be re-admitted to its scheduling
"reservation" promptly on wake. This alone is reason enough not to sleep the DSP thread
directly without care, independent of the `EsxiClock` interaction above.

Both changes were reverted; `esaiclock.cpp` is back to byte-identical with upstream, and
`dspthread.h/.cpp` are back to pristine (no dead code left behind).

## Case study #2: samples-produced-vs-wall-clock pacing (2026-07-08, same day)

Built and tested as a follow-up to case study #1, on the theory that the crackle was
specifically caused by touching `getRemainingPeripheralsCycles()` / `IPeripherals::m_targetClock`
- shared state the hot frame-sync JIT path also reads. This attempt was designed to prove
that theory by avoiding shared state entirely:

1. **New counter, not shared with anything:** `Audio::getFramesWritten()` — a trivial
   read-only getter over the *existing* `m_writeFrameIndex`, which `Audio::m_writeTxCallback`
   ([audio.cpp:18](../source/dsp56300/source/dsp56kEmu/audio.cpp#L18)) already incremented on
   every produced output frame, on the DSP thread, before this change. Reading it added a new
   accessor but touched no existing logic.
2. **Compared against wall-clock, entirely inside `DSPThread`:** `setRealtimePacing(getter,
   sampleRate)` stored a getter callback and the host sample rate (threaded through from
   `Device::bootDSP`/`bootDSPs`, which needed a signature change since `bootDSP` is `static`
   and couldn't reach `Device::m_samplerate` directly - caught by a compile error, not by
   testing). `pacedSleep()` computed `aheadSeconds = samplesProduced/sampleRate -
   elapsedWallClock` once ahead by ≥10ms, and slept in ≤2ms chunks.
3. **Used `std::this_thread::sleep_for`, not a `condition_variable`** - specifically to
   rule out the realtime-thread-blocked-on-a-condvar risk flagged in case study #1, by not
   using a condvar at all. No mutex held during the sleep. No shared peripheral/JIT state
   read anywhere in the new code (confirmed by re-review before shipping the test build).

**Verified before testing:** full `dsp56kTestRunner` suite passed, Release build, symbols
present in the deployed binary (`nm` grep).

**Measured result: crackling again ("sometimes"), reported after a longer test than case
study #1's "brief testing."** Reverted in full - `dsp56300` submodule and `virusLib` are
back to byte-identical with upstream (confirmed via `git status`/`git diff`).

## What both attempts had in common

Both mechanisms differed completely in *what data they read* to decide when to sleep (shared
DSP-cycle state vs. a private sample counter) and *how they slept* (condition_variable vs.
plain `sleep_for`). Both caused the same symptom. The one thing they shared: **both put
`DSPThread` - a thread running under Mach `THREAD_TIME_CONSTRAINT_POLICY`
([threadtools.cpp:183](../source/dsp56300/source/dsp56kBase/threadtools.cpp#L183)) - to
voluntary sleep at all**, however briefly and however that sleep was implemented.

This shifts the leading hypothesis from "case study #1's specific shared-state bug" to
something more structural: **this realtime thread class may not tolerate being voluntarily
suspended and resumed by application code, regardless of mechanism.** Plausible reasons,
neither confirmed:
- Waking from *any* sleep (timed or condvar) may not promptly restore the thread to its
  `THREAD_TIME_CONSTRAINT_POLICY` "computation/constraint" scheduling window, causing it to
  be scheduled late relative to when the audio pipeline needed it, on an unpredictable subset
  of wakes - consistent with "sometimes," not "always."
- `ThreadTools::setCurrentThreadRealtimeParameters()` ([threadtools.cpp:183](../source/dsp56300/source/dsp56kBase/threadtools.cpp#L183))
  configures the policy with `period=0` (non-periodic - see `usePeriod = false` in that
  function). A thread declared non-periodic to the scheduler, then made to sleep and resume
  on an actually-periodic-ish cadence by application code, may simply be operating outside
  what that policy configuration was validated for.

**Neither of these has been verified** - this is inference from two data points, not a
proven mechanism. If a third attempt is made, the highest-value next step is probably
confirming or ruling out *this* hypothesis specifically (e.g. a build that sleeps
unconditionally on a fixed timer regardless of any pacing logic, to isolate "does sleeping
this thread at all cause the crackle" from "does *this particular pacing decision* cause
it") before investing in a new pacing algorithm - a third attempt at "when to sleep" without
first confirming sleeping-at-all is safe would likely just be a third data point for the
same underlying question.

An untried alternative that sidesteps the question entirely: instead of sleeping the
realtime-scheduled `DSPThread` itself, lower its OS scheduling priority/policy (drop out of
`THREAD_TIME_CONSTRAINT_POLICY`) during confirmed-idle stretches and restore it when work
resumes - never suspending the thread, only how favorably the OS schedules it. Not attempted
this session.

## Case study #3 (2026-07-08/09): the "confirm sleeping-at-all is safe" test - result is negative

The suggested next step from the previous section was carried out: isolate "does voluntarily
sleeping `DSPThread` at all cause the crackle" from "does a specific pacing decision cause
it," using a fixed-duration sleep with no pacing logic whatsoever.

**Mechanism:** `DSPThread::diagnosticSleep()`
([dspthread.cpp](../source/dsp56300/source/dsp56kEmu/dspthread.cpp)) sleeps
`std::this_thread::sleep_for(300us)` gated purely by `++m_diagnosticSleepCounter % 100`, no
peripheral/JIT/shared state read anywhere. Unlike both case studies #1 and #2, this is not
trying to be a real feature - it can't reduce CPU (the average added overhead is
duration/N per iteration, roughly constant regardless of load), it exists solely to answer
the yes/no question. Gated by `setDiagnosticSleepEnabled(bool)`/
`getDiagnosticSleepEnabled()`.

**Made live-toggleable** rather than requiring a rebuild per test: `virusLib::Device::set/
getDiagnosticSleepEnabled()` → `virus::VirusProcessor::set/getDiagnosticSleepEnabled()`
(persisted to the JUCE config under `diagnosticDspIdleSleep`, default `false`) → a checkbox
in the plugin's own Settings UI (`source/osTIrusJucePlugin/tus_settings_dspaudio_OsTIrus.rml`,
id `btDiagnosticDspSleep`, inside the existing `settings-advanced` gated section) wired in
`SettingsDspAudioOsTIrus.cpp`. This let the user flip the sleep on/off during live playback
in Bitwig without a rebuild between each try.

**Intermediate variant also tried:** the user reported the crackle seemed to correlate with
preset changes ("only occasional cracks appear after (or during) preset change"). Mechanism
investigated: `Device::onAudioWritten()` (audio-sample production) and `HDI08::writeRX()`
(preset-load burst traffic, rate-limited to one word per 200 DSP cycles) both happen nested
inside the same `m_dsp.exec()` call on `DSPThread` - there's no separate thread for
peripheral servicing. Hypothesis: a preset load transiently shrinks whatever real-time margin
the thread normally has, and the fixed sleep only becomes audible when it lands in that
window. Tested via `DSPThread::notifyBurstActivity()`, called from `Device::sendMidi()`
right before `sendSysex()`, which suppresses `diagnosticSleep()` for a 5ms cooldown after any
sysex/preset dispatch. **This did not eliminate the crackle either.**

**Final result: the crackle occurred with the diagnostic sleep both ON and OFF.** The user's
own read: "seems it's something unrelated, or patch specific." This is a clean negative
result for the entire voluntary-sleep hypothesis built up across case studies #1 and #2 -
neither of those attempts included this control (a zero-pacing-logic sleep, or a genuine
no-sleep-at-all baseline), so it's quite plausible their crackling was this same
pre-existing, unrelated bug all along, misattributed to their sleep mechanisms because sleep
was the only variable being changed at the time.

**Revised conclusion: "DSPThread cannot tolerate voluntary suspension under
`THREAD_TIME_CONSTRAINT_POLICY`" should be treated as unconfirmed, not settled.** If CPU
savings via idle-sleep are worth pursuing again, re-running this exact diagnostic-sleep
control first (cheap, reversible, already built) before investing in new pacing logic is the
right sequencing - it directly answers the question a real prototype's crackle-or-not result
can't disambiguate on its own.

**Left in the tree, not reverted:** the live toggle and `notifyBurstActivity()` machinery are
harmless (default off) but still explicitly diagnostic-only - strip before any real release.
The actual crackle's cause is still open; see the "Fourth investigation" entry in
[[project_bitwig_cpu_creep_investigation_2026-07]] memory for where that stands.
