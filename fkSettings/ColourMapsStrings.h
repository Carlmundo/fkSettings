#pragma once
#include <cstddef>
#include <string>

namespace ColourMaps
{
    struct MapStrings
    {
        std::wstring strImportFolder;
    };

    inline MapStrings MakeMapStrings(const std::string& language)
    {
        MapStrings strings{};
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
            strings.strImportFolder = L"Vyberte soubor .dat ve složce Levels\\Import v adresáři hry.";
            break;
        case 1: // de
            strings.strImportFolder = L"Wählen Sie eine .dat-Datei im Ordner Levels\\Import des Spiels aus.";
            break;
        case 2: // en
        default:
            strings.strImportFolder = L"Select a .dat file inside the game's Levels\\Import folder.";
            break;
        case 3: // es
            strings.strImportFolder = L"Selecciona un archivo .dat dentro de la carpeta Levels\\Import del juego.";
            break;
        case 4: // es-419
            strings.strImportFolder = L"Selecciona un archivo .dat dentro de la carpeta Levels\\Import del juego.";
            break;
        case 5: // fr
            strings.strImportFolder = L"Sélectionnez un fichier .dat dans le dossier Levels\\Import du jeu.";
            break;
        case 6: // is
            strings.strImportFolder = L"Veldu .dat-skrá í möppunni Levels\\Import í leikjamöppunni.";
            break;
        case 7: // it
            strings.strImportFolder = L"Seleziona un file .dat nella cartella Levels\\Import del gioco.";
            break;
        case 8: // nl
            strings.strImportFolder = L"Selecteer een .dat-bestand in de map Levels\\Import van het spel.";
            break;
        case 9: // pl
            strings.strImportFolder = L"Wybierz plik .dat w folderze Levels\\Import w katalogu gry.";
            break;
        case 10: // pt
            strings.strImportFolder = L"Selecione um ficheiro .dat na pasta Levels\\Import do jogo.";
            break;
        case 11: // pt-br
            strings.strImportFolder = L"Selecione um arquivo .dat na pasta Levels\\Import do jogo.";
            break;
        case 12: // ru
            strings.strImportFolder = L"Выберите файл .dat в папке Levels\\Import в каталоге игры.";
            break;
        case 13: // sv
            strings.strImportFolder = L"Välj en .dat-fil i spelets Levels\\Import-mapp.";
            break;
        case 14: // zh-Hans
            strings.strImportFolder = L"请选择游戏 Levels\\Import 文件夹中的 .dat 文件。";
            break;
        }
        return strings;
    }
}
