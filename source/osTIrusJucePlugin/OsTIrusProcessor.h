#pragma once

#include "virusJucePlugin/VirusProcessor.h"

#include "clap-juce-extensions/clap-juce-extensions.h"

class OsTIrusProcessor
	: public virus::VirusProcessor
	, public clap_juce_extensions::clap_juce_audio_processor_capabilities
{
public:
    OsTIrusProcessor();
    ~OsTIrusProcessor() override;

	jucePluginEditorLib::PluginEditorState* createEditorState() override;

	// CLAP_EXT_PRESET_LOAD
	bool supportsPresetLoad() const noexcept override { return true; }
	bool presetLoadFromLocation(uint32_t _locationKind, const char* _location, const char* _loadKey) noexcept override;
};
