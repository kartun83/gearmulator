#pragma once

#include "virusJucePlugin/VirusProcessor.h"

#include "baseLib/event.h"

#include "clap-juce-extensions/clap-juce-extensions.h"

class OsTIrusProcessor : public virus::VirusProcessor,
                          public clap_juce_extensions::clap_juce_audio_processor_capabilities
{
public:
    OsTIrusProcessor();
    ~OsTIrusProcessor() override;

	jucePluginEditorLib::PluginEditorState* createEditorState() override;

	// CLAP_EXT_REMOTE_CONTROLS
	uint32_t remoteControlsPageCount() noexcept override;
	bool remoteControlsPageFill(uint32_t _pageIndex, juce::String& _sectionName,
	                            uint32_t& _pageID, juce::String& _pageName,
	                            std::array<juce::AudioProcessorParameter*, CLAP_REMOTE_CONTROLS_COUNT>& _params) noexcept override;

private:
	// Part whose parameters the current remote control pages expose.
	// Updated by the onCurrentPartChanged event; triggers a page refresh.
	uint8_t                          m_remoteControlsPart = 0;
	baseLib::EventListener<uint8_t>  m_partChangedListener;
};
