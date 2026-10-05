#pragma once
#include <cstddef>
#include <string>

namespace SecretWeapons
{
    struct WeaponStrings
    {
        std::wstring strSalvationArmy;
        std::wstring strMBBomb;
        std::wstring strSheepStrike;
        std::wstring strCarpetBomb;
        std::wstring strClonedSheep;
        std::wstring strConcreteDonkey;
        std::wstring strNuclearBomb;
        std::wstring strMagicBullet;
    };

    // Imported menu names; longer native alternatives are kept as commented assignments.
    // Chinese has no supplied game translation and retains English placeholders.
    inline WeaponStrings MakeWeaponStrings(const std::string& language)
    {
        WeaponStrings strings{};
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
            strings.strSalvationArmy = L"Armáda spásy";
            strings.strMBBomb = L"MB bomba";
            strings.strSheepStrike = L"Ovčí útok";
            strings.strCarpetBomb = L"Kobercová bomba";
            strings.strClonedSheep = L"Naklon. ovce";
            strings.strConcreteDonkey = L"Betonový osel";
            strings.strNuclearBomb = L"Jaderná bomba";
            strings.strMagicBullet = L"Magická kulka";
            break;

        case 1: // de
            strings.strSalvationArmy = L"Heilsarmee";
            strings.strMBBomb = L"MB-Bombe";
            strings.strSheepStrike = L"Schafsangriff";
            strings.strCarpetBomb = L"Bombenteppich";
            strings.strClonedSheep = L"Schafsklon";
            strings.strConcreteDonkey = L"Betonesel";
            strings.strNuclearBomb = L"Atombombe";
            strings.strMagicBullet = L"Zauberkugel";
            break;

        case 2: // en
        case 14: // zh-Hans
        default: // Unknown or missing language uses English.
            strings.strSalvationArmy = L"Salvation Army";
            strings.strMBBomb = L"MB Bomb";
            strings.strSheepStrike = L"Sheep Strike";
            strings.strCarpetBomb = L"Carpet Bomb";
            strings.strClonedSheep = L"Cloned Sheep";
            strings.strConcreteDonkey = L"Concrete Donkey";
            strings.strNuclearBomb = L"Nuclear Bomb";
            strings.strMagicBullet = L"Magic Bullet";
            break;

        case 3: // es
            strings.strSalvationArmy = L"Ejército de Salvación";
            strings.strMBBomb = L"Bomba MB";
            strings.strSheepStrike = L"Ataque Ovejas";
            strings.strCarpetBomb = L" Bomba Carpet";
            strings.strClonedSheep = L"Oveja Clónica";
            strings.strConcreteDonkey = L"Burro de Hormigón";
            strings.strNuclearBomb = L"Bomba Nuclear";
            strings.strMagicBullet = L"Bala Mágica";
            break;

        case 4: // es-419
            strings.strSalvationArmy = L"Ejército Salva";
            strings.strMBBomb = L"Bomba MB";
            strings.strSheepStrike = L"Ataque ovejuno";
            strings.strCarpetBomb = L"Bomba arrasa";
            strings.strClonedSheep = L"Oveja clónica";
            strings.strConcreteDonkey = L"Burro de hormigón";
            strings.strNuclearBomb = L"Bomba nuclear";
            strings.strMagicBullet = L"Bala mágica";
            break;

        case 5: // fr
            strings.strSalvationArmy = L"Armée du salut";
            strings.strMBBomb = L"Bombe MB";
            strings.strSheepStrike = L"Attaque du mouton";
            strings.strCarpetBomb = L"Bombe tapis";
            strings.strClonedSheep = L"Mouton cloné";
            strings.strConcreteDonkey = L"Âne de ciment";
            strings.strNuclearBomb = L"Bombe nucléaire";
            strings.strMagicBullet = L"Balle magique";
            break;

        case 6: // is
            strings.strSalvationArmy = L"Hjálpræðisherinn";
            strings.strMBBomb = L"MB-sprengja";
            strings.strSheepStrike = L"Kindarárás";
            strings.strCarpetBomb = L"Teppasprengja";
            strings.strClonedSheep = L"Klónuð kind";
            strings.strConcreteDonkey = L"Steypuasninn";
            strings.strNuclearBomb = L"Kjarnorkusprengja";
            strings.strMagicBullet = L"Töfrakúla";
            break;

        case 7: // it
            strings.strSalvationArmy = L"Esercito della salvezza";
            strings.strMBBomb = L"Bomba MB";
            strings.strSheepStrike = L"Attacco Pecore";
            strings.strCarpetBomb = L"Bomba tappeto";
            strings.strClonedSheep = L"Pecora clonata";
            strings.strConcreteDonkey = L"Asino cemento";
            strings.strNuclearBomb = L"Bomba nucleare";
            strings.strMagicBullet = L"Pallottola magica";
            break;

        case 8: // nl
            strings.strSalvationArmy = L"Leger des Twijfels";
            strings.strMBBomb = L"MB Bom";
            strings.strSheepStrike = L"Schaapaanval";
            strings.strCarpetBomb = L"Tapijt Bom";
            strings.strClonedSheep = L"Namaakschapen";
            strings.strConcreteDonkey = L"Betonnen Ezel";
            strings.strNuclearBomb = L"Atoombom";
            strings.strMagicBullet = L"Tover Kogel";
            break;

        case 9: // pl
            strings.strSalvationArmy = L"Armia Zbawienia";
            strings.strMBBomb = L"Bomba megabajtowa";
            strings.strSheepStrike = L"Owczy atak";
            strings.strCarpetBomb = L"Nalot dywanowy";
            strings.strClonedSheep = L"Sklonowane owce";
            strings.strConcreteDonkey = L"Betonowy osioł";
            strings.strNuclearBomb = L"Bomba nuklearna";
            strings.strMagicBullet = L"Magiczny pocisk";
            break;

        case 10: // pt
            strings.strSalvationArmy = L"Exército de Resgate";
            strings.strMBBomb = L"Bomba MB";
            strings.strSheepStrike = L"Ataque Ovelhas";
            strings.strCarpetBomb = L"Bomba Carpete";
            strings.strClonedSheep = L"Ovelha Clonada";
            strings.strConcreteDonkey = L"Burro de Cimento";
            strings.strNuclearBomb = L"Bomba Nuclear";
            strings.strMagicBullet = L"Bala Mágica";
            break;

        case 11: // pt-br
            strings.strSalvationArmy = L"Exército da Salvação";
            strings.strMBBomb = L"Bomba MB";
            strings.strSheepStrike = L"Golpe de Áries";
            strings.strCarpetBomb = L"Tapete Voador";
            strings.strClonedSheep = L"Carneiro-Bomba 2";
            strings.strConcreteDonkey = L"Asno Duro";
            strings.strNuclearBomb = L"Bomba Nuclear";
            strings.strMagicBullet = L"Bala Mágica";
            break;

        case 12: // ru
            strings.strSalvationArmy = L"Армия спасения";
            strings.strMBBomb = L"Бомба МБ";
            strings.strSheepStrike = L"Удар овец";
            strings.strCarpetBomb = L"Ковровая бомбардировка";
            strings.strClonedSheep = L"Клонированная овца";
            strings.strConcreteDonkey = L"Бетонный осёл";
            strings.strNuclearBomb = L"Атомная бомба";
            strings.strMagicBullet = L"Волшебная пуля";
            break;

        case 13: // sv
            strings.strSalvationArmy = L"Frälsningsarmén";
            strings.strMBBomb = L"MB Bomb";
            strings.strSheepStrike = L"Får-attack";
            strings.strCarpetBomb = L"Bomb Matta";
            strings.strClonedSheep = L"Klonat Får";
            strings.strConcreteDonkey = L"Betong Åsna";
            strings.strNuclearBomb = L"Atom Bomb";
            strings.strMagicBullet = L"Magisk Kula";
            break;

        }
        return strings;
    }
}
