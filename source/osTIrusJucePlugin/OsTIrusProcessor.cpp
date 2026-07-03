#include "OsTIrusProcessor.h"
#include "OsTIrusEditorState.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "BinaryData.h"
#include "jucePluginLib/processorPropertiesInit.h"

#include "virusLib/romloader.h"
#include "virusLib/microcontrollerTypes.h"
#include "virusJucePlugin/VirusController.h"

#include "synthLib/midiToSysex.h"

#include <clap/ext/preset-load.h>

#include <string>

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
}

OsTIrusProcessor::~OsTIrusProcessor()
{
	destroyEditorState();
}

jucePluginEditorLib::PluginEditorState* OsTIrusProcessor::createEditorState()
{
	return new OsTIrusEditorState(*this, getController());
}

// Safe integer parse — returns false on any parse failure without throwing.
static bool parseInt(const char* _str, int& _out) noexcept
{
	if (!_str || !*_str)
		return false;
	char* end = nullptr;
	const long v = std::strtol(_str, &end, 10);
	if (end == _str || *end != '\0')
		return false;
	_out = static_cast<int>(v);
	return true;
}

bool OsTIrusProcessor::presetLoadFromLocation(const uint32_t _locationKind, const char* _location, const char* _loadKey) noexcept
{
	if (!_loadKey)
		return false;

	auto& ctrl = static_cast<virus::Controller&>(getController());

	if (_locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN)
	{
		const std::string key(_loadKey);

		if (key.substr(0, 4) == "rom:")
		{
			// key = "rom:<m_singles_arrayIdx>:<prog>"
			const auto sep = key.find(':', 4);
			if (sep == std::string::npos)
				return false;

			int arrayIdx = 0, prog = 0;
			const std::string idxPart = key.substr(4, sep - 4);
			if (!parseInt(idxPart.c_str(), arrayIdx) || arrayIdx < 0 || arrayIdx > 255)
				return false;
			if (!parseInt(key.c_str() + sep + 1, prog) || prog < 0 || prog > 127)
				return false;

			const auto bankNum = virusLib::fromArrayIndex(static_cast<uint8_t>(arrayIdx));
			const auto part    = static_cast<uint8_t>(ctrl.isMultiMode() ? ctrl.getCurrentPart() : 0);
			ctrl.setCurrentPartPreset(part, bankNum, static_cast<uint8_t>(prog));
			return true;
		}

		if (key.substr(0, 5) == "file:")
		{
			// key = "file:<absolute_path>:<presetIdx>"
			// Split on the LAST ':' to separate path from index.
			const auto lastColon = key.rfind(':');
			if (lastColon <= 5)
				return false;

			int idx = 0;
			if (!parseInt(key.c_str() + lastColon + 1, idx) || idx < 0)
				return false;

			const std::string filePath = key.substr(5, lastColon - 5);

			synthLib::SysexBufferList messages;
			if (!synthLib::MidiToSysex::extractSysexFromFile(messages, filePath))
				return false;

			int presetIdx = 0;
			for (const auto& msg : messages)
			{
				if (msg.size() < 12) continue;
				if (msg[0] != 0xF0 || msg[1] != 0x00 || msg[2] != 0x20 || msg[3] != 0x33) continue;
				if (msg[6] != 0x10) continue; // DUMP_SINGLE only

				if (presetIdx == idx)
					return ctrl.activatePatch(msg);
				++presetIdx;
			}
			return false;
		}

		return false;
	}

	if (_locationKind == CLAP_PRESET_DISCOVERY_LOCATION_FILE && _location)
	{
		// key = "<presetIdx>" or "" (empty = first preset)
		int idx = 0;
		if (*_loadKey != '\0' && (!parseInt(_loadKey, idx) || idx < 0))
			return false;

		synthLib::SysexBufferList messages;
		if (!synthLib::MidiToSysex::extractSysexFromFile(messages, _location))
			return false;

		int presetIdx = 0;
		for (const auto& msg : messages)
		{
			if (msg.size() < 12) continue;
			if (msg[0] != 0xF0 || msg[1] != 0x00 || msg[2] != 0x20 || msg[3] != 0x33) continue;
			if (msg[6] != 0x10) continue; // DUMP_SINGLE

			if (presetIdx == idx)
				return ctrl.activatePatch(msg);
			++presetIdx;
		}
		return false;
	}

	return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OsTIrusProcessor();
}
