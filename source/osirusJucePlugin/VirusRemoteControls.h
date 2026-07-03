#pragma once

// Remote controls page definitions for the classic Virus A/B/C (Osirus).
// Parameter names must match "name" fields in parameterDescriptions_C.json exactly.
// nullptr marks an unused slot (permitted by the CLAP spec).
//
// Classic hardware has no Filter Bank insert board and combines Reverb/Delay
// into a single "Delay/Reverb" effect (unlike Virus TI, which has both
// simultaneously plus a separate Filter Bank/Distortion/EQ insert board), so
// the effects pages here differ from VirusTIRemoteControls.h accordingly.

#include "clap-juce-extensions/clap-juce-extensions.h"

#include <cstdint>
#include <iterator>

namespace virusC
{
	struct RemoteControlsPage
	{
		const char* sectionName;
		const char* pageName;
		const char* params[CLAP_REMOTE_CONTROLS_COUNT];
	};

	static constexpr RemoteControlsPage g_remoteControlsPages[] =
	{
		// ---- Sound sources --------------------------------------------------

		{ "Oscillators", "Oscillator 1",
		  { "Osc1 Wave Select", "Osc1 Shape",    "Osc1 Pulsewidth", "Osc1 Semitone",
		    "Osc1 Keyfollow",   "Osc1 Shape Velocity", "Noise Volume", "Noise Color" } },

		{ "Oscillators", "Oscillator 2",
		  { "Osc2 Wave Select", "Osc2 Shape",    "Osc2 Pulsewidth", "Osc2 Semitone",
		    "Osc2 Detune",      "Osc2 FM Amount","Osc2 Sync",       "Osc2 Filt Env Amt" } },

		{ "Oscillators", "Oscillator 3 / Common",
		  { "Osc3 Mode",   "Osc3 Volume",           "Osc3 Semitone",        "Osc3 Detune",
		    "Osc Balance", "Suboscillator Volume",  "Suboscillator Shape",  "Ringmodulator Volume" } },

		// ---- Filter ---------------------------------------------------------

		{ "Filter", "Filter 1",
		  { "Cutoff",          "Filter1 Resonance", "Filter1 Env Amt",     "Filter1 Keyfollow",
		    "Filter1 Mode",    "Filter1 Env Polarity", "Filter Select",    "Filter Routing" } },

		{ "Filter", "Filter 2",
		  { "Cutoff2",         "Filter2 Resonance", "Filter2 Env Amt",     "Filter2 Keyfollow",
		    "Cutoff2Offset",   "Filter Balance",    "Filter2 Mode",        "Filter2 Cutoff Link" } },

		{ "Filter", "Filter Envelope",
		  { "Filter Env Attack", "Filter Env Decay",        "Filter Env Sustain",
		    "Filter Env Sustain Time", "Filter Env Release", "Osc2 Filt Env Amt",
		    "Filter1 Env Polarity",    nullptr } },

		// ---- Amplifier ------------------------------------------------------

		{ "Amplifier", "Amplifier",
		  { "Amp Env Attack", "Amp Env Decay",        "Amp Env Sustain",
		    "Amp Env Sustain Time", "Amp Env Release", "Patch Volume",
		    "Panorama",       nullptr } },

		// ---- LFOs -----------------------------------------------------------

		{ "LFO", "LFO 1",
		  { "Lfo1 Rate",        "Lfo1 Shape",        "Lfo1 Symmetry",
		    "Osc1 Lfo1 Amount", "Osc2 Lfo1 Amount",  "PW Lfo1 Amount",
		    "Reso Lfo1 Amount", "Lfo1 Mode" } },

		{ "LFO", "LFO 2",
		  { "Lfo2 Rate",         "Lfo2 Shape",          "Lfo2 Symmetry",
		    "FM Lfo2 Amount",    "Cutoff1 Lfo2 Amount", "Cutoff2 Lfo2 Amount",
		    "Pan Lfo2 Amount",   "Lfo2 Mode" } },

		{ "LFO", "LFO 3",
		  { "Lfo3 Rate",       "Lfo3 Shape",        "Lfo3 Mode",
		    "Lfo3 Destination","Osc Lfo3 Amount",   "Lfo3 Fade-In Time",
		    "Lfo3 Keyfollow",  "Lfo3 Clock" } },

		// ---- Effects --------------------------------------------------------

		{ "Effects", "Chorus",
		  { "Chorus Mix",      "Chorus Rate",    "Chorus Depth",    "Chorus Delay",
		    "Chorus Feedback", "Chorus Lfo Shape", nullptr,         nullptr } },

		{ "Effects", "Delay/Reverb",
		  { "Delay/Reverb Mode", "Effect Send",   "Delay Time",       "Delay Feedback",
		    "Dly Rate / Rev Decay", "Rev Size",   "Reverb Damping",   "Delay Color" } },

		{ "Effects", "Phaser",
		  { "Phaser Mode",     "Phaser Mix",     "Phaser Rate",     "Phaser Depth",
		    "Phaser Frequency","Phaser Feedback","Phaser Spread",   nullptr } },

		{ "Effects", "EQ",
		  { "LowEQ Gain",    "LowEQ Frequency",
		    "MidEQ Gain",    "MidEQ Frequency",  "MidEQ Q-Factor",
		    "HighEQ Gain",   "HighEQ Frequency", nullptr } },

		{ "Effects", "Distortion",
		  { "Distortion Curve", "Distortion Intensity", "Bass Intensity", "Bass Tune",
		    "Punch Intensity",  nullptr,                nullptr,          nullptr } },

		// ---- Arpeggiator & Performance --------------------------------------

		{ "Arpeggiator", "Arpeggiator",
		  { "Arp Mode",        "Arp Pattern Selct", "Arp Clock",        "Arp Note Length",
		    "Arp Octave Range","Arp Hold Enable",   "Arp Swing",        "Clock Tempo" } },

		{ "Performance", "Performance",
		  { "Patch Volume",   "Panorama",         "Portamento Time",  "Transpose",
		    "Unison Detune",  "Unison Pan Spread", "Bender Range Up", "Bender Range Down" } },
	};

	static constexpr uint32_t g_remoteControlsPageCount =
		static_cast<uint32_t>(std::size(g_remoteControlsPages));

} // namespace virusC
