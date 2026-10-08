#include "../fkSettings/ColourMaps.cpp"

extern "C" { extern __declspec(thread) int _Init_thread_epoch; }

int wmain(int argc, wchar_t** argv)
{
    try
    {
        namespace CM = ColourMaps;
        CM::traceEnabled = false;
        const BYTE digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
        const int savedEpoch = _Init_thread_epoch; _Init_thread_epoch = 0;
        const uint32_t first = CM::Checksum(digits, sizeof(digits)); _Init_thread_epoch = savedEpoch;
        if (first != 0xcbf43926 || CM::Checksum(nullptr, 0) != 0) throw std::runtime_error("XP first CRC32");
        std::vector<BYTE> bytes(256);
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<BYTE>(i);
        if (CM::Checksum(bytes) != 0x29058c73) throw std::runtime_error("CRC32 of all byte values");
        const std::wstring path = L"Online Worms\\\u00e9\u65e5\u672c.dat";
        if (CM::DecodePath(CM::EncodePath(path)) != path || CM::ValidRelativePath(L"..\\outside.dat"))
            throw std::runtime_error("XP Unicode and confined map paths");
        for (const char* language : { "cs", "de", "en", "es", "es-419", "fr", "is", "it", "nl", "pl", "pt", "pt-br", "ru", "sv", "zh-Hans" })
        {
            CM::SetLanguage(std::string("\xEF\xBB\xBF") + language + "\r\n");
            bool rejected = false;
            try { CM::ImportedPath(L"..\\outside.dat"); }
            catch (const std::runtime_error& error)
            {
                wchar_t text[256]{};
                if (!MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, text, 256) || text != CM::strings.strImportFolder)
                    throw std::runtime_error("XP localized import message encoding");
                rejected = true;
            }
            if (!rejected) throw std::runtime_error("XP localized path rejection");
        }
        CM::SetLanguage("en");
        puts("PASS: XP toolset preserves all 15 localized import messages through UTF-8/Unicode conversion");
        if (argc == 2)
        {
            auto map = CM::Load(argv[1]);
            if (map.sourceCrc != 0x3a3314b7) throw std::runtime_error("Birthday CRC32");
            map.relativePath = L"XP Local Map Fixture.dat";
            const auto levels = CM::GameDirectory() + L"\\Levels", root = CM::ImportRoot(), data = CM::GameDirectory() + L"\\Data";
            if (GetFileAttributesW(root.c_str()) != INVALID_FILE_ATTRIBUTES || GetFileAttributesW(data.c_str()) != INVALID_FILE_ATTRIBUTES)
                throw std::runtime_error("XP test needs isolated Release/Levels/Import and Release/Data");
            CreateDirectoryW(levels.c_str(), nullptr); CreateDirectoryW(root.c_str(), nullptr); CreateDirectoryW(data.c_str(), nullptr);
            const auto local = CM::ImportedPath(map.relativePath);
            if (!CM::Publish(map, local)) throw std::runtime_error("stage XP installed map");
            CM::MapIdentity identity; identity.revision = 1; identity.size = static_cast<uint32_t>(map.bytes.size()); identity.crc = CM::Checksum(map.bytes);
            auto packet = CM::ReferencePacket(&map, identity);
            CM::MapReference reference; std::unique_ptr<CM::Map> resolved;
            if (packet.size() >= 1200 || CM::ResolveReference(packet.data(), static_cast<uint32_t>(packet.size()), reference, resolved) != CM::MapResult::Ready ||
                !resolved || resolved->bytes != map.bytes) throw std::runtime_error("XP installed map validation/publication");
            printf("PASS: XP toolset validates Birthday from its installed path using %u metadata bytes\n", static_cast<unsigned>(packet.size()));
            DeleteFileW(local.c_str()); DeleteFileW((data + L"\\land.dat").c_str());
            RemoveDirectoryW(root.c_str()); RemoveDirectoryW(levels.c_str()); RemoveDirectoryW(data.c_str());
        }
        puts("PASS: XP toolset CRC32, zero TLS epoch, UTF-8 paths and installed map selection");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
