// CLAP preset discovery factory for OsTIrus.
// Comprehensive logging to /tmp/ostirus_discovery.log for debugging host behaviour.

#include <clap/factory/preset-discovery.h>

#include "virusLib/romloader.h"
#include "virusLib/romfile.h"
#include "virusLib/microcontrollerTypes.h"

#include "synthLib/midiToSysex.h"
#include "synthLib/romLoader.h"

#include "jucePluginLib/tools.h"

#include "baseLib/filesystem.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
static constexpr char g_logPath[] = "/tmp/ostirus_discovery.log";

struct Log
{
    std::ofstream f;
    explicit Log() : f(g_logPath, std::ios::app) {}
    template<typename T> Log& operator<<(const T& v) { f << v; return *this; }
    ~Log() { f << "\n"; }
};
#define LOG() Log()

static constexpr char g_clapIdSynth[]  = "com.theusualsuspects.Ttip";  // OsTIrus synth
static constexpr char g_clapIdFx[]     = "com.theusualsuspects.Ttif";  // OsTIrusFX
static constexpr char g_providerId[]   = "com.theusualsuspects.Ttip.preset-provider";
static constexpr char g_providerName[] = "OsTIrus Preset Provider";
static constexpr char g_vendor[]       = "The Usual Suspects";

static const clap_universal_plugin_id_t g_pluginIds[] = {
    { "clap", g_clapIdSynth },
    { "clap", g_clapIdFx    },
};

static const clap_preset_discovery_provider_descriptor_t g_descriptor =
{
    CLAP_VERSION, g_providerId, g_providerName, g_vendor
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string romBankDisplayName(const virusLib::ROMFile& _rom, uint32_t _b)
{
    char buf[64];
    const uint32_t n = _rom.getNumSingleBanks();
    if (n <= 26)
    {
        const char* lbl = (_rom.getModel() == virusLib::DeviceModel::Snow) ? "Snow ROM" : "ROM";
        snprintf(buf, sizeof(buf), "%s - Bank %c", lbl, 'A' + static_cast<int>(_b));
        return buf;
    }
    const uint32_t cTI  = virusLib::ROMFile::getRomBankCount(virusLib::DeviceModel::TI);
    const uint32_t cTI2 = virusLib::ROMFile::getRomBankCount(virusLib::DeviceModel::TI2);
    if (_b < cTI)
        snprintf(buf, sizeof(buf), "TI ROM - Bank %c",   'A' + static_cast<int>(_b));
    else if (_b < cTI + cTI2)
        snprintf(buf, sizeof(buf), "TI2 ROM - Bank %c",  'A' + static_cast<int>(_b - cTI));
    else
        snprintf(buf, sizeof(buf), "Snow ROM - Bank %c", 'A' + static_cast<int>(_b - cTI - cTI2));
    return buf;
}

static std::string romBankSoundpackId(const virusLib::ROMFile& _rom, uint32_t _b)
{
    std::string id = "ostirus-rom-";
    for (char c : romBankDisplayName(_rom, _b))
    {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u))       id += static_cast<char>(std::tolower(u));
        else if (id.back() != '-') id += '-';
    }
    while (!id.empty() && id.back() == '-') id.pop_back();
    return id;
}

static std::string fileSoundpackId(const std::string& _path)
{
    const auto base = baseLib::filesystem::stripExtension(
        baseLib::filesystem::getFilenameWithoutPath(_path));
    std::string id = "ostirus-user-";
    for (char c : base)
    {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u))       id += static_cast<char>(std::tolower(u));
        else if (id.back() != '-') id += '-';
    }
    while (!id.empty() && id.back() == '-') id.pop_back();
    return id;
}

static void emitCategories(const virusLib::ROMFile::TPreset& _p,
                            const clap_preset_discovery_metadata_receiver_t* _r)
{
    for (const auto idx : { virusLib::ROMFile::getSingleCategory1(_p),
                             virusLib::ROMFile::getSingleCategory2(_p) })
    {
        const char* name = virusLib::ROMFile::getSingleCategoryName(idx);
        if (name && idx > 0)   // skip index 0 = "--"
            _r->add_feature(_r, name);
    }
}

