#pragma once

// Remote controls page definitions for the Virus TI (OsTIrus).
// Parameter names must match "name" fields in parameterDescriptions_TI.json exactly.
// nullptr marks an unused slot (permitted by the CLAP spec).

#include "clap-juce-extensions/clap-juce-extensions.h"

namespace virusTI
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
		    "Osc1 Keyfollow",   "Osc1 Mode",     "Noise Volume",    "Noise Color" } },

		{ "Oscillators", "Oscillator 2",
		  { "Osc2 Wave Select", "Osc2 Shape",    "Osc2 Pulsewidth", "Osc2 Semitone",
		    "Osc2 Detune",      "Osc2 FM Amount","Osc2 Sync",       "Osc2 Filt Env Amt" } },

		// ---- Filter ---------------------------------------------------------

		{ "Filter", "Filter 1",
		  { "Cutoff",          "Filter1 Resonance", "Filter1 Env Amt",     "Filter1 Keyfollow",
		    "Filter1 Mode",    "Filter1 Env Polarity", "Filter Select",    "Filter Routing" } },

		{ "Filter", "Filter 2",
		  { "Cutoff2",         "Filter2 Resonance", "Filter2 Env Amt",     "Filter2 Keyfollow",
		    "Cutoff2 Offset",  "Filter Balance",    "Filter2 Mode",        nullptr } },

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
		    "Chorus Feedback", "Chorus Lfo Shape","Chorus/Type",    nullptr } },

		{ "Effects", "Phaser",
		  { "Phaser Mix",      "Phaser Rate",    "Phaser Depth",    "Phaser Frequency",
		    "Phaser Feedback", "Phaser Spread",  "Phaser Mode",     nullptr } },

		{ "Effects", "Reverb",
		  { "Reverb Send",    "Reverb Time",     "Reverb Type",   "Reverb Damping",
		    "Reverb Color",   "Reverb Predelay", "Reverb Feedback","Reverb Mode" } },

		{ "Effects", "Delay",
		  { "Delay Send",           "Delay Time",          "Delay Feedback",  "Delay Color",
		    "Dly Rate / Rev Decay", "Dly Depth ",          "Delay Type",      "Delay Clock" } },

		// ---- FX1 section ----------------------------------------------------

		// Context-dependent: indices 21-24 change display name with Filter Bank/Type
		// but the underlying parameter is the same — controller resolves by name.
		{ "FX1", "Filter Bank",
		  { "Filter Bank/Type",        "Filter Bank/Mix",         "Filter Bank/Frequency",
		    "Filter Bank/Resonance",   "Filter Bank/Stereo Phase","Filter Bank/Filter Type",
		    "Filter Bank/Slope",       nullptr } },

		// Patch Distortion sub-section of FX1
		{ "FX1", "Distortion",
		  { "Distortion Curve",            "Distortion Intensity",
		    "Patch Distortion/Mix",        "Patch Distortion/Treble Booster",
		    "Patch Distortion/High Cut",   "Patch Distortion/Tone127",
		    "Bass Intensity",              "Punch Intensity" } },

		// 3-band EQ: 7 params, one empty slot
		{ "FX1", "EQ",
		  { "LowEQ Gain",    "LowEQ Frequency",
		    "MidEQ Gain",    "MidEQ Frequency",  "MidEQ Q-Factor",
		    "HighEQ Gain",   "HighEQ Frequency", nullptr } },

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

} // namespace virusTI
