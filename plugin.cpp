#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
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
        std::unordered_set<std::string> exclusionList;
        std::vector<std::pair<std::string, std::uint32_t>> magicEffectExclusionSpecs;
        std::unordered_set<RE::FormID> magicEffectExcludedRuntime;
    };

    static Settings g_settings;

    static std::string Trim(std::string s) {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    }

    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static bool ParseBool(std::string_view v, bool fallback) {
        auto s = ToLower(Trim(std::string(v)));
        if (s == "true") return true;
        if (s == "false") return false;
        return fallback;
    }

    static std::vector<std::string> ExtractQuotedStrings(const std::string& s) {
        std::vector<std::string> out;
        bool in_quote = false;
        char quote = 0;
        std::string cur;

        for (size_t i = 0; i < s.size(); i++) {
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
                    if (!t.empty()) out.push_back(t);
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
        if (t.empty()) return false;

        auto pos = t.find(':');
        if (pos == std::string::npos) pos = t.find('|');
        if (pos == std::string::npos) return false;

        auto file = Trim(t.substr(0, pos));
        auto id = Trim(t.substr(pos + 1));
        if (file.empty() || id.empty()) return false;

        if (id.rfind("0x", 0) == 0 || id.rfind("0X", 0) == 0) id = id.substr(2);

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

    static void LoadConfigFromFile(const char* path) {
        std::ifstream in(path);
        if (!in.is_open()) return;

        bool parsingStringArray = false;
        std::string arrayKey;
        std::string arrayBuf;

        std::string line;
        while (std::getline(in, line)) {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

            auto pos_hash = line.find('#');
            if (pos_hash != std::string::npos) line = line.substr(0, pos_hash);
            auto pos_slash = line.find("//");
            if (pos_slash != std::string::npos) line = line.substr(0, pos_slash);

            line = Trim(line);
            if (line.empty()) continue;

            if (!parsingStringArray) {
                if (line.front() == '[' && line.back() == ']') continue;
            }

            if (parsingStringArray) {
                arrayBuf.push_back(' ');
                arrayBuf += line;

                if (line.find(']') != std::string::npos) {
                    auto items = ExtractQuotedStrings(arrayBuf);

                    if (arrayKey == "exclusionlist") {
                        g_settings.exclusionList.clear();
                        for (auto& it : items) g_settings.exclusionList.insert(ToLower(it));
                    } else if (arrayKey == "magiceffectexclusionlist") {
                        g_settings.magicEffectExclusionSpecs.clear();
                        for (auto& it : items) {
                            std::string file;
                            std::uint32_t localID = 0;
                            if (ParseFileFormPair(it, file, localID)) {
                                g_settings.magicEffectExclusionSpecs.emplace_back(file, localID);
                            }
                        }
                    }

                    parsingStringArray = false;
                    arrayKey.clear();
                    arrayBuf.clear();
                }
                continue;
            }

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

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

            if (key == "exclusionlist" || key == "magiceffectexclusionlist") {
                arrayKey = key;
                arrayBuf = val;

                if (val.find('[') == std::string::npos) {
                    parsingStringArray = true;
                    continue;
                }

                if (val.find(']') != std::string::npos) {
                    auto items = ExtractQuotedStrings(arrayBuf);

                    if (arrayKey == "exclusionlist") {
                        g_settings.exclusionList.clear();
                        for (auto& it : items) g_settings.exclusionList.insert(ToLower(it));
                    } else if (arrayKey == "magiceffectexclusionlist") {
                        g_settings.magicEffectExclusionSpecs.clear();
                        for (auto& it : items) {
                            std::string file;
                            std::uint32_t localID = 0;
                            if (ParseFileFormPair(it, file, localID)) {
                                g_settings.magicEffectExclusionSpecs.emplace_back(file, localID);
                            }
                        }
                    }

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

    static bool IsExcludedByPlugin(RE::TESForm* form) {
        if (!form || g_settings.exclusionList.empty()) return false;

        auto* file = form->GetFile(0);
        if (!file) return false;

        auto name = ToLower(std::string(file->GetFilename()));
        return g_settings.exclusionList.find(name) != g_settings.exclusionList.end();
    }

    static bool IsProtectedMagicEffect(RE::EffectSetting* eff) {
        if (!eff) return true;

        if (eff->data.associatedSkill == RE::ActorValue::kConjuration) return true;

        using A = RE::EffectSetting::Archetype;
        const auto a = eff->GetArchetype();

        switch (a) {
            case A::kBoundWeapon:
            case A::kSummonCreature:
            case A::kCommandSummoned:
            case A::kReanimate:
            case A::kSpawnScriptedRef:
                return true;
            default:
                break;
        }

        return false;
    }

    static void ResolveMagicEffectExclusions() {
        g_settings.magicEffectExcludedRuntime.clear();

        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) return;

        for (auto& [file, localID] : g_settings.magicEffectExclusionSpecs) {
            auto* form = dh->LookupForm(localID, file);
            if (!form) continue;

            auto* eff = form->As<RE::EffectSetting>();
            if (!eff) continue;

            g_settings.magicEffectExcludedRuntime.insert(eff->GetFormID());
        }
    }

    static bool IsExcludedMagicEffect(RE::EffectSetting* eff) {
        if (!eff) return true;
        if (IsProtectedMagicEffect(eff)) return true;

        auto id = eff->GetFormID();
        if (g_settings.magicEffectExcludedRuntime.find(id) != g_settings.magicEffectExcludedRuntime.end()) return true;
        if (IsExcludedByPlugin(eff)) return true;

        return false;
    }

    static void PatchEffectSetting(RE::EffectSetting* eff) {
        if (!eff) return;

        if (g_settings.removeActorFX) {
            eff->data.effectShader = nullptr;
        }

        if (g_settings.removeWeaponFX) {
            eff->data.enchantShader = nullptr;
        }
    }

    static void PatchAllMagicEffects() {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) return;

        auto& effects = dh->GetFormArray<RE::EffectSetting>();
        for (auto* eff : effects) {
            if (!eff) continue;
            if (IsExcludedMagicEffect(eff)) continue;
            PatchEffectSetting(eff);
        }
    }

    static void OnDataLoaded() {
        LoadConfig();
        ResolveMagicEffectExclusions();

        if (g_settings.removeActorFX || g_settings.removeWeaponFX) {
            PatchAllMagicEffects();
        }
    }

    static void OnMessage(SKSE::MessagingInterface::Message* msg) {
        if (!msg) return;

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            OnDataLoaded();
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) return false;

    messaging->RegisterListener(GlowBeGone::OnMessage);
    return true;
}