static bool sysexToPreset(const synthLib::SysexBuffer& _msg, virusLib::ROMFile::TPreset& _preset)
{
    constexpr uint32_t hdr = 9, ftr = 2;
    if (_msg.size() < hdr + ftr + 256) return false;
    _preset.fill(0);
    if (_msg.size() == 524) // D-preset: extra internal checksum at payload byte 256
    {
        std::copy_n(_msg.data() + hdr,       256, _preset.begin());
        std::copy_n(_msg.data() + hdr + 257, 255, _preset.begin() + 256);
    }
    else
    {
        const auto n = std::min(_preset.size(), _msg.size() - hdr - ftr);
        std::copy_n(_msg.data() + hdr, n, _preset.begin());
    }
    return true;
}

static std::vector<std::string> scanDir(const std::string& _dir,
                                         std::initializer_list<const char*> _exts)
{
    std::vector<std::string> result;
    if (_dir.empty()) return result;
    for (const char* ext : _exts)
        baseLib::filesystem::findFiles(result, _dir, ext, 0, 0);
    return result;
}

// ---------------------------------------------------------------------------
// Provider struct
// ---------------------------------------------------------------------------

struct OsTIrusPresetProvider
{
    clap_preset_discovery_provider_t       clap;    // must be first
    const clap_preset_discovery_indexer_t* indexer;
    std::vector<virusLib::ROMFile>          roms;
    std::string                             patchmanagerPath;
    std::string                             userPresetsPath;
    std::map<std::string, std::string>      fileSoundpacks;
};

// ---------------------------------------------------------------------------
// Emit presets from one bank file (used by both PLUGIN and FILE paths)
// Returns true if the host stopped iteration early.
// ---------------------------------------------------------------------------

