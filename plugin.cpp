#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef DLLEXPORT
    #define DLLEXPORT __declspec(dllexport)
#endif

#ifndef SKSEAPI
    #define SKSEAPI __cdecl
#endif

namespace GlowBeGone {
    struct Settings {
        bool removeActorFX{true};
        bool removeWeaponFX{true};
        bool removeOnlyListed{false};
        std::unordered_set<std::string> exclusionList;
        std::vector<std::pair<std::string, std::uint32_t>> magicEffectExclusionSpecs;
        std::unordered_set<RE::FormID> magicEffectExcludedRuntime;
        std::unordered_set<std::string> removalList;
        std::vector<std::pair<std::string, std::uint32_t>> magicEffectRemovalSpecs;
        std::unordered_set<RE::FormID> magicEffectRemovalRuntime;
    };

    static Settings g_settings;

    static std::string Trim(std::string s) {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) {
            s.erase(s.begin());
        }
        while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
        return s;
    }

    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::string NormalizePluginName(std::string s) {
        s = Trim(std::move(s));
        auto slash = s.find_last_of("\\/");
        if (slash != std::string::npos) {
            s = s.substr(slash + 1);
        }
        return ToLower(std::move(s));
    }

    static bool ParseBool(std::string_view v, bool fallback) {
        auto s = ToLower(Trim(std::string(v)));
        if (s == "true") {
            return true;
        }
        if (s == "false") {
            return false;
        }
        return fallback;
    }

    static std::vector<std::string> ExtractQuotedStrings(const std::string& s) {
        std::vector<std::string> out;
        bool in_quote = false;
        char quote = 0;
        std::string cur;

        for (std::size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (!in_quote) {
                if (c == '"' || c == '\'') {
                    in_quote = true;
                    quote = c;
                    cur.clear();
                }
            } else {
                if (c == quote) {
                    in_quote = false;
                    auto t = Trim(cur);
                    if (!t.empty()) {
                        out.push_back(t);
                    }
                    cur.clear();
                } else {
                    cur.push_back(c);
                }
            }
        }

        return out;
    }

    static bool ParseFileFormPair(const std::string& s, std::string& outFile, std::uint32_t& outLocalFormID) {
        auto t = Trim(s);
        if (t.empty()) {
            return false;
        }

        auto pos = t.find(':');
        if (pos == std::string::npos) {
            pos = t.find('|');
        }
        if (pos == std::string::npos) {
            return false;
        }

        auto file = Trim(t.substr(0, pos));
        auto id = Trim(t.substr(pos + 1));
        if (file.empty() || id.empty()) {
            return false;
        }

        if (id.rfind("0x", 0) == 0 || id.rfind("0X", 0) == 0) {
            id = id.substr(2);
        }

        std::uint32_t val = 0;
        try {
            val = static_cast<std::uint32_t>(std::stoul(id, nullptr, 16));
        } catch (...) {
            return false;
        }

        outFile = file;
        outLocalFormID = val;
        return true;
    }

    struct ArrayTarget {
        std::unordered_set<std::string>* plugins{nullptr};
        std::vector<std::pair<std::string, std::uint32_t>>* specs{nullptr};
    };

    static ArrayTarget ResolveArrayTarget(const std::string& key) {
        if (key == "exclusionlist") {
            return {&g_settings.exclusionList, nullptr};
        }
        if (key == "removallist") {
            return {&g_settings.removalList, nullptr};
        }
        if (key == "magiceffectexclusionlist") {
            return {nullptr, &g_settings.magicEffectExclusionSpecs};
        }
        if (key == "magiceffectremovallist") {
            return {nullptr, &g_settings.magicEffectRemovalSpecs};
        }
        return {};
    }

    static bool IsArrayKey(const std::string& key) {
        auto target = ResolveArrayTarget(key);
        return target.plugins != nullptr || target.specs != nullptr;
    }

    static void ApplyStringArray(const std::string& key, const std::string& buf) {
        auto target = ResolveArrayTarget(key);

        auto items = ExtractQuotedStrings(buf);

        if (target.plugins) {
            target.plugins->clear();
            for (auto& it : items) {
                target.plugins->insert(NormalizePluginName(it));
            }
            return;
        }

        if (!target.specs) {
            return;
        }

        target.specs->clear();
        for (auto& it : items) {
            std::string file;
            std::uint32_t localID = 0;
            if (ParseFileFormPair(it, file, localID)) {
                target.specs->emplace_back(file, localID);
            }
        }
    }

    static void LoadConfigFromFile(const char* path) {
        std::ifstream in(path);
        if (!in.is_open()) {
            return;
        }

        bool parsingStringArray = false;
        std::string arrayKey;
        std::string arrayBuf;

        std::string line;
        while (std::getline(in, line)) {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

            auto pos_hash = line.find('#');
            if (pos_hash != std::string::npos) {
                line = line.substr(0, pos_hash);
            }
            auto pos_slash = line.find("//");
            if (pos_slash != std::string::npos) {
                line = line.substr(0, pos_slash);
            }

            line = Trim(line);
            if (line.empty()) {
                continue;
            }

            if (!parsingStringArray) {
                if (line.front() == '[' && line.back() == ']') {
                    continue;
                }
            }

            if (parsingStringArray) {
                arrayBuf.push_back(' ');
                arrayBuf += line;

                if (line.find(']') != std::string::npos) {
                    ApplyStringArray(arrayKey, arrayBuf);

                    parsingStringArray = false;
                    arrayKey.clear();
                    arrayBuf.clear();
                }
                continue;
            }

            auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }

            auto key = ToLower(Trim(line.substr(0, eq)));
            auto val = Trim(line.substr(eq + 1));

            if (key == "removeactorfx") {
                g_settings.removeActorFX = ParseBool(val, g_settings.removeActorFX);
                continue;
            }

            if (key == "removeweaponfx") {
                g_settings.removeWeaponFX = ParseBool(val, g_settings.removeWeaponFX);
                continue;
            }

            if (key == "removeonlylisted") {
                g_settings.removeOnlyListed = ParseBool(val, g_settings.removeOnlyListed);
                continue;
            }

            if (IsArrayKey(key)) {
                arrayKey = key;
                arrayBuf = val;

                if (val.find('[') == std::string::npos) {
                    parsingStringArray = true;
                    continue;
                }

                if (val.find(']') != std::string::npos) {
                    ApplyStringArray(arrayKey, arrayBuf);

                    arrayKey.clear();
                    arrayBuf.clear();
                    parsingStringArray = false;
                } else {
                    parsingStringArray = true;
                }

                continue;
            }
        }
    }

    static void LoadConfig() {
        g_settings = Settings{};
        LoadConfigFromFile("Data/SKSE/Plugins/GlowBeGoneSSE.toml");
    }

    static void CollectFormFiles(RE::TESForm* form, std::vector<const RE::TESFile*>& out) {
        out.clear();
        if (!form) {
            return;
        }

        auto* array = form->sourceFiles.array;
        if (!array) {
            return;
        }

        out.reserve(array->size());
        for (auto* f : *array) {
            if (f && std::find(out.begin(), out.end(), f) == out.end()) {
                out.push_back(f);
            }
        }
    }

    static bool IsFormFromListedPlugin(RE::TESForm* form, const std::unordered_set<std::string>& plugins) {
        if (!form || plugins.empty()) {
            return false;
        }

        std::vector<const RE::TESFile*> files;
        CollectFormFiles(form, files);

        for (auto* f : files) {
            if (!f) {
                continue;
            }

            auto name = NormalizePluginName(std::string(f->GetFilename()));

            if (plugins.find(name) != plugins.end()) {
                return true;
            }
        }

        return false;
    }

    static bool IsExcludedByPlugin(RE::TESForm* form) {
        return IsFormFromListedPlugin(form, g_settings.exclusionList);
    }

    static bool IsConjuration(RE::EffectSetting* eff) {
        if (!eff) {
            return false;
        }
        return eff->data.associatedSkill == RE::ActorValue::kConjuration;
    }

    static bool IsProtectedMagicEffect(RE::EffectSetting* eff) {
        if (!eff) {
            return true;
        }

        if (IsConjuration(eff)) {
            return true;
        }

        using A = RE::EffectSetting::Archetype;
        const auto a = eff->GetArchetype();

        switch (a) {
            case A::kBoundWeapon:
            case A::kSummonCreature:
            case A::kCommandSummoned:
            case A::kReanimate:
            case A::kSpawnScriptedRef:
            case A::kEtherealize:
            case A::kInvisibility:
            case A::kDetectLife:
                return true;
            default:
                break;
        }

        auto* assoc = eff->data.associatedForm;
        if (assoc && (assoc->Is(RE::FormType::NPC) || assoc->Is(RE::FormType::LeveledNPC))) {
            return true;
        }

        return false;
    }

    static void ResolveMagicEffectSpecs(const std::vector<std::pair<std::string, std::uint32_t>>& specs,
                                        std::unordered_set<RE::FormID>& out) {
        out.clear();

        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            return;
        }

        for (auto& [file, rawID] : specs) {
            auto* mod = dh->LookupModByName(file);
            if (!mod) {
                continue;
            }

            const RE::FormID localID = mod->IsLight() ? (rawID & 0xFFF) : (rawID & 0xFFFFFF);

            auto* eff = dh->LookupForm<RE::EffectSetting>(localID, file);
            if (!eff) {
                continue;
            }

            out.insert(eff->GetFormID());
        }
    }

    static bool IsEffectExcluded(RE::EffectSetting* eff) {
        if (!eff) {
            return false;
        }

        return g_settings.magicEffectExcludedRuntime.find(eff->GetFormID()) !=
               g_settings.magicEffectExcludedRuntime.end();
    }

    static void PatchEffectSetting(RE::EffectSetting* eff) {
        if (!eff) {
            return;
        }

        if (g_settings.removeActorFX) {
            eff->data.effectShader = nullptr;
        }

        if (g_settings.removeWeaponFX) {
            eff->data.enchantShader = nullptr;
        }
    }

    static bool IsEffectListedForRemoval(RE::EffectSetting* eff) {
        if (!eff) {
            return false;
        }

        return g_settings.magicEffectRemovalRuntime.find(eff->GetFormID()) !=
               g_settings.magicEffectRemovalRuntime.end();
    }

    static bool ShouldRemoveFX(RE::EffectSetting* eff) {
        if (!eff) {
            return false;
        }

        if (IsEffectListedForRemoval(eff)) {
            return true;
        }

        if (IsEffectExcluded(eff)) {
            return false;
        }

        if (IsProtectedMagicEffect(eff)) {
            return false;
        }

        if (IsFormFromListedPlugin(eff, g_settings.removalList)) {
            return true;
        }

        if (g_settings.removeOnlyListed) {
            return false;
        }

        return !IsExcludedByPlugin(eff);
    }

    static void PatchAllMagicEffects() {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            return;
        }

        auto& effects = dh->GetFormArray<RE::EffectSetting>();
        for (auto* eff : effects) {
            if (ShouldRemoveFX(eff)) {
                PatchEffectSetting(eff);
            }
        }
    }

    static void OnDataLoaded() {
        LoadConfig();
        ResolveMagicEffectSpecs(g_settings.magicEffectExclusionSpecs, g_settings.magicEffectExcludedRuntime);
        ResolveMagicEffectSpecs(g_settings.magicEffectRemovalSpecs, g_settings.magicEffectRemovalRuntime);

        if (g_settings.removeActorFX || g_settings.removeWeaponFX) {
            PatchAllMagicEffects();
        }
    }

    static void OnMessage(SKSE::MessagingInterface::Message* msg) {
        if (!msg) {
            return;
        }

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            OnDataLoaded();
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        return false;
    }

    messaging->RegisterListener(GlowBeGone::OnMessage);
    return true;
}