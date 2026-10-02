#pragma once
#include <Windows.h>
#include <cstddef>
#include <string>

namespace ExtendedOptions
{
    struct OptionStrings
    {
        std::wstring strGodMode;
        std::wstring strHighJump;
        std::wstring strSuicideBomber;
        std::wstring strSheepHeaven;
        std::wstring strSuperShopperCrates;
        std::wstring strExtendedFusesHerds;
        std::wstring strUtilitiesDontEndTurn;
        std::wstring strWeaponsDontEndTurn;
        std::wstring strLossOfControlDoesntEndTurn;
        std::wstring strWormSelectAfterMovement;
        std::wstring strLowGravity;
        std::wstring strPersistentRope;
        std::wstring strRapidPlay;
        std::wstring strIndestructibleTerrain;
        std::wstring strInvisibleTerrain;
        std::wstring strFastCrates;
        std::wstring strCrateSpy;
        std::wstring strCrateLimit;
        std::wstring strCrateRate;
        std::wstring strAquaSheep;
        std::wstring strInstantMines;
        std::wstring strHerd;
        std::wstring strHerdDynamite;
        std::wstring strHerdMine;
        std::wstring strHerdMingVase;
        std::wstring strHerdSheep;
        std::wstring strDisableBackflip;
        std::wstring strDisableUnlockedAim;
        std::wstring strExtendedOptions;
    };

    inline std::wstring OptionResourceString(HMODULE resourceModule, UINT stringId)
    {
        wchar_t text[256]{};
        LoadStringW(resourceModule, stringId, text, 256);
        return text;
    }

    // Fill each language case below. Blank translations intentionally remain blank.
    inline OptionStrings MakeOptionStrings(const std::string& language, HMODULE resourceModule)
    {
        OptionStrings strings{};
        const char* const languages[] = {
            "cs", "de", "en", "es", "es-419", "fr", "is", "it", "nl", "pl", "pt", "pt-br", "ru", "sv", "zh-Hans"
        };
        std::string code = language;
        if (code.compare(0, 3, "\xEF\xBB\xBF") == 0) code.erase(0, 3);
        const auto first = code.find_first_not_of(" \t\r\n");
        code = first == std::string::npos ? std::string{} :
            code.substr(first, code.find_last_not_of(" \t\r\n") - first + 1);
        size_t languageIndex = 2; // English fallback.
        for (size_t i = 0; i < sizeof(languages) / sizeof(languages[0]); ++i)
            if (code == languages[i]) { languageIndex = i; break; }

        switch (languageIndex)
        {
        case 0: // cs
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 1: // de
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 2: // en
        default: // Unknown or missing language uses English.
            strings.strGodMode = L"God Mode";
            strings.strHighJump = L"High Jump";
            strings.strSuicideBomber = L"Change Kamikaze to Suicide Bomber";
            strings.strSheepHeaven = L"Sheep Heaven";
            strings.strSuperShopperCrates = L"Super Shopper Crates";
            strings.strExtendedFusesHerds = L"Extended Fuses/Herds";
            strings.strUtilitiesDontEndTurn = L"Utilities don't end turn";
            strings.strWeaponsDontEndTurn = L"Weapons don't end turn";
            strings.strLossOfControlDoesntEndTurn = L"Loss of control doesn't end turn";
            strings.strWormSelectAfterMovement = L"Worm select after movement";
            strings.strLowGravity = L"Low Gravity";
            strings.strPersistentRope = L"Persistent Rope";
            strings.strRapidPlay = L"Rapid Play";
            strings.strIndestructibleTerrain = L"Indestructible Terrain";
            strings.strInvisibleTerrain = L"Invisible Terrain";
            strings.strFastCrates = L"Fast Crates";
            strings.strCrateSpy = L"Crate Spy";
            strings.strCrateLimit = L"Crate Limit";
            strings.strCrateRate = L"Crate Rate";
            strings.strAquaSheep = L"Aqua Sheep";
            strings.strInstantMines = L"Instant Mines";
            strings.strHerd = L"Herd weapon";
            strings.strDisableBackflip = L"Disable Backflip";
            strings.strDisableUnlockedAim = L"Disable Unlocked Aim";
            strings.strExtendedOptions = L"Extended Options";
            break;

        case 3: // es
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 4: // es-419
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 5: // fr
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 6: // is
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 7: // it
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 8: // nl
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 9: // pl
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 10: // pt
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 11: // pt-br
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 12: // ru
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 13: // sv
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        case 14: // zh-Hans
            strings.strGodMode = L"";
            strings.strHighJump = L"";
            strings.strSuicideBomber = L"";
            strings.strSheepHeaven = L"";
            strings.strSuperShopperCrates = L"";
            strings.strExtendedFusesHerds = L"";
            strings.strUtilitiesDontEndTurn = L"";
            strings.strWeaponsDontEndTurn = L"";
            strings.strLossOfControlDoesntEndTurn = L"";
            strings.strWormSelectAfterMovement = L"";
            strings.strLowGravity = L"";
            strings.strPersistentRope = L"";
            strings.strRapidPlay = L"";
            strings.strIndestructibleTerrain = L"";
            strings.strInvisibleTerrain = L"";
            strings.strFastCrates = L"";
            strings.strCrateSpy = L"";
            strings.strCrateLimit = L"";
            strings.strCrateRate = L"";
            strings.strAquaSheep = L"";
            strings.strInstantMines = L"";
            strings.strHerd = L"";
            strings.strDisableBackflip = L"";
            strings.strDisableUnlockedAim = L"";
            strings.strExtendedOptions = L"";
            break;

        }
        strings.strHerdDynamite = strings.strHerd + L": " + OptionResourceString(resourceModule, 4915);
        strings.strHerdMine = strings.strHerd + L": " + OptionResourceString(resourceModule, 4916);
        strings.strHerdMingVase = strings.strHerd + L": " + OptionResourceString(resourceModule, 4917);
        strings.strHerdSheep = strings.strHerd + L": " + OptionResourceString(resourceModule, 4930);
        return strings;
    }
}