static bool emitFilePresets(
    const std::string&                                _filePath,
    const std::string&                                _soundpackId,
    uint32_t                                          _flags,
    bool                                              _pluginLocationKey,
    const clap_preset_discovery_metadata_receiver_t*  _receiver)
{
    LOG() << "  emitFilePresets: " << _filePath << " pluginKey=" << _pluginLocationKey;
    synthLib::SysexBufferList messages;
    if (!synthLib::MidiToSysex::extractSysexFromFile(messages, _filePath))
    {
        LOG() << "    FAILED to read file";
        return false;
    }
    LOG() << "    extracted " << messages.size() << " SysEx messages";

    const auto bankName = baseLib::filesystem::stripExtension(
        baseLib::filesystem::getFilenameWithoutPath(_filePath));

    uint32_t presetIdx = 0;
    for (const auto& msg : messages)
    {
        if (msg.size() < 12) continue;
        if (msg[0] != 0xF0 || msg[1] != 0x00 || msg[2] != 0x20 || msg[3] != 0x33) continue;
        const bool isSingle = (msg[6] == 0x10);
        const bool isMulti  = (msg[6] == 0x11);
        if (!isSingle && !isMulti) continue;

        virusLib::ROMFile::TPreset preset{};
        std::string name;
        if (isSingle && sysexToPreset(msg, preset))
            name = virusLib::ROMFile::getSingleName(preset);

        const auto loadKey = _pluginLocationKey
            ? ("file:" + _filePath + ":" + std::to_string(presetIdx))
            : std::to_string(presetIdx);

        const char* displayName = name.empty() ? "(unnamed)" : name.c_str();
        const bool shouldLog = (presetIdx == 0 || presetIdx % 16 == 0);
        if (shouldLog)
            LOG() << "    preset[" << presetIdx << "] name=\"" << displayName << "\" key=\"" << loadKey << "\"";
        bool cont = _receiver->begin_preset(_receiver, displayName, loadKey.c_str());
        if (shouldLog)
            LOG() << "    begin_preset[" << presetIdx << "] -> " << (cont ? "continue" : "STOP");
        if (!cont) { LOG() << "    HOST STOPPED at presetIdx=" << presetIdx; return true; }

        for (const auto& pid : g_pluginIds) _receiver->add_plugin_id(_receiver, &pid);
        _receiver->set_flags(_receiver, _flags);
        if (!_soundpackId.empty())
            _receiver->set_soundpack_id(_receiver, _soundpackId.c_str());
        _receiver->add_extra_info(_receiver, "bank", bankName.c_str());
        if (isSingle)
            emitCategories(preset, _receiver);
        else
            _receiver->add_feature(_receiver, "Multi");

        ++presetIdx;
    }
    LOG() << "    total begin_preset calls emitted: " << presetIdx;
    return false;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

static bool providerInit(const clap_preset_discovery_provider_t* _p)
{
    auto* self = reinterpret_cast<const OsTIrusPresetProvider*>(_p);
    auto* idx  = self->indexer;

    LOG() << "=== providerInit ===";
    LOG() << "  indexer: " << idx->name << " " << (idx->vendor ? idx->vendor : "") << " " << (idx->version ? idx->version : "");
    LOG() << "  patchmanagerPath: " << self->patchmanagerPath;
    LOG() << "  userPresetsPath:  " << self->userPresetsPath;

    auto doDeclareLoc = [&](const clap_preset_discovery_location_t& loc) {
        bool ok = idx->declare_location(idx, &loc);
        LOG() << "  declare_location: kind=" << loc.kind
              << " flags=" << loc.flags
              << " name=\"" << (loc.name ? loc.name : "NULL") << "\""
              << " location=" << (loc.location ? loc.location : "NULL")
              << " -> " << (ok ? "ok" : "REJECTED");
    };

    // PLUGIN location: ROM factory presets
    { clap_preset_discovery_location_t loc{};
      loc.flags=CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT; loc.name="OsTIrus ROM Presets";
      loc.kind=CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN; loc.location=nullptr;
      doDeclareLoc(loc); }

    // File types
    const struct { const char* n; const char* d; const char* e; } fts[] = {
        { "Virus TI SysEx",      "Virus TI preset SysEx dump",       "syx"  },
        { "Virus TI MIDI SysEx", "Virus TI preset bank as MIDI file", "mid"  },
        { "Virus TI MIDI SysEx", "Virus TI preset bank as MIDI file", "midi" },
    };
    for (const auto& ft : fts)
    {
        clap_preset_discovery_filetype_t f{};
        f.name = ft.n; f.description = ft.d; f.file_extension = ft.e;
        bool ok = idx->declare_filetype(idx, &f);
        LOG() << "  declare_filetype: ext=\"" << ft.e << "\" -> " << (ok ? "ok" : "REJECTED");
    }

    // FILE locations
    auto declareFileLoc = [&](const std::string& path, const char* name, uint32_t flags)
    {
        if (path.empty()) { LOG() << "  declare_location FILE \"" << name << "\": SKIPPED (empty path)"; return; }
        clap_preset_discovery_location_t loc{};
        loc.flags=flags; loc.name=name;
        loc.kind=CLAP_PRESET_DISCOVERY_LOCATION_FILE; loc.location=path.c_str();
        doDeclareLoc(loc);
    };
    declareFileLoc(self->patchmanagerPath, "OsTIrus User Banks",   CLAP_PRESET_DISCOVERY_IS_USER_CONTENT);
    declareFileLoc(self->userPresetsPath,  "OsTIrus User Presets", CLAP_PRESET_DISCOVERY_IS_USER_CONTENT);

    // Soundpacks for ROM banks
    uint32_t romSpCount = 0;
    for (const auto& rom : self->roms)
    {
        if (!rom.isValid()) continue;
        for (uint32_t b = 0; b < rom.getNumSingleBanks(); ++b)
        {
            const auto name = romBankDisplayName(rom, b);
            const auto id   = romBankSoundpackId(rom, b);
            clap_preset_discovery_soundpack_t sp{};
            sp.flags=CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT;
            sp.id=id.c_str(); sp.name=name.c_str(); sp.vendor=g_vendor;
            bool ok = idx->declare_soundpack(idx, &sp);
            ++romSpCount;
            if (b == 0 || b == rom.getNumSingleBanks()-1)
                LOG() << "  declare_soundpack ROM[" << b << "] \"" << name << "\" -> " << (ok ? "ok" : "REJECTED");
        }
    }
    LOG() << "  ROM soundpacks declared: " << romSpCount;

    // Soundpacks for user bank files
    auto* ms = const_cast<OsTIrusPresetProvider*>(self);
    auto declareFileSoundpacks = [&](const std::string& dir)
    {
        auto files = scanDir(dir, { ".syx", ".mid", ".midi" });
        LOG() << "  scanning \"" << dir << "\": " << files.size() << " files";
        for (const auto& file : files)
        {
            const auto id   = fileSoundpackId(file);
            const auto name = baseLib::filesystem::stripExtension(
                baseLib::filesystem::getFilenameWithoutPath(file));
            ms->fileSoundpacks[file] = id;
            clap_preset_discovery_soundpack_t sp{};
            sp.flags=CLAP_PRESET_DISCOVERY_IS_USER_CONTENT;
            sp.id=id.c_str(); sp.name=name.c_str(); sp.vendor=g_vendor;
            bool ok = idx->declare_soundpack(idx, &sp);
            LOG() << "    soundpack \"" << name << "\" id=\"" << id << "\" -> " << (ok ? "ok" : "REJECTED");
        }
    };
    declareFileSoundpacks(self->patchmanagerPath);
    declareFileSoundpacks(self->userPresetsPath);

    LOG() << "=== providerInit done ===";
    return true;
}

static void providerDestroy(const clap_preset_discovery_provider_t* _p)
{
    delete reinterpret_cast<const OsTIrusPresetProvider*>(_p);
}

// ---------------------------------------------------------------------------
// get_metadata
// ---------------------------------------------------------------------------

static bool providerGetMetadata(
    const clap_preset_discovery_provider_t*          _p,
    uint32_t                                          _locationKind,
    const char*                                       _location,
    const clap_preset_discovery_metadata_receiver_t* _receiver)
{
    const auto* self = reinterpret_cast<const OsTIrusPresetProvider*>(_p);

    LOG() << "=== get_metadata kind=" << _locationKind
          << " location=" << (_location ? _location : "NULL") << " ===";

    // ---- PLUGIN location (kind=1) ----
    if (_locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN)
    {
        uint32_t emitted = 0;
        // 1. ROM singles
        for (const auto& rom : self->roms)
        {
            if (!rom.isValid()) continue;
            for (uint32_t b = 0; b < rom.getNumSingleBanks(); ++b)
            {
                const uint32_t arrayIdx = 2u + b;
                const auto spId     = romBankSoundpackId(rom, b);
                const auto bankName = romBankDisplayName(rom, b);

                for (uint32_t p = 0; p < rom.getPresetsPerBank(); ++p)
                {
                    virusLib::ROMFile::TPreset preset{};
                    if (!rom.getSingle(static_cast<int>(b), static_cast<int>(p), preset)) continue;
                    const auto name = virusLib::ROMFile::getSingleName(preset);
                    if (name.empty()) continue;

                    const auto key = "rom:" + std::to_string(arrayIdx) + ":" + std::to_string(p);
                    if (p == 0) LOG() << "  ROM bank \"" << bankName << "\" arrayIdx=" << arrayIdx;
                    bool cont = _receiver->begin_preset(_receiver, name.c_str(), key.c_str());
                    if (p == 0) LOG() << "    begin_preset[0] name=\"" << name << "\" key=\"" << key << "\" -> " << (cont ? "continue" : "STOP");
                    if (!cont) { LOG() << "  HOST STOPPED at ROM bank=" << b << " prog=" << p; return true; }
                    for (const auto& pid : g_pluginIds) _receiver->add_plugin_id(_receiver, &pid);
                    _receiver->set_flags(_receiver, CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT);
                    _receiver->set_soundpack_id(_receiver, spId.c_str());
                    _receiver->add_extra_info(_receiver, "bank", bankName.c_str());
                    emitCategories(preset, _receiver);
                    ++emitted;
                }
            }
        }

        // 2. User bank presets — scan directories and emit individual presets.
        //    Load-key encodes the file path so presetLoadFromLocation can find the preset.
        auto emitDir = [&](const std::string& dir) -> bool
        {
            for (const auto& file : scanDir(dir, { ".syx", ".mid", ".midi" }))
            {
                const auto spIt = self->fileSoundpacks.find(file);
                const std::string spId = (spIt != self->fileSoundpacks.end())
                    ? spIt->second : std::string();

                if (emitFilePresets(file, spId,
                        CLAP_PRESET_DISCOVERY_IS_USER_CONTENT, true, _receiver))
                    return true; // host stopped
            }
            return false;
        };

        LOG() << "  PLUGIN user dirs scan: patchmanager=" << self->patchmanagerPath;
        if (emitDir(self->patchmanagerPath)) return true;
        LOG() << "  PLUGIN user dirs scan: userPresets=" << self->userPresetsPath;
        if (emitDir(self->userPresetsPath))  return true;
        LOG() << "  PLUGIN done, total emitted=" << emitted;
        return true;
    }

    // ---- FILE location (kind=0) — Bitwig crawls these per-file ----
    if (_locationKind == CLAP_PRESET_DISCOVERY_LOCATION_FILE && _location)
    {
        const std::string filePath(_location);
        const auto spIt = self->fileSoundpacks.find(filePath);
        const std::string spId = (spIt != self->fileSoundpacks.end())
            ? spIt->second : std::string();
        LOG() << "  FILE soundpackId=\"" << spId << "\"";
        emitFilePresets(filePath, spId,
            CLAP_PRESET_DISCOVERY_IS_USER_CONTENT, false, _receiver);
        LOG() << "  FILE done";
        return true;
    }

    LOG() << "  UNHANDLED kind=" << _locationKind;
    return false;
}

static const void* providerGetExtension(const clap_preset_discovery_provider_t*, const char*)
{
    return nullptr;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

static uint32_t factoryCount(const clap_preset_discovery_factory_t*) { return 1; }

static const clap_preset_discovery_provider_descriptor_t* factoryGetDescriptor(
    const clap_preset_discovery_factory_t*, uint32_t _i)
{
    return _i == 0 ? &g_descriptor : nullptr;
}

static const clap_preset_discovery_provider_t* factoryCreate(
    const clap_preset_discovery_factory_t*,
    const clap_preset_discovery_indexer_t* _indexer,
    const char*                             _providerId)
{
    if (strcmp(_providerId, g_providerId) != 0) return nullptr;

    auto* p = new OsTIrusPresetProvider();
    p->clap.desc          = &g_descriptor;
    p->clap.provider_data = p;
    p->clap.init          = providerInit;
    p->clap.destroy       = providerDestroy;
    p->clap.get_metadata  = providerGetMetadata;
    p->clap.get_extension = providerGetExtension;
    p->indexer = _indexer;

    const auto publicData = pluginLib::Tools::getPublicDataFolder("The Usual Suspects", "OsTIrus");
    synthLib::RomLoader::addSearchPath(publicData + "roms/");
    {
        auto allRoms = virusLib::ROMLoader::findROMs(virusLib::DeviceModel::TI2, virusLib::DeviceModel::Snow);
        // Deduplicate by MD5 hash — Snow is TI-family so findROMs(Snow) re-matches
        // the same large .bin files, producing duplicates.
        std::set<std::string> seen;
        for (auto& rom : allRoms)
        {
            if (!rom.isValid()) continue;
            if (seen.insert(rom.getHash().toString()).second)
                p->roms.push_back(std::move(rom));
        }
    }

    p->patchmanagerPath = publicData + "patchmanager/";
    const auto appData = baseLib::filesystem::getSpecialFolderPath(
        baseLib::filesystem::SpecialFolderType::PrivateAppData);
    if (!appData.empty())
        p->userPresetsPath = appData + "DSP56300Emulator_OsTIrus/presets/";

    LOG() << "=== factoryCreate providerId=" << _providerId << " ===";
    LOG() << "  indexer: " << _indexer->name << " v=" << (_indexer->version ? _indexer->version : "?")
          << " vendor=" << (_indexer->vendor ? _indexer->vendor : "?");
    LOG() << "  publicData: " << publicData;
    LOG() << "  patchmanagerPath: " << p->patchmanagerPath;
    LOG() << "  userPresetsPath:  " << p->userPresetsPath;
    LOG() << "  roms found: " << p->roms.size();
    for (size_t i = 0; i < p->roms.size(); ++i)
    {
        const auto& rom = p->roms[i];
        LOG() << "    rom[" << i << "] valid=" << rom.isValid()
              << " banks=" << (rom.isValid() ? rom.getNumSingleBanks() : 0)
              << " presetsPerBank=" << (rom.isValid() ? rom.getPresetsPerBank() : 0)
              << " file=" << rom.getFilename();
    }

    return &p->clap;
}

static const clap_preset_discovery_factory_t g_presetDiscoveryFactory =
{
    factoryCount, factoryGetDescriptor, factoryCreate
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

const void* clapJuceExtensionCustomFactory(const char* _factoryId)
{
    LOG() << "clapJuceExtensionCustomFactory: factoryId=" << (_factoryId ? _factoryId : "NULL");
    if (strcmp(_factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID) == 0 ||
        strcmp(_factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID_COMPAT) == 0)
    {
        LOG() << "  -> returning preset-discovery factory";
        return static_cast<const void*>(&g_presetDiscoveryFactory);
    }
    LOG() << "  -> no match, returning nullptr";
    return nullptr;
}
