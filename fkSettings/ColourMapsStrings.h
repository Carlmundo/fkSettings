#pragma once
#include <cstddef>
#include <string>

namespace ColourMaps
{
    struct MapStrings
    {
        std::wstring strImportFolder;
        std::wstring strHostMapError;
        std::wstring strLocalMapError;
        std::wstring strPlayer;
        std::wstring strMissingMap;
        std::wstring strDifferentMap;
        std::wstring strInvalidMap;
        std::wstring strCannotSaveMap;
        std::wstring strFileNotFound;
        std::wstring strFileMismatch;
        std::wstring strInvalidFile;
        std::wstring strSaveFailed;
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
            strings.strHostMapError = L"Následující hráči nemohou použít tvou mapu:";
            strings.strLocalMapError = L"Nemůžeš použít hostitelovu mapu.";
            strings.strPlayer = L"Hráč";
            strings.strMissingMap = L"Soubor s mapou chybí.";
            strings.strDifferentMap = L"Mapa nesouhlasí s verzí hostitele.";
            strings.strInvalidMap = L"Soubor s mapou nebo cesta jsou neplatné.";
            strings.strCannotSaveMap = L"Chyba ukládání Data\\land.dat";
            strings.strFileNotFound = L"Soubor nenalezen";
            strings.strFileMismatch = L"Nesoulad souboru";
            strings.strInvalidFile = L"Neplatný soubor";
            strings.strSaveFailed = L"Ukládání selhalo";
            break;
        case 1: // de
            strings.strImportFolder = L"Wählen Sie eine .dat-Datei im Ordner Levels\\Import des Spiels aus.";
            strings.strHostMapError = L"Die folgenden Spieler können Ihre Karte nicht verwenden:";
            strings.strLocalMapError = L"Sie können die Karte des Hosts nicht verwenden.";
            strings.strPlayer = L"Spieler";
            strings.strMissingMap = L"Die Kartendatei fehlt.";
            strings.strDifferentMap = L"Die Karte stimmt nicht mit der Version des Hosts überein.";
            strings.strInvalidMap = L"Die Kartendatei oder der Pfad ist ungültig.";
            strings.strCannotSaveMap = L"Fehler beim Speichern von Data\\land.dat";
            strings.strFileNotFound = L"Datei nicht gefunden";
            strings.strFileMismatch = L"Datei stimmt nicht überein";
            strings.strInvalidFile = L"Ungültige Datei";
            strings.strSaveFailed = L"Speichern fehlgeschlagen";
            break;
        case 2: // en
        default:
            strings.strImportFolder = L"Select a .dat file inside the game's Levels\\Import folder.";
            strings.strHostMapError = L"The following players are unable to use your map:";
            strings.strLocalMapError = L"You cannot use the host's map.";
            strings.strPlayer = L"Player";
            strings.strMissingMap = L"The map file is missing.";
            strings.strDifferentMap = L"The map does not match the host's version.";
            strings.strInvalidMap = L"The map file or path is invalid.";
            strings.strCannotSaveMap = L"Error saving Data\\land.dat";
            strings.strFileNotFound = L"File not found";
            strings.strFileMismatch = L"File mismatch";
            strings.strInvalidFile = L"Invalid file";
            strings.strSaveFailed = L"Save failed";
            break;
        case 3: // es
            strings.strImportFolder = L"Selecciona un archivo .dat dentro de la carpeta Levels\\Import del juego.";
            strings.strHostMapError = L"Los siguientes jugadores no pueden usar tu mapa:";
            strings.strLocalMapError = L"No puedes usar el mapa del anfitrión.";
            strings.strPlayer = L"Jugador";
            strings.strMissingMap = L"Falta el archivo del mapa.";
            strings.strDifferentMap = L"El mapa no coincide con la versión del anfitrión.";
            strings.strInvalidMap = L"El archivo del mapa o la ruta no son válidos.";
            strings.strCannotSaveMap = L"Error al guardar Data\\land.dat";
            strings.strFileNotFound = L"Archivo no encontrado";
            strings.strFileMismatch = L"El archivo no coincide";
            strings.strInvalidFile = L"Archivo no válido";
            strings.strSaveFailed = L"Error al guardar";
            break;
        case 4: // es-419
            strings.strImportFolder = L"Selecciona un archivo .dat dentro de la carpeta Levels\\Import del juego.";
            strings.strHostMapError = L"Los siguientes jugadores no pueden usar tu mapa:";
            strings.strLocalMapError = L"No puedes usar el mapa del anfitrión.";
            strings.strPlayer = L"Jugador";
            strings.strMissingMap = L"Falta el archivo del mapa.";
            strings.strDifferentMap = L"El mapa no coincide con la versión del anfitrión.";
            strings.strInvalidMap = L"El archivo del mapa o la ruta no son válidos.";
            strings.strCannotSaveMap = L"Error al guardar Data\\land.dat";
            strings.strFileNotFound = L"Archivo no encontrado";
            strings.strFileMismatch = L"El archivo no coincide";
            strings.strInvalidFile = L"Archivo no válido";
            strings.strSaveFailed = L"Error al guardar";
            break;
        case 5: // fr
            strings.strImportFolder = L"Sélectionnez un fichier .dat dans le dossier Levels\\Import du jeu.";
            strings.strHostMapError = L"Les joueurs suivants ne peuvent pas utiliser votre carte :";
            strings.strLocalMapError = L"Vous ne pouvez pas utiliser la carte de l’hôte.";
            strings.strPlayer = L"Joueur";
            strings.strMissingMap = L"Le fichier de la carte est introuvable.";
            strings.strDifferentMap = L"La carte ne correspond pas à la version de l’hôte.";
            strings.strInvalidMap = L"Le fichier de la carte ou le chemin est invalide.";
            strings.strCannotSaveMap = L"Erreur lors de l’enregistrement de Data\\land.dat";
            strings.strFileNotFound = L"Fichier introuvable";
            strings.strFileMismatch = L"Fichier différent";
            strings.strInvalidFile = L"Fichier invalide";
            strings.strSaveFailed = L"Échec de l’enregistrement";
            break;
        case 6: // is
            strings.strImportFolder = L"Veldu .dat-skrá í möppunni Levels\\Import í leikjamöppunni.";
            strings.strHostMapError = L"Eftirfarandi leikmenn geta ekki notað kortið þitt:";
            strings.strLocalMapError = L"Þú getur ekki notað kort gestgjafans.";
            strings.strPlayer = L"Leikmaður";
            strings.strMissingMap = L"Kortaskrána vantar.";
            strings.strDifferentMap = L"Kortið samsvarar ekki útgáfu gestgjafans.";
            strings.strInvalidMap = L"Kortaskráin eða slóðin er ógild.";
            strings.strCannotSaveMap = L"Villa við vistun Data\\land.dat";
            strings.strFileNotFound = L"Skrá fannst ekki";
            strings.strFileMismatch = L"Skráin samsvarar ekki";
            strings.strInvalidFile = L"Ógild skrá";
            strings.strSaveFailed = L"Vistun mistókst";
            break;
        case 7: // it
            strings.strImportFolder = L"Seleziona un file .dat nella cartella Levels\\Import del gioco.";
            strings.strHostMapError = L"I seguenti giocatori non possono usare la tua mappa:";
            strings.strLocalMapError = L"Non puoi usare la mappa dell’host.";
            strings.strPlayer = L"Giocatore";
            strings.strMissingMap = L"Il file della mappa manca.";
            strings.strDifferentMap = L"La mappa non corrisponde alla versione dell’host.";
            strings.strInvalidMap = L"Il file della mappa o il percorso non è valido.";
            strings.strCannotSaveMap = L"Errore durante il salvataggio di Data\\land.dat";
            strings.strFileNotFound = L"File non trovato";
            strings.strFileMismatch = L"File diverso";
            strings.strInvalidFile = L"File non valido";
            strings.strSaveFailed = L"Salvataggio non riuscito";
            break;
        case 8: // nl
            strings.strImportFolder = L"Selecteer een .dat-bestand in de map Levels\\Import van het spel.";
            strings.strHostMapError = L"De volgende spelers kunnen je kaart niet gebruiken:";
            strings.strLocalMapError = L"Je kunt de kaart van de host niet gebruiken.";
            strings.strPlayer = L"Speler";
            strings.strMissingMap = L"Het kaartbestand ontbreekt.";
            strings.strDifferentMap = L"De kaart komt niet overeen met de versie van de host.";
            strings.strInvalidMap = L"Het kaartbestand of pad is ongeldig.";
            strings.strCannotSaveMap = L"Fout bij het opslaan van Data\\land.dat";
            strings.strFileNotFound = L"Bestand niet gevonden";
            strings.strFileMismatch = L"Bestand komt niet overeen";
            strings.strInvalidFile = L"Ongeldig bestand";
            strings.strSaveFailed = L"Opslaan mislukt";
            break;
        case 9: // pl
            strings.strImportFolder = L"Wybierz plik .dat znajdujący się w folderze Levels\Import gry.";
            strings.strHostMapError = L"Następujący gracze nie mogą korzystać z Twojej mapy:";
            strings.strLocalMapError = L"Nie możesz korzystać z mapy hosta.";
            strings.strPlayer = L"Gracz";
            strings.strMissingMap = L"Brakuje pliku mapy.";
            strings.strDifferentMap = L"Mapa nie jest zgodna z wersją mapy hosta.";
            strings.strInvalidMap = L"Plik mapy lub ścieżka są nieprawidłowe.";
            strings.strCannotSaveMap = L"Błąd podczas zapisywania pliku Data\\land.dat";
            strings.strFileNotFound = L"Nie znaleziono pliku";
            strings.strFileMismatch = L"Niezgodność pliku";
            strings.strInvalidFile = L"Nieprawidłowy plik";
            strings.strSaveFailed = L"Zapisywanie nie powiodło się";
            break;
        case 10: // pt
            strings.strImportFolder = L"Selecione um ficheiro .dat na pasta Levels\\Import do jogo.";
            strings.strHostMapError = L"Os seguintes jogadores não podem utilizar o seu mapa:";
            strings.strLocalMapError = L"Não pode utilizar o mapa do anfitrião.";
            strings.strPlayer = L"Jogador";
            strings.strMissingMap = L"O ficheiro do mapa não foi encontrado.";
            strings.strDifferentMap = L"O mapa não corresponde à versão do anfitrião.";
            strings.strInvalidMap = L"O ficheiro do mapa ou o caminho é inválido.";
            strings.strCannotSaveMap = L"Erro ao guardar Data\\land.dat";
            strings.strFileNotFound = L"Ficheiro não encontrado";
            strings.strFileMismatch = L"Ficheiro diferente";
            strings.strInvalidFile = L"Ficheiro inválido";
            strings.strSaveFailed = L"Erro ao guardar";
            break;
        case 11: // pt-br
            strings.strImportFolder = L"Selecione um arquivo .dat na pasta Levels\\Import do jogo.";
            strings.strHostMapError = L"Os seguintes jogadores não podem usar seu mapa:";
            strings.strLocalMapError = L"Você não pode usar o mapa do anfitrião.";
            strings.strPlayer = L"Jogador";
            strings.strMissingMap = L"O arquivo do mapa não foi encontrado.";
            strings.strDifferentMap = L"O mapa não corresponde à versão do anfitrião.";
            strings.strInvalidMap = L"O arquivo do mapa ou o caminho é inválido.";
            strings.strCannotSaveMap = L"Erro ao salvar Data\\land.dat";
            strings.strFileNotFound = L"Arquivo não encontrado";
            strings.strFileMismatch = L"Arquivo diferente";
            strings.strInvalidFile = L"Arquivo inválido";
            strings.strSaveFailed = L"Erro ao salvar";
            break;
        case 12: // ru
            strings.strImportFolder = L"Выберите файл .dat в папке Levels\\Import в каталоге игры.";
            strings.strHostMapError = L"Следующие игроки не могут использовать вашу карту:";
            strings.strLocalMapError = L"Вы не можете использовать карту хоста.";
            strings.strPlayer = L"Игрок";
            strings.strMissingMap = L"Файл карты отсутствует.";
            strings.strDifferentMap = L"Карта не совпадает с версией хоста.";
            strings.strInvalidMap = L"Файл карты или путь недопустим.";
            strings.strCannotSaveMap = L"Ошибка сохранения Data\\land.dat";
            strings.strFileNotFound = L"Файл не найден";
            strings.strFileMismatch = L"Файл не совпадает";
            strings.strInvalidFile = L"Недопустимый файл";
            strings.strSaveFailed = L"Ошибка сохранения";
            break;
        case 13: // sv
            strings.strImportFolder = L"Välj en .dat-fil i spelets Levels\\Import-mapp.";
            strings.strHostMapError = L"Följande spelare kan inte använda din karta:";
            strings.strLocalMapError = L"Du kan inte använda värdens karta.";
            strings.strPlayer = L"Spelare";
            strings.strMissingMap = L"Kartfilen saknas.";
            strings.strDifferentMap = L"Kartan matchar inte värdens version.";
            strings.strInvalidMap = L"Kartfilen eller sökvägen är ogiltig.";
            strings.strCannotSaveMap = L"Fel vid sparande av Data\\land.dat";
            strings.strFileNotFound = L"Filen hittades inte";
            strings.strFileMismatch = L"Filen matchar inte";
            strings.strInvalidFile = L"Ogiltig fil";
            strings.strSaveFailed = L"Sparandet misslyckades";
            break;
        case 14: // zh-Hans
            strings.strImportFolder = L"请选择游戏 Levels\\Import 文件夹中的 .dat 文件。";
            strings.strHostMapError = L"以下玩家无法使用你的地图：";
            strings.strLocalMapError = L"你无法使用主机的地图。";
            strings.strPlayer = L"玩家";
            strings.strMissingMap = L"地图文件不存在。";
            strings.strDifferentMap = L"地图与主机的版本不一致。";
            strings.strInvalidMap = L"地图文件或路径无效。";
            strings.strCannotSaveMap = L"保存 Data\\land.dat 时出错";
            strings.strFileNotFound = L"文件未找到";
            strings.strFileMismatch = L"文件不匹配";
            strings.strInvalidFile = L"文件无效";
            strings.strSaveFailed = L"保存失败";
            break;
        }
        return strings;
    }
}
