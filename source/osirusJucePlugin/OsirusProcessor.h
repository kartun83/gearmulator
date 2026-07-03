#pragma once

#include "virusJucePlugin/VirusProcessor.h"

#include "baseLib/event.h"

#include "clap-juce-extensions/clap-juce-extensions.h"

#include <string>
#include <unordered_map>

class OsirusProcessor : public virus::VirusProcessor,
                         public clap_juce_extensions::clap_juce_audio_processor_capabilities,
                         public juce::AudioProcessorListener
{
public:
    OsirusProcessor();
    ~OsirusProcessor() override;

    jucePluginEditorLib::PluginEditorState* createEditorState() override;

	// CLAP_EXT_REMOTE_CONTROLS
	bool supportsRemoteControls() const noexcept override { return true; }
	uint32_t remoteControlsPageCount() noexcept override;
	bool remoteControlsPageFill(uint32_t _pageIndex, juce::String& _sectionName,
	                            uint32_t& _pageID, juce::String& _pageName,
	                            std::array<juce::AudioProcessorParameter*, CLAP_REMOTE_CONTROLS_COUNT>& _params) noexcept override;

	// juce::AudioProcessorListener — used to detect parameter gesture begin
	// and suggest the corresponding remote controls page to the host.
	void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
	void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}
	void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int _parameterIndex) override;

private:
	// Part whose parameters the current remote control pages expose.
	// Updated by the onCurrentPartChanged event; triggers a page refresh.
	uint8_t                          m_remoteControlsPart = 0;
	baseLib::EventListener<uint8_t>  m_partChangedListener;

	// Reverse lookup built at startup: parameter name → page index.
	// Used by audioProcessorParameterChangeGestureBegin to suggest pages.
	std::unordered_map<std::string, uint32_t> m_paramNameToPage;
};
