#include "OsTIrusProcessor.h"
#include "OsTIrusEditorState.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "BinaryData.h"
#include "jucePluginLib/processorPropertiesInit.h"

#include "virusLib/romloader.h"

#include "jucePluginLib/controller.h"

namespace
{
	juce::PropertiesFile::Options getConfigOptions()
	{
		juce::PropertiesFile::Options opts;
		opts.applicationName = "DSP56300Emulator_OsTIrus";
		opts.filenameSuffix = ".settings";
		opts.folderName = "DSP56300Emulator_OsTIrus";
		opts.osxLibrarySubFolder = "Application Support/DSP56300Emulator_OsTIrus";
		return opts;
	}
}

// ---------------------------------------------------------------------------
// CLAP_EXT_REMOTE_CONTROLS — page table
// ---------------------------------------------------------------------------
namespace
{
	struct RemoteControlsPage
	{
		const char* sectionName;
		const char* pageName;
		// Exact "name" values from parameterDescriptions_TI.json; nullptr = empty slot.
		const char* params[CLAP_REMOTE_CONTROLS_COUNT];
	};

	static constexpr RemoteControlsPage g_rcPages[] =
	{
		{ "Oscillators", "Oscillators",
		  { "Osc1 Wave Select", "Osc1 Pulsewidth", "Osc1 Semitone",
		    "Osc2 Wave Select", "Osc2 Pulsewidth", "Osc2 Detune", "Osc2 Semitone",
		    "Osc2 FM Amount" } },

		{ "Filter", "Filter",
		  { "Cutoff", "Filter1 Resonance",
		    "Cutoff2", "Filter2 Resonance",
		    "Filter1 Env Amt", "Filter2 Env Amt",
		    "Filter Balance", "Filter1 Keyfollow" } },

		{ "Filter Envelope", "Filter Envelope",
		  { "Filter Env Attack", "Filter Env Decay",
		    "Filter Env Sustain", "Filter Env Sustain Time", "Filter Env Release",
		    "Osc2 Filt Env Amt", "Filter1 Env Polarity", nullptr } },

		{ "Amplifier", "Amplifier",
		  { "Amp Env Attack", "Amp Env Decay",
		    "Amp Env Sustain", "Amp Env Sustain Time", "Amp Env Release",
		    "Patch Volume", "Panorama", nullptr } },

		{ "LFO", "LFO 1",
		  { "Lfo1 Rate", "Lfo1 Shape", "Lfo1 Symmetry",
		    "Osc1 Lfo1 Amount", "Osc2 Lfo1 Amount",
		    "PW Lfo1 Amount", "Reso Lfo1 Amount", "Lfo1 Mode" } },

		{ "LFO", "LFO 2",
		  { "Lfo2 Rate", "Lfo2 Shape", "Lfo2 Symmetry",
		    "FM Lfo2 Amount", "Cutoff1 Lfo2 Amount", "Cutoff2 Lfo2 Amount",
		    "Pan Lfo2 Amount", "Lfo2 Mode" } },

		{ "Effects", "Reverb",
		  { "Reverb Send", "Reverb Time", "Reverb Type",
		    "Reverb Damping", "Reverb Color", "Reverb Predelay",
		    "Reverb Feedback", "Reverb Mode" } },

		{ "Effects", "Delay",
		  { "Delay Send", "Delay Time", "Delay Feedback",
		    "Delay Color", "Dly Rate / Rev Decay", "Dly Depth ",
		    "Delay Type", "Delay Clock" } },

		{ "Arpeggiator", "Arpeggiator",
		  { "Arp Mode", "Arp Pattern Selct", "Arp Clock",
		    "Arp Note Length", "Arp Octave Range", "Arp Hold Enable",
		    "Arp Swing", "Clock Tempo" } },

		{ "Performance", "Performance",
		  { "Patch Volume", "Panorama", "Portamento Time", "Transpose",
		    "Unison Detune", "Unison Pan Spread",
		    "Bender Range Up", "Bender Range Down" } },
	};

	static constexpr uint32_t g_rcPageCount = static_cast<uint32_t>(std::size(g_rcPages));
}

//==============================================================================
OsTIrusProcessor::OsTIrusProcessor() :
    VirusProcessor(BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#if JucePlugin_IsSynth
                   .withOutput("Out 2", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Out 3", juce::AudioChannelSet::stereo(), true)
                   .withOutput("USB 1", juce::AudioChannelSet::stereo(), true)
                   .withOutput("USB 2", juce::AudioChannelSet::stereo(), true)
                   .withOutput("USB 3", juce::AudioChannelSet::stereo(), true)
#endif
	, ::getConfigOptions(), pluginLib::initProcessorProperties()
	, virusLib::DeviceModel::TI2)
{
	postConstruct(virusLib::ROMLoader::findROMs(virusLib::DeviceModel::TI2, virusLib::DeviceModel::Snow));

	// Subscribe to part-selection changes so remote controls pages stay in sync.
	// postConstruct() guarantees the controller exists at this point.
	m_partChangedListener.set(getController().onCurrentPartChanged, [this](const uint8_t _part)
	{
		m_remoteControlsPart = _part;
		remoteControlsChanged();
	});
}

OsTIrusProcessor::~OsTIrusProcessor()
{
	destroyEditorState();
}

uint32_t OsTIrusProcessor::remoteControlsPageCount() noexcept
{
	return g_rcPageCount;
}

bool OsTIrusProcessor::remoteControlsPageFill(
	const uint32_t _pageIndex,
	juce::String& _sectionName,
	uint32_t& _pageID,
	juce::String& _pageName,
	std::array<juce::AudioProcessorParameter*, CLAP_REMOTE_CONTROLS_COUNT>& _params) noexcept
{
	if(_pageIndex >= g_rcPageCount)
		return false;

	const auto& page = g_rcPages[_pageIndex];
	const auto* ctrl = getControllerConst();

	_sectionName = page.sectionName;
	_pageName    = page.pageName;
	// Encode page index + part into the ID so the host can distinguish sets
	// across part changes while keeping page indices stable within a set.
	_pageID = (_pageIndex << 8) | m_remoteControlsPart;

	for(uint32_t i = 0; i < CLAP_REMOTE_CONTROLS_COUNT; ++i)
	{
		_params[i] = nullptr;
		if(!ctrl || !page.params[i])
			continue;
		_params[i] = ctrl->getParameter(page.params[i], m_remoteControlsPart);
	}

	return true;
}

jucePluginEditorLib::PluginEditorState* OsTIrusProcessor::createEditorState()
{
	return new OsTIrusEditorState(*this, getController());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OsTIrusProcessor();
}
