#include "OsirusProcessor.h"
#include "OsirusEditorState.h"
#include "VirusRemoteControls.h"

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
		opts.applicationName = "DSP56300 Emulator";
		opts.filenameSuffix = ".settings";
		opts.folderName = "DSP56300 Emulator";
		opts.osxLibrarySubFolder = "Application Support/DSP56300 Emulator";
		return opts;
	}
}

//==============================================================================
OsirusProcessor::OsirusProcessor() :
    VirusProcessor(BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#if JucePlugin_IsSynth
                   .withOutput("Out 2", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Out 3", juce::AudioChannelSet::stereo(), true)
#endif
	, ::getConfigOptions(), pluginLib::initProcessorProperties()
	, virusLib::DeviceModel::ABC)
{
	postConstruct(virusLib::ROMLoader::findROMs(virusLib::DeviceModel::ABC));

	// Subscribe to part-selection changes so remote controls pages stay in sync.
	// postConstruct() guarantees the controller exists at this point.
	m_partChangedListener.set(getController().onCurrentPartChanged, [this](const uint8_t _part)
	{
		m_remoteControlsPart = _part;
		remoteControlsChanged();
	});

	// Build reverse lookup: parameter name → page index.
	// Used to suggest the right remote controls page when the user touches a control.
	for(uint32_t pageIdx = 0; pageIdx < virusC::g_remoteControlsPageCount; ++pageIdx)
	{
		const auto& page = virusC::g_remoteControlsPages[pageIdx];
		for(const auto* name : page.params)
		{
			if(name)
				m_paramNameToPage.emplace(name, pageIdx);
		}
	}

	addListener(this);
}

OsirusProcessor::~OsirusProcessor()
{
	removeListener(this);
	destroyEditorState();
}

void OsirusProcessor::audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, const int _parameterIndex)
{
	const auto& params = getParameters();
	if(_parameterIndex < 0 || _parameterIndex >= static_cast<int>(params.size()))
		return;

	const auto name = params[_parameterIndex]->getName(255).toStdString();
	const auto it   = m_paramNameToPage.find(name);
	if(it == m_paramNameToPage.end())
		return;

	suggestRemoteControlsPage((it->second << 8) | m_remoteControlsPart);
}

uint32_t OsirusProcessor::remoteControlsPageCount() noexcept
{
	return virusC::g_remoteControlsPageCount;
}

bool OsirusProcessor::remoteControlsPageFill(
	const uint32_t _pageIndex,
	juce::String& _sectionName,
	uint32_t& _pageID,
	juce::String& _pageName,
	std::array<juce::AudioProcessorParameter*, CLAP_REMOTE_CONTROLS_COUNT>& _params) noexcept
{
	if(_pageIndex >= virusC::g_remoteControlsPageCount)
		return false;

	const auto& page = virusC::g_remoteControlsPages[_pageIndex];
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

jucePluginEditorLib::PluginEditorState* OsirusProcessor::createEditorState()
{
	return new OsirusEditorState(*this, getController());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OsirusProcessor();
}
