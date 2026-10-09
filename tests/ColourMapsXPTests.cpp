#include "../fkSettings/DirectPlayCompat.cpp"
#include "../fkSettings/ColourMaps.cpp"
#include "DirectPlayCompatTests.h"

extern "C" { extern __declspec(thread) int _Init_thread_epoch; }

int wmain(int argc, wchar_t** argv)
{
    try
    {
        namespace CM = ColourMaps;
        if (CM::traceEnabled) throw std::runtime_error("Release logging must default to disabled");
        DirectPlayCompatFixture::Run();
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
            for (const auto& text : { CM::strings.strHostMapError, CM::strings.strLocalMapError, CM::strings.strPlayer,
                CM::strings.strMissingMap, CM::strings.strDifferentMap, CM::strings.strInvalidMap,
                CM::strings.strCannotSaveMap, CM::strings.strFileNotFound,
                CM::strings.strFileMismatch, CM::strings.strInvalidFile, CM::strings.strSaveFailed })
                if (text.empty()) throw std::runtime_error("XP localized network alert");
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
            const HWND lobby = CreateWindowExW(0, L"STATIC", L"XP preview fixture", WS_POPUP,
                0, 0, 300, 150, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            const HWND nativePreview = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_GRAYFRAME,
                10, 10, 200, 80, lobby, reinterpret_cast<HMENU>(1207), GetModuleHandleW(nullptr), nullptr);
            const LONG_PTR nativeProcedure = GetWindowLongPtrW(nativePreview, GWLP_WNDPROC);
            CM::network.joining = true; CM::network.remote = std::move(resolved);
            CM::network.remote->pixels.assign(1920 * 696, 1);
            if (!lobby || !nativePreview || !CM::AttachGamePreview(lobby, 1207) || CM::gamePreviews.size() != 1 ||
                CM::gamePreviews.front().nativeWindow != nativePreview || GetWindowLongPtrW(nativePreview, GWLP_WNDPROC) != nativeProcedure)
                throw std::runtime_error("XP preview overlay preserves native control procedure");
            SetWindowLongPtrW(nativePreview, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DefWindowProcW));
            CM::RefreshAll();
            HDC dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = 200;
            info.bmiHeader.biHeight = -80; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            void* pixels = nullptr; HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            HGDIOBJ previous = SelectObject(dc, bitmap);
            SendMessageW(CM::gamePreviews.front().window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
            const auto color = CM::network.remote->palette[1];
            if (GetPixel(dc, 100, 40) != RGB(color.rgbRed, color.rgbGreen, color.rgbBlue) ||
                GetWindowLongPtrW(nativePreview, GWLP_WNDPROC) != reinterpret_cast<LONG_PTR>(DefWindowProcW))
                throw std::runtime_error("XP overlay paints after native preview detachment");
            SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
            SetWindowLongPtrW(nativePreview, GWLP_WNDPROC, nativeProcedure); DestroyWindow(lobby); CM::ResetNetwork();
            if (!CM::gamePreviews.empty()) throw std::runtime_error("XP overlay cleanup");
            puts("PASS: XP toolset overlay paints after native preview detachment without altering its procedure");
            DeleteFileW(local.c_str()); DeleteFileW((data + L"\\land.dat").c_str());
            RemoveDirectoryW(root.c_str()); RemoveDirectoryW(levels.c_str()); RemoveDirectoryW(data.c_str());
        }
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES }; InitCommonControlsEx(&controls);
        const HWND owner = CreateWindowExW(0, L"STATIC", L"XP error fixture", 0, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        const std::wstring message = L"Map error\nLevels\\Import\\\u00e9\u65e5\u672c.dat";
        const auto popup = CM::ShowErrorPopup(nullptr, owner, message.c_str(), L"Import");
        HWND dialog = nullptr; const DWORD started = GetTickCount();
        while (GetTickCount() - started < 3000)
        {
            MSG pending{};
            while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&pending); DispatchMessageW(&pending); }
            dialog = CM::ErrorPopupWindow(popup);
            if (IsWindow(dialog) && (GetDlgItem(dialog, IDOK) || GetDlgItem(dialog, IDCANCEL))) break;
            Sleep(1);
        }
        if (!IsWindow(dialog) || !IsWindowEnabled(owner) || GetWindowThreadProcessId(dialog, nullptr) == GetCurrentThreadId())
            throw std::runtime_error("XP native error worker keeps frontend enabled");
        wchar_t displayed[256]{}; GetWindowTextW(GetDlgItem(dialog, 0xffff), displayed, 256);
        if (std::wstring(displayed).find(L"Levels\\Import\\\u00e9\u65e5\u672c.dat") == std::wstring::npos)
            throw std::runtime_error("XP native error preserves Unicode path");
        PostMessageW(dialog, WM_CLOSE, 0, 0);
        const DWORD closed = GetTickCount();
        while (!InterlockedCompareExchange(&popup->finished, 0, 0) && GetTickCount() - closed < 3000) Sleep(1);
        if (!InterlockedCompareExchange(&popup->finished, 0, 0)) throw std::runtime_error("XP native error OK dismissal");
        DestroyWindow(owner); CM::CloseErrorPopups();
        puts("PASS: XP toolset native MessageBox worker keeps frontend enabled, preserves Unicode and dismisses with OK");
        puts("PASS: XP toolset CRC32, zero TLS epoch, UTF-8 paths and installed map selection");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
