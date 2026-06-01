#include "OsTIrusProcessor.h"
#include "OsTIrusEditorState.h"
#include "VirusTIRemoteControls.h"

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

// Page table lives in VirusTIRemoteControls.h

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
	return virusTI::g_remoteControlsPageCount;
}

bool OsTIrusProcessor::remoteControlsPageFill(
	const uint32_t _pageIndex,
	juce::String& _sectionName,
	uint32_t& _pageID,
	juce::String& _pageName,
	std::array<juce::AudioProcessorParameter*, CLAP_REMOTE_CONTROLS_COUNT>& _params) noexcept
{
	if(_pageIndex >= virusTI::g_remoteControlsPageCount)
		return false;

	const auto& page = virusTI::g_remoteControlsPages[_pageIndex];
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
