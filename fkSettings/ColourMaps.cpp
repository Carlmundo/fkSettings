#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "ColourMaps.h"
#include "ColourMapsStrings.h"
#include "include/MinHook.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace ColourMaps
{
namespace
{
    constexpr int ImportId = 51010, PreviewId = 51012, PreviewLabelId = 51015, CancelId = 51016,
        LevelStyleId = 1021, WaterId = 1020, GenerateId = 1031, EditTerrainId = 1032, SaveAsId = 1023;
    constexpr UINT ImportStringId = 447, PreviewLabelStringId = 726, TerrainStringId = 357, CancelStringId = 19;
    constexpr int PreviewVerticalOffset = 6; // Dialog units below the native Generate baseline.
    constexpr size_t MaximumFileSize = 8 * 1024 * 1024;
    MapStrings strings = MakeMapStrings("en");
    std::string ImportFolderErrorText()
    {
        const auto& text = strings.strImportFolder;
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string message(length, '\0');
        if (length) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &message[0], length, nullptr, nullptr);
        if (!message.empty()) message.pop_back();
        return message;
    }
    std::wstring ResourceCaption(UINT stringId, const wchar_t* fallback, HWND resourceOwner = nullptr)
    {
        const HINSTANCE resources = resourceOwner ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(resourceOwner, GWLP_HINSTANCE)) :
            GetModuleHandleW(nullptr);
        wchar_t text[128]{};
        if (LoadStringW(resources, stringId, text, sizeof(text) / sizeof(text[0]))) return text;
        return fallback;
    }
    std::wstring ImportCaption(HWND resourceOwner = nullptr)
    { return ResourceCaption(ImportStringId, L"Import", resourceOwner); }
    void ShowImportError(HWND owner, const std::exception& error, HWND resourceOwner)
    {
        const char* text = error.what();
        const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        std::wstring message(length ? length : 1, L'\0');
        if (length) MultiByteToWideChar(CP_UTF8, 0, text, -1, &message[0], length);
        MessageBoxW(owner, message.c_str(), ImportCaption(resourceOwner).c_str(), MB_OK | MB_ICONERROR);
    }
    struct Map
    {
        std::vector<BYTE> bytes, pixels;
        std::array<RGBQUAD, 256> palette{};
        std::wstring relativePath;
        uint32_t sourceSize = 0, sourceCrc = 0;
        bool cavern = false;
        size_t waterOffset = 0;
        std::string waterPath;
    };
    struct Reader
    {
        const std::vector<BYTE>& bytes;
        size_t position = 0;
        void Require(size_t count) const
        {
            if (count > bytes.size() - position) throw std::runtime_error("Truncated terrain file.");
        }
        unsigned Byte() { Require(1); return bytes[position++]; }
        unsigned Word() { unsigned n = Byte(); return n | (Byte() << 8); }
        uint32_t Dword() { uint32_t n = Word(); return n | (Word() << 16); }
        void Skip(size_t count) { Require(count); position += count; }
        std::string Path()
        {
            unsigned length = Byte();
            Require(length);
            for (unsigned i = 0; i < length; ++i)
                if (bytes[position + i] < 32) throw std::runtime_error("Invalid terrain resource path.");
            const std::string path(bytes.begin() + position, bytes.begin() + position + length);
            Skip(length);
            return path;
        }
    };

    void ReadImage(Reader& reader, Map& map, unsigned imageIndex)
    {
        const size_t start = reader.position;
        if (reader.Dword() != 0x1a474d49) throw std::runtime_error("Missing terrain image.");
        const unsigned declaredEnd = reader.Dword();
        unsigned bits = reader.Byte();
        if (bits == 0) bits = reader.Byte();
        else if (bits > 32)
        {
            while (reader.Byte() != 0) {}
            bits = reader.Byte();
        }
        const unsigned flags = reader.Byte();
        if (bits != (imageIndex == 0 ? 8u : 1u) || (flags & ~0xc0u))
            throw std::runtime_error("Expected an 8-bit foreground and 1-bit terrain masks.");
        unsigned colors = 0;
        if (flags & 0x80)
        {
            colors = reader.Word();
            if (colors > (1u << bits) - 1) throw std::runtime_error("Invalid image palette.");
            for (unsigned i = 1; i <= colors; ++i)
            {
                RGBQUAD color{};
                color.rgbRed = static_cast<BYTE>(reader.Byte());
                color.rgbGreen = static_cast<BYTE>(reader.Byte());
                color.rgbBlue = static_cast<BYTE>(reader.Byte());
                if (imageIndex == 0) map.palette[i] = color;
            }
        }
        if (imageIndex == 0 && !(flags & 0x80)) throw std::runtime_error("The foreground has no palette.");
        unsigned width = reader.Word(), height = reader.Word();
        if (width != (imageIndex == 3 ? 240u : 1920u) || height != (imageIndex == 3 ? 87u : 696u))
            throw std::runtime_error("This frontend supports standard 1920 x 696 terrain files.");
        const size_t size = static_cast<size_t>(width) * height * bits / 8;
        std::vector<BYTE> pixels;
        if (flags & 0x40)
        {
            pixels.reserve(size);
            for (;;)
            {
                const unsigned command = reader.Byte();
                if (!(command & 0x80))
                {
                    if (pixels.size() == size) throw std::runtime_error("Image exceeds its dimensions.");
                    pixels.push_back(static_cast<BYTE>(command));
                    continue;
                }
                unsigned length = (command >> 3) & 15;
                unsigned distance = ((command << 8) | reader.Byte()) & 0x7ff;
                if (!length && !distance) break;
                if (length) { length += 2; ++distance; }
                else length = reader.Byte() + 18;
                if (!distance || distance > pixels.size() || length > size - pixels.size())
                    throw std::runtime_error("Invalid compressed terrain image.");
                while (length--) pixels.push_back(pixels[pixels.size() - distance]);
            }
            if (pixels.size() != size) throw std::runtime_error("Incomplete compressed terrain image.");
        }
        else
        {
            reader.Require(size);
            if (imageIndex == 0)
                pixels.assign(reader.bytes.begin() + reader.position, reader.bytes.begin() + reader.position + size);
            reader.Skip(size);
        }
        // Syroot saves embedded sizes as absolute stream positions. Other
        // writers use chunk lengths; both must describe exactly what we read.
        if (declaredEnd != reader.position && declaredEnd != reader.position - start)
            throw std::runtime_error("Incorrect terrain image size.");
        if (imageIndex == 0)
        {
            for (BYTE pixel : pixels)
                if (pixel > colors) throw std::runtime_error("Foreground index exceeds its palette.");
            map.pixels.swap(pixels);
        }
    }

    Map Parse(std::vector<BYTE> bytes)
    {
        if (bytes.size() > MaximumFileSize) throw std::runtime_error("Terrain file is too large.");
        Map map;
        map.bytes.swap(bytes);
        Reader reader{ map.bytes };
        if (reader.Dword() != 0x1a444e4c || reader.Dword() != map.bytes.size())
            throw std::runtime_error("Not a complete Worms 2 LND terrain file.");
        if (reader.Dword() != 1920 || reader.Dword() != 696)
            throw std::runtime_error("This frontend supports standard 1920 x 696 terrain files.");
        unsigned border = reader.Dword();
        if (border > 1) throw std::runtime_error("Invalid cavern flag.");
        map.cavern = border != 0;
        const unsigned spawns = reader.Dword();
        if (spawns < 18 || spawns > 32) throw std::runtime_error("Expected 18 to 32 object locations.");
        for (unsigned i = 0; i < spawns; ++i)
        {
            int32_t x = static_cast<int32_t>(reader.Dword()), y = static_cast<int32_t>(reader.Dword());
            if ((x != -1 || y != -1) && (x < 0 || x >= 1920 || y < 0 || y >= 696))
                throw std::runtime_error("Object location is outside the map.");
        }
        reader.Dword(); // Native unknown field; preserve it in the original bytes.
        for (unsigned i = 0; i < 4; ++i) ReadImage(reader, map, i);
        reader.Path();
        map.waterOffset = reader.position;
        map.waterPath = reader.Path();
        if (reader.position != map.bytes.size()) throw std::runtime_error("Unexpected trailing terrain data.");
        return map;
    }

    std::wstring WaterName(const Map& map)
    {
        const size_t slash = map.waterPath.find_last_of("\\/");
        const std::string name = map.waterPath.substr(slash == std::string::npos ? 0 : slash + 1);
        const int length = MultiByteToWideChar(CP_ACP, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
        std::wstring wide(length, L'\0');
        if (length) MultiByteToWideChar(CP_ACP, 0, name.data(), static_cast<int>(name.size()), &wide[0], length);
        return wide;
    }

    bool ChangeWater(Map& map, const std::wstring& name)
    {
        if (name.empty() || name.find_first_of(L"\\/") != std::wstring::npos) return false;
        if (_wcsicmp(name.c_str(), WaterName(map).c_str()) == 0) return true;
        BOOL substituted = FALSE;
        const int length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0, nullptr, &substituted);
        if (!length || substituted) return false;
        std::string path = "Data\\Water\\";
        const size_t prefix = path.size();
        path.resize(prefix + length);
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name.data(), static_cast<int>(name.size()), &path[prefix], length, nullptr, &substituted);
        if (substituted || path.size() > 255 || std::any_of(path.begin(), path.end(), [](unsigned char c) { return c < 32; }) ||
            map.waterOffset < 8 || map.waterOffset > map.bytes.size() || map.waterOffset + 1 + path.size() > MaximumFileSize) return false;
        // The final LND field is a byte-length-prefixed path. Embedded IMG
        // sizes precede it, so only the outer LND length needs updating.
        std::vector<BYTE> bytes(map.bytes.begin(), map.bytes.begin() + map.waterOffset);
        bytes.push_back(static_cast<BYTE>(path.size()));
        bytes.insert(bytes.end(), path.begin(), path.end());
        const uint32_t size = static_cast<uint32_t>(bytes.size());
        memcpy(bytes.data() + 4, &size, sizeof(size));
        map.bytes.swap(bytes);
        map.waterPath.swap(path);
        return true;
    }

    uint32_t Checksum(const std::vector<BYTE>& bytes);
    Map Load(const wchar_t* path)
    {
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open the selected terrain file.");
        LARGE_INTEGER length{};
        std::vector<BYTE> bytes;
        DWORD read = 0;
        bool success = GetFileSizeEx(file, &length) && length.QuadPart >= 28 && length.QuadPart <= MaximumFileSize;
        if (success)
        {
            bytes.resize(static_cast<size_t>(length.QuadPart));
            success = ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
        }
        CloseHandle(file);
        if (!success) throw std::runtime_error("Cannot read a complete terrain file (maximum 8 MB).");
        Map map = Parse(std::move(bytes));
        map.sourceSize = static_cast<uint32_t>(map.bytes.size());
        map.sourceCrc = Checksum(map.bytes);
        return map;
    }

    BYTE* image = nullptr;
    bool enabled = false, preparingLocal = false, launchFailed = false;
    std::unique_ptr<Map> selected;
    std::wstring lastDirectory;
    typedef void (__thiscall* NativeVoid)(void*);
    typedef int (__thiscall* NativeInit)(void*);
    typedef int (__thiscall* NativeLaunch)(void*, const char*);
    typedef void (__thiscall* NativePrepareTerrain)(void*, const void*);
    NativeInit originalCreatePage = nullptr;
    NativeInit originalCreateGameControls = nullptr;
    NativeVoid originalPrepareLocal = nullptr, originalGenerate = nullptr;
    NativePrepareTerrain originalPrepareTerrain = nullptr;
    NativeLaunch originalLaunch = nullptr;
    typedef void (__thiscall* NativeReceive)(void*, uint32_t, const void*, uint32_t);
    typedef void (__thiscall* NativeSendPlayer)(void*, uint32_t, uint32_t, const void*, uint32_t);
    NativeReceive originalHostReceive = nullptr, originalHostRoundReceive = nullptr;
    NativeSendPlayer originalSendReady = nullptr;
    NativeInit originalHostInit = nullptr;
    NativeVoid originalHostLaunch = nullptr, originalHostRoundLaunch = nullptr;
    NativeVoid originalHostGo = nullptr, originalHostRoundGo = nullptr;
    // The wrapper at 0x1e3c4 waits up to 200 ms for each recipient's ACK,
    // retrying seven times. Map checks must use its underlying Send once.
    using NativeDirectSend = HRESULT (__thiscall*)(void*, uint32_t, uint32_t, uint32_t, const void*, uint32_t);
    NativeDirectSend directSend = nullptr;
    using NativeDirectReceive = HRESULT (__thiscall*)(void*, uint32_t*, uint32_t*, uint32_t, void*, uint32_t*);
    NativeDirectReceive originalDirectReceive = nullptr;
    using NativeSequenceFilter = int (__thiscall*)(void*, uint32_t, uint32_t);
    NativeSequenceFilter originalSequenceFilter = nullptr;
    using NativeRunApplication = HRESULT (__thiscall*)(void*, uint32_t*, const void*, HANDLE);
    NativeRunApplication originalRunApplication = nullptr;
    using NativeSpawn = intptr_t (__cdecl*)(int, const char*, const char* const*, const char* const*);
    NativeSpawn originalSpawn = nullptr;
    void RefreshAll();

    constexpr uint32_t MapPacketType = 0x464b4d31; // FKM1, outside the native message range.
    constexpr uint32_t MapAckPacketType = 0x464b4d32;
    constexpr uint32_t MapTrailerMagic = 0x314d4b46; // ASCII FKM1.
    constexpr uint32_t MapProtocol = 10;
    constexpr unsigned NetworkSendBurst = 4;
    struct MapIdentity { uint32_t magic = MapTrailerMagic, revision = 0, size = 0, crc = 0; };
    struct MapHeader { uint32_t type = MapPacketType, version = MapProtocol; MapIdentity identity; };
    struct MapReference
    {
        MapHeader header;
        uint32_t sourceSize = 0, sourceCrc = 0, cavern = 0, pathLength = 0, waterLength = 0;
    };
    enum class MapResult : uint32_t { Ready, Missing, Different, Invalid, CannotPublish, Unsupported };
    struct MapReply { MapHeader header; MapResult result = MapResult::Ready; uint32_t sourceCrc = 0; };
    static_assert(sizeof(MapHeader) == 24 && sizeof(MapReference) == 44 && sizeof(MapReply) == 32, "network layout");
    constexpr size_t MaximumMapMessage = sizeof(MapReference) + MAX_PATH * 3 + 255;
    struct MapDelivery
    {
        bool awaiting = false, confirmed = false;
        DWORD sentAt = 0;
    };
    struct MapCheck
    {
        MapIdentity identity;
        uint32_t sourceCrc = 0;
        std::vector<BYTE> packet;
        std::vector<MapDelivery> deliveries;
        std::vector<uint32_t> players;
        size_t cursor = 0;
        bool broadcast = false;
        DWORD lastProgress = 0;
    };
    struct NetworkState
    {
        bool hosting = false, joining = false, generating = false, failed = false;
        uint32_t host = 0, revision = 0;
        HWND window = nullptr;
        MapIdentity incoming{}, complete{}, round{};
        std::unique_ptr<Map> remote, roundMap;
        std::array<std::vector<uint32_t>, 2> acknowledgements;
        void* transport = nullptr;
        uint32_t source = 0;
        NetworkSend send = nullptr;
        std::shared_ptr<const Map> outgoingMap;
        MapIdentity outgoing{};
        std::deque<MapCheck> checks;
        void* deferredGoObject = nullptr;
        NativeVoid deferredGo = nullptr;
        std::shared_ptr<const Map> startMap;
        MapIdentity startIdentity{};
        bool preparedGo = false;
        std::vector<BYTE> pendingGo;
        DWORD goStarted = 0;
        struct Control
        {
            void* object = nullptr;
            uint32_t source = 0;
            std::vector<uint32_t> players;
            size_t player = 0;
            std::vector<BYTE> packet;
            bool readyChannel = false;
            DWORD lastProgress = 0;
        };
        std::deque<Control> controls;
        std::vector<uint32_t> goRecipients;
    } network;
    UINT_PTR networkTimer = 0;
    void PumpNetworkQueues();
    void Trace(const char* phase, uint32_t a, uint32_t b);
    void StopNetworkTimer()
    {
        if (networkTimer) KillTimer(nullptr, networkTimer);
        networkTimer = 0;
    }
    void ResetNetwork()
    {
        StopNetworkTimer(); network = NetworkState{};
    }
    void CancelNetworkQueues()
    {
        StopNetworkTimer(); network.checks.clear(); network.controls.clear(); network.pendingGo.clear(); network.failed = true;
        network.deferredGo = nullptr; network.deferredGoObject = nullptr; network.startMap.reset(); network.preparedGo = false;
    }
    bool QueuedGo()
    {
        for (const auto& control : network.controls)
        {
            uint32_t type = 0;
            if (control.packet.size() >= 4) memcpy(&type, control.packet.data(), 4);
            if (type == 27) return true;
        }
        return false;
    }

    uint32_t PacketType(const void* packet, uint32_t length)
    {
        uint32_t type = 0;
        if (packet && length >= 4) memcpy(&type, packet, 4);
        return type;
    }
    constexpr uint32_t ChecksumTableEntry(uint32_t value)
    {
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
        return value;
    }
    template<size_t... Indices>
    constexpr std::array<uint32_t, sizeof...(Indices)> MakeChecksumTable(std::index_sequence<Indices...>)
    {
        return {{ ChecksumTableEntry(static_cast<uint32_t>(Indices))... }};
    }
    // A guarded function-local static depends on the CRT's TLS epoch. On XP,
    // loading a DLL can leave that epoch zero and skip the first initialization.
    // Constant initialization puts the complete table in the DLL image instead.
    constexpr auto ChecksumTable = MakeChecksumTable(std::make_index_sequence<256>{});
    static_assert(ChecksumTableEntry(1) == 0x77073096u, "CRC32 polynomial");
    uint32_t Checksum(const BYTE* bytes, size_t size)
    {
        uint32_t crc = ~0u;
        for (size_t i = 0; i < size; ++i) crc = ChecksumTable[(crc ^ bytes[i]) & 255] ^ (crc >> 8);
        return ~crc;
    }
    uint32_t Checksum(const std::vector<BYTE>& bytes) { return Checksum(bytes.data(), bytes.size()); }
    bool SameIdentity(const MapIdentity& a, const MapIdentity& b)
    {
        return a.magic == MapTrailerMagic && b.magic == MapTrailerMagic &&
            a.revision == b.revision && a.size == b.size && a.crc == b.crc;
    }
    const Map* PreviewMap() { return network.joining ? network.remote.get() : selected.get(); }
    const Map* NetworkMap() { return network.round.size ? network.roundMap.get() : nullptr; }

    LRESULT CALLBACK NetworkWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR)
    {
        // Native first-match/results Go both use timer 8 for their 30 s timeout.
        // A cancelled native start must never be sent later by our queue.
        if (message == WM_TIMER && wparam == 8 && network.window == window && (!network.pendingGo.empty() || !network.controls.empty())) CancelNetworkQueues();
        if (message == WM_NCDESTROY)
        {
            if (network.window == window) { ResetNetwork(); RefreshAll(); }
            RemoveWindowSubclass(window, NetworkWindowProc, id);
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }
    void WatchNetworkWindow(void* object)
    {
        if (!object) return;
        HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 28);
        if (window && IsWindow(window))
        {
            network.window = window;
            SetWindowSubclass(window, NetworkWindowProc, 0x464b4d31, 0);
        }
    }
    void SelectNetworkHost(uint32_t host)
    {
        if (!network.joining || network.host != host)
        {
            const HWND window = network.window;
            ResetNetwork();
            network.window = window;
            network.joining = true;
            network.host = host;
            Trace("joiner host selected", host, *reinterpret_cast<uint32_t*>(image + 0x1892ac));
            Trace("transport mode", *reinterpret_cast<uint32_t*>(image + 0x188b14), 0);
            Trace("NAT module loaded", GetModuleHandleW(L"fkWorm2NAT.dll") != nullptr, 0);
            RefreshAll();
        }
    }

    std::wstring GameDirectory()
    {
        wchar_t path[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (!length || length >= MAX_PATH) return {};
        wchar_t* slash = wcsrchr(path, L'\\');
        if (!slash) return {};
        *slash = 0;
        return path;
    }
    std::wstring ImportRoot() { return GameDirectory() + L"\\Levels\\Import"; }
    bool ImportFolderExists()
    {
        const DWORD attributes = GetFileAttributesW(ImportRoot().c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    bool ValidRelativePath(const std::wstring& path)
    {
        if (path.empty() || path.size() >= MAX_PATH || path.back() == L'\\' ||
            path.find_first_of(L"/:<>\"|?*") != std::wstring::npos) return false;
        size_t start = 0;
        while (start < path.size())
        {
            const size_t slash = path.find(L'\\', start);
            const auto part = path.substr(start, slash == std::wstring::npos ? slash : slash - start);
            if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ' ||
                std::any_of(part.begin(), part.end(), [](wchar_t c) { return c < 32; })) return false;
            if (slash == std::wstring::npos) break;
            start = slash + 1;
        }
        return path.size() > 4 && !_wcsicmp(path.c_str() + path.size() - 4, L".dat");
    }
    std::wstring ImportedPath(const std::wstring& relative)
    {
        if (!ValidRelativePath(relative)) throw std::runtime_error(ImportFolderErrorText());
        const auto root = ImportRoot();
        const auto full = root + L"\\" + relative;
        if (full.size() >= MAX_PATH) throw std::runtime_error("The imported map path is too long.");
        // Do not follow junctions or symbolic links out of the import tree.
        for (size_t end = GameDirectory().size() + 1; end <= full.size(); ++end)
        {
            if (end != full.size() && full[end] != L'\\') continue;
            const DWORD attributes = GetFileAttributesW(full.substr(0, end).c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("Imported map folders cannot use junctions or symbolic links.");
        }
        return full;
    }
    std::wstring RelativeImportPath(const wchar_t* path)
    {
        wchar_t full[MAX_PATH]{};
        const DWORD length = GetFullPathNameW(path, MAX_PATH, full, nullptr);
        const auto prefix = ImportRoot() + L"\\";
        if (!length || length >= MAX_PATH || length <= prefix.size() || _wcsnicmp(full, prefix.c_str(), prefix.size()))
            throw std::runtime_error(ImportFolderErrorText());
        std::wstring relative(full + prefix.size()); ImportedPath(relative); return relative;
    }
    Map LoadImported(const std::wstring& relative)
    {
        Map map = Load(ImportedPath(relative).c_str()); map.relativePath = relative; return map;
    }
    std::string EncodePath(const std::wstring& path)
    {
        if (!ValidRelativePath(path)) throw std::runtime_error("Invalid relative map path.");
        const int count = WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0, nullptr, nullptr);
        if (!count || count > MAX_PATH * 3) throw std::runtime_error("Invalid map path encoding.");
        std::string bytes(count, '\0');
        WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), &bytes[0], count, nullptr, nullptr);
        return bytes;
    }
    std::wstring DecodePath(const std::string& bytes)
    {
        if (bytes.empty() || bytes.size() > MAX_PATH * 3) throw std::runtime_error("Invalid map path encoding.");
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (!count || count >= MAX_PATH) throw std::runtime_error("Invalid map path encoding.");
        std::wstring path(count, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), &path[0], count);
        // XP can drop invalid UTF-8. Require a byte-for-byte round trip too.
        if (EncodePath(path) != bytes) throw std::runtime_error("Invalid map path encoding.");
        return path;
    }
    bool traceEnabled = true;
    std::wstring traceGamePath, traceMirrorPath;
    DWORD traceGameError = ERROR_SUCCESS;
    DWORD AppendTrace(const std::wstring& path, const char* line, DWORD length)
    {
        if (path.empty()) return ERROR_PATH_NOT_FOUND;
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return GetLastError();
        DWORD written = 0;
        const BOOL success = ::WriteFile(file, line, length, &written, nullptr);
        const DWORD error = success ? (written == length ? ERROR_SUCCESS : ERROR_WRITE_FAULT) : GetLastError();
        CloseHandle(file); return error;
    }
    void TraceText(const char* message)
    {
        if (!traceEnabled) return;
        try
        {
            SYSTEMTIME time{}; GetSystemTime(&time);
            char line[2048]{};
            const int length = sprintf_s(line, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ pid=%lu tick=%lu %s\r\n",
                time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                GetCurrentProcessId(), GetTickCount(), message);
            if (length <= 0) return;
            OutputDebugStringA(line);
            if (traceGamePath.empty()) traceGamePath = GameDirectory() + L"\\Data\\fkSettings-map-network.log";
            if (traceMirrorPath.empty())
            {
                wchar_t temporary[MAX_PATH]{};
                const DWORD count = GetTempPathW(MAX_PATH, temporary);
                if (count && count < MAX_PATH) traceMirrorPath = std::wstring(temporary) + L"fkSettings-map-network.log";
            }
            const DWORD error = AppendTrace(traceGamePath, line, static_cast<DWORD>(length));
            AppendTrace(traceMirrorPath, line, static_cast<DWORD>(length));
            if (error && error != traceGameError)
            {
                char failure[160]{};
                const int count = sprintf_s(failure, "pid=%lu Data log append failed error=%lu; using TEMP log\r\n", GetCurrentProcessId(), error);
                if (count > 0) { OutputDebugStringA(failure); AppendTrace(traceMirrorPath, failure, static_cast<DWORD>(count)); }
            }
            traceGameError = error;
        }
        catch (...) { } // Logging must never alter network control flow.
    }
    void Trace(const char* phase, uint32_t a = 0, uint32_t b = 0)
    {
        if (!traceEnabled) return;
        char line[192]{};
        if (sprintf_s(line, "%s %08lx %08lx", phase, static_cast<unsigned long>(a), static_cast<unsigned long>(b)) > 0) TraceText(line);
    }
    void TracePath(const char* label, const wchar_t* path)
    {
        if (!traceEnabled) return;
        char converted[MAX_PATH * 3 + 1]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, converted, sizeof(converted), nullptr, nullptr)) return;
        char line[MAX_PATH * 3 + 80]{};
        if (sprintf_s(line, "%s %s", label, converted) > 0) TraceText(line);
    }
    void TraceMap(const char* phase, uint32_t peer, uint32_t other, const MapHeader& header, uint32_t length, uint32_t sequence = 0)
    {
        if (!traceEnabled) return;
        char line[384]{};
        if (sprintf_s(line, "%s peer=%08lx other=%08lx type=%08lx ver=%lu rev=%08lx wire=%08lx seq=%08lx crc=%08lx",
            phase, static_cast<unsigned long>(peer), static_cast<unsigned long>(other), static_cast<unsigned long>(header.type),
            static_cast<unsigned long>(header.version), static_cast<unsigned long>(header.identity.revision),
            static_cast<unsigned long>(length),
            static_cast<unsigned long>(sequence), static_cast<unsigned long>(header.identity.crc)) > 0) TraceText(line);
    }
    bool ReadMapHeader(const void* packet, uint32_t length, MapHeader& header)
    {
        if (!packet || length < sizeof(header)) return false;
        memcpy(&header, packet, sizeof(header));
        return header.type == MapPacketType || header.type == MapAckPacketType;
    }

    HRESULT __fastcall DirectReceive(void* object, void*, uint32_t* source, uint32_t* target, uint32_t flags, void* packet, uint32_t* length)
    {
        const uint32_t capacity = length ? *length : 0;
        const HRESULT result = originalDirectReceive(object, source, target, flags, packet, length);
        if (result == S_OK && source && target && length && *length <= capacity)
        {
            MapHeader header; uint32_t sequence = 0;
            const BYTE* payload = static_cast<const BYTE*>(packet); uint32_t size = *length;
            if (!ReadMapHeader(payload, size, header) && payload && size >= 4)
            {
                memcpy(&sequence, payload, 4); payload += 4; size -= 4;
            }
            if (ReadMapHeader(payload, size, header))
            {
                TraceMap("map wire received", *source, *target, header, *length, sequence);
            }
        }
        return result;
    }
    void TraceSequenceFilter(uint32_t sender, uint32_t sequence, int result, const BYTE* caller)
    {
        // This caller has the current reliable packet in the native buffer.
        // Keep the original filter result and sequence bookkeeping unchanged.
        if (!traceEnabled || !result || caller != image + 0x12cbd) return;
        MapHeader header;
        uint32_t bufferedSequence = 0; memcpy(&bufferedSequence, image + 0x1563c0, 4);
        if (sequence == bufferedSequence && ReadMapHeader(image + 0x1563c4, sizeof(header), header))
            TraceMap("native map sequence dropped", sender, 0, header, 0, sequence);
    }
    int __fastcall SequenceFilter(void* object, void*, uint32_t sender, uint32_t sequence)
    {
        const int result = originalSequenceFilter(object, sender, sequence);
        TraceSequenceFilter(sender, sequence, result, static_cast<BYTE*>(_ReturnAddress()));
        return result;
    }

    bool Publish(const Map& map, const std::wstring& destination)
    {
        // Never change CTerrain's read-only flag or replace a partial file.
        wchar_t temporary[MAX_PATH]{};
        const size_t slash = destination.find_last_of(L"\\/");
        if (slash == std::wstring::npos || !GetTempFileNameW(destination.substr(0, slash).c_str(), L"fkm", 0, temporary)) return false;
        HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD written = 0;
        bool success = file != INVALID_HANDLE_VALUE;
        if (success)
        {
            success = ::WriteFile(file, map.bytes.data(), static_cast<DWORD>(map.bytes.size()), &written, nullptr) && written == map.bytes.size();
            if (success) success = FlushFileBuffers(file) != FALSE;
            if (!CloseHandle(file)) success = false;
        }
        if (success) success = MoveFileExW(temporary, destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!success) DeleteFileW(temporary);
        return success;
    }

    bool TryPacketSend(void* object, uint32_t source, uint32_t target, const void* packet, uint32_t length, bool readyChannel)
    {
        if (!object || !directSend) return false;
        BYTE* channel = static_cast<BYTE*>(object) + (readyChannel ? 0x128c8 : 0x12d50);
        const bool reliable = *reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0x120d4) == 1;
        // Match the native reliable envelope and advance its shared sequence
        // only after Send succeeds. Never enter its ACK polling/retry loop.
        uint32_t& next = *reinterpret_cast<uint32_t*>(channel + 0x438);
        const uint32_t sequence = next ? next : 1;
        std::array<BYTE, 4 + MaximumMapMessage> envelope{};
        if (length > envelope.size() - 4) return false;
        if (reliable) { memcpy(envelope.data(), &sequence, 4); memcpy(envelope.data() + 4, packet, length); }
        MapHeader header;
        const bool custom = ReadMapHeader(packet, length, header);
        // Limit repeated busy diagnostics while retaining an entry before each
        // successful selection and each application-level retry. A blocked Send
        // then leaves a useful final entry instead of an unexplained silence.
        static MapHeader last{}; static uint32_t lastTarget = 0; static DWORD lastTrace = 0; static bool lastFailed = false;
        const DWORD started = GetTickCount();
        const bool report = custom && (!lastFailed || lastTarget != target || memcmp(&last, &header, sizeof(last)) || started - lastTrace >= 5000);
        if (report)
        {
            TraceMap("map Send enter", target, source, header, length + (reliable ? 4 : 0), reliable ? sequence : 0);
            lastTrace = started;
        }
        const HRESULT result = directSend(channel, source, target, 1, reliable ? envelope.data() : packet, length + (reliable ? 4 : 0));
        if (report) Trace("map Send returned", static_cast<uint32_t>(result), GetTickCount() - started);
        if (PacketType(packet, length) == 14) Trace("start Send returned", static_cast<uint32_t>(result), target);
        if (custom) { last = header; lastTarget = target; lastFailed = result != S_OK; }
        if (result != S_OK) return false;
        if (reliable) next = sequence + 1;
        return true;
    }
    bool TryMapSend(void* object, uint32_t source, uint32_t target, const void* packet, uint32_t length)
    { return TryPacketSend(object, source, target, packet, length, false); }
    using ControlSendAttempt = bool (*)(void*, uint32_t, uint32_t, const void*, uint32_t, bool);
    ControlSendAttempt controlSendAttempt = TryPacketSend;
    using MapSendAttempt = bool (*)(void*, uint32_t, uint32_t, const void*, uint32_t);
    MapSendAttempt mapSendAttempt = TryMapSend;
    void CALLBACK NetworkTick(HWND, UINT, UINT_PTR timer, DWORD)
    {
        if (timer != networkTimer) return;
        try { PumpNetworkQueues(); }
        catch (const std::exception&)
        {
            CancelNetworkQueues();
            OutputDebugStringA("fkSettings: Local map check failed; network start withheld.\n");
        }
    }
    void StartNetworkTimer()
    {
        if (!networkTimer) networkTimer = SetTimer(nullptr, 0, 20, NetworkTick);
        if (!networkTimer) throw std::runtime_error("Cannot schedule the local map check.");
    }
    void QueueControl(void* object, uint32_t source, uint32_t target, bool broadcast, const void* packet, uint32_t length, bool readyChannel = false)
    {
        if (length > 256) throw std::runtime_error("Oversize terrain handshake packet.");
        if (network.controls.size() >= 64) throw std::runtime_error("Too many pending terrain handshake packets.");
        NetworkState::Control control;
        control.object = object; control.source = source; control.readyChannel = readyChannel; control.lastProgress = GetTickCount();
        control.packet.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
        if (broadcast)
        {
            for (size_t i = 0; i < 14; ++i)
            {
                uint32_t player = 0; memcpy(&player, image + 0x1a7698 + 0x108c + i * 4, 4);
                if (player && player != source && std::find(control.players.begin(), control.players.end(), player) == control.players.end()) control.players.push_back(player);
            }
        }
        else if (target && target != source) control.players.push_back(target);
        if (control.players.empty()) return;
        if (PacketType(packet, length) != MapAckPacketType) Trace("control queued", PacketType(packet, length), static_cast<uint32_t>(control.players.size()));
        network.controls.push_back(std::move(control)); StartNetworkTimer();
    }
    MapIdentity OutgoingMap(const Map* map)
    {
        if (network.outgoing.revision && ((!map && !network.outgoingMap) ||
            (map && network.outgoingMap && map->bytes == network.outgoingMap->bytes &&
                map->relativePath == network.outgoingMap->relativePath && map->sourceCrc == network.outgoingMap->sourceCrc &&
                map->sourceSize == network.outgoingMap->sourceSize))) return network.outgoing;
        network.outgoingMap = map ? std::make_shared<Map>(*map) : nullptr;
        network.outgoing = {};
        network.outgoing.revision = ++network.revision;
        if (!network.outgoing.revision) network.outgoing.revision = ++network.revision;
        if (map) { network.outgoing.size = static_cast<uint32_t>(map->bytes.size()); network.outgoing.crc = Checksum(map->bytes); }
        return network.outgoing;
    }
    std::vector<BYTE> ReferencePacket(const Map* map, const MapIdentity& identity)
    {
        MapReference reference; reference.header.identity = identity;
        std::string path;
        if (map)
        {
            if (map->relativePath.empty()) throw std::runtime_error("The map must be inside Levels\\Import.");
            path = EncodePath(map->relativePath);
            reference.sourceSize = map->sourceSize; reference.sourceCrc = map->sourceCrc;
            reference.cavern = map->cavern ? 1 : 0;
            reference.pathLength = static_cast<uint32_t>(path.size());
            reference.waterLength = static_cast<uint32_t>(map->waterPath.size());
        }
        std::vector<BYTE> packet(sizeof(reference)); memcpy(packet.data(), &reference, sizeof(reference));
        packet.insert(packet.end(), path.begin(), path.end());
        if (map) packet.insert(packet.end(), map->waterPath.begin(), map->waterPath.end());
        if (packet.size() > MaximumMapMessage) throw std::runtime_error("Map path is too long.");
        return packet;
    }
    void QueueMap(uint32_t target, bool broadcast)
    {
        const MapIdentity identity = network.outgoing;
        for (const auto& check : network.checks)
            if (SameIdentity(check.identity, identity) && ((broadcast && check.broadcast) ||
                (!broadcast && std::find(check.players.begin(), check.players.end(), target) != check.players.end())))
                { StartNetworkTimer(); return; }
        if (network.checks.size() >= 16) throw std::runtime_error("Too many pending map checks.");
        MapCheck check;
        check.identity = identity; check.broadcast = broadcast; check.lastProgress = GetTickCount();
        check.sourceCrc = network.outgoingMap ? network.outgoingMap->sourceCrc : 0;
        check.packet = ReferencePacket(network.outgoingMap.get(), identity);
        if (network.outgoingMap) TracePath("host local map", network.outgoingMap->relativePath.c_str());
        if (broadcast)
        {
            for (size_t i = 0; i < 14; ++i)
            {
                uint32_t player = 0; memcpy(&player, image + 0x1a7698 + 0x108c + i * 4, 4);
                if (player && player != network.source && std::find(check.players.begin(), check.players.end(), player) == check.players.end())
                    check.players.push_back(player);
            }
        }
        else if (target && target != network.source) check.players.push_back(target);
        if (check.players.empty()) return;
        check.deliveries.resize(check.players.size());
        network.checks.push_back(std::move(check)); StartNetworkTimer();
    }
    void BeginHostGo(void* object, NativeVoid handler)
    {
        Trace("Go clicked", (enabled ? 1u : 0u) | (network.hosting ? 2u : 0u) | (selected ? 4u : 0u), network.outgoing.revision);
        if (network.deferredGo || !network.pendingGo.empty() || QueuedGo()) return;
        if (!enabled || !network.hosting || !selected) { handler(object); return; }
        try
        {
            WatchNetworkWindow(object);
            network.failed = false;
            network.startMap = std::make_shared<Map>(*selected);
            if (!Publish(*network.startMap, GameDirectory() + L"\\Data\\land.dat"))
            {
                network.failed = true; network.startMap.reset();
                MessageBoxW(nullptr, L"Error saving Data\\land.dat",
                    ImportCaption().c_str(), MB_OK | MB_ICONERROR); return;
            }
            network.startIdentity = OutgoingMap(network.startMap.get());
            // Recheck installed files for every start, including retries and
            // subsequent rounds. Delayed acknowledgements cannot release Go.
            network.startIdentity.revision = ++network.revision;
            if (!network.startIdentity.revision) network.startIdentity.revision = ++network.revision;
            network.outgoing = network.startIdentity;
            network.checks.clear();
            network.transport = image + 0x176a40;
            memcpy(&network.source, image + 0x1892ac, 4);
            QueueMap(0, true);
            network.deferredGoObject = object; network.deferredGo = handler;
            Trace("Go waiting for map", network.startIdentity.revision, network.startIdentity.size);
            StartNetworkTimer();
        }
        catch (const std::exception&) { Trace("Go preparation failed"); CancelNetworkQueues(); }
    }
    void __fastcall HostGo(void* object, void*) { BeginHostGo(object, originalHostGo); }
    void __fastcall HostRoundGo(void* object, void*) { BeginHostGo(object, originalHostRoundGo); }
    bool ReceiveMapAck(uint32_t sender, const void* packet, uint32_t length)
    {
        if (!network.hosting || length != sizeof(MapReply)) return false;
        MapReply reply; memcpy(&reply, packet, sizeof(reply));
        if (reply.header.type != MapAckPacketType || reply.header.version != MapProtocol ||
            static_cast<uint32_t>(reply.result) > static_cast<uint32_t>(MapResult::Unsupported)) return false;
        for (auto& check : network.checks)
        {
            if (!SameIdentity(reply.header.identity, check.identity) || reply.sourceCrc != check.sourceCrc) continue;
            const auto found = std::find(check.players.begin(), check.players.end(), sender);
            if (found == check.players.end()) continue;
            auto& delivery = check.deliveries[found - check.players.begin()];
            if (!delivery.awaiting || delivery.confirmed) return false;
            if (reply.result != MapResult::Ready)
            {
                network.failed = true;
                Trace("local map check failed", sender, static_cast<uint32_t>(reply.result));
            }
            else { delivery.confirmed = true; check.lastProgress = GetTickCount(); }
            TraceMap("map reply accepted", sender, network.source, reply.header, length);
            return true;
        }
        return false;
    }
    void AdvanceMapCheck()
    {
        if (network.checks.empty() || network.failed) return;
        const auto& check = network.checks.front();
        if (!std::all_of(check.deliveries.begin(), check.deliveries.end(), [](const MapDelivery& d) { return d.confirmed; })) return;
        Trace("local map confirmed", check.identity.revision, check.identity.crc);
        network.checks.pop_front();
        if (!network.checks.empty()) network.checks.front().lastProgress = GetTickCount();
        else if (!network.pendingGo.empty()) network.goStarted = GetTickCount();
    }
    void PumpMapChecks()
    {
        const DWORD started = GetTickCount();
        for (unsigned attempt = 0; attempt < NetworkSendBurst; ++attempt)
        {
            if (network.failed) { CancelNetworkQueues(); return; }
            AdvanceMapCheck();
            if (network.checks.empty()) break;
            auto& check = network.checks.front();
            if (GetTickCount() - check.lastProgress >= 15000)
            {
                Trace("map confirmation timeout", check.identity.revision, 0); CancelNetworkQueues(); return;
            }
            size_t player = check.players.size();
            for (size_t i = 0; i < check.players.size(); ++i)
            {
                const size_t slot = (check.cursor + i) % check.players.size();
                const auto& delivery = check.deliveries[slot];
                if (delivery.confirmed || (delivery.awaiting && GetTickCount() - delivery.sentAt < 1000)) continue;
                player = slot; break;
            }
            if (player == check.players.size()) break;
            check.cursor = (player + 1) % check.players.size();
            auto& delivery = check.deliveries[player];
            const bool awaiting = delivery.awaiting; const DWORD previousSent = delivery.sentAt;
            delivery.awaiting = true; delivery.sentAt = GetTickCount();
            if (!mapSendAttempt(network.transport, network.source, check.players[player], check.packet.data(), static_cast<uint32_t>(check.packet.size())))
            { delivery.awaiting = awaiting; delivery.sentAt = previousSent; return; }
            if (network.failed) { CancelNetworkQueues(); return; }
            AdvanceMapCheck();
            if (GetTickCount() - started >= 5) break;
        }
        if (network.checks.empty() && !network.deferredGo && network.pendingGo.empty() && network.controls.empty()) StopNetworkTimer();
    }
    void PumpNetworkQueues()
    {
        // Bound each burst and keep all native state on the frontend UI thread.
        if (!network.hosting && !network.joining) { CancelNetworkQueues(); return; }
        if (network.checks.empty() && (!network.pendingGo.empty() || QueuedGo()) && GetTickCount() - network.goStarted >= 25000) { Trace("Go timeout"); CancelNetworkQueues(); return; }
        if (!network.checks.empty())
        {
            PumpMapChecks();
            return;
        }
        if (network.deferredGo)
        {
            // Native Go disables all frontend windows and starts its 30 s
            // timer. Enter it only after the installed maps are confirmed.
            const auto handler = network.deferredGo; void* object = network.deferredGoObject;
            network.deferredGo = nullptr; network.deferredGoObject = nullptr; network.preparedGo = true;
            network.goStarted = GetTickCount();
            network.roundMap.reset(new Map(*network.startMap)); network.round = network.startIdentity;
            Trace("native Go enter", network.round.revision, network.round.crc);
            handler(object);
            Trace("native Go returned", network.round.revision, network.round.crc);
            network.preparedGo = false; network.startMap.reset();
            if (network.checks.empty() && network.pendingGo.empty() && network.controls.empty()) StopNetworkTimer();
            return;
        }
        // Native Go and its map marker must follow local map confirmation.
        auto go = std::move(network.pendingGo); network.pendingGo.clear();
        if (!go.empty()) { network.goStarted = GetTickCount(); QueueControl(network.transport, network.source, 0, true, go.data(), static_cast<uint32_t>(go.size())); return; }
        const DWORD controlStarted = GetTickCount();
        for (unsigned attempt = 0; attempt < NetworkSendBurst && !network.controls.empty(); ++attempt)
        {
            auto& control = network.controls.front();
            const uint32_t player = control.players[control.player];
            if (!controlSendAttempt(control.object, control.source, player, control.packet.data(), static_cast<uint32_t>(control.packet.size()), control.readyChannel))
            {
                if (GetTickCount() - control.lastProgress >= 15000) { Trace("control timeout", PacketType(control.packet.data(), static_cast<uint32_t>(control.packet.size())), player); CancelNetworkQueues(); }
                return;
            }
            const uint32_t type = PacketType(control.packet.data(), static_cast<uint32_t>(control.packet.size()));
            if (type == 27) network.goRecipients.push_back(player);
            if (type == MapAckPacketType)
            {
                MapHeader ack;
                if (ReadMapHeader(control.packet.data(), static_cast<uint32_t>(control.packet.size()), ack))
                    TraceMap("map reply sent", player, control.source, ack, static_cast<uint32_t>(control.packet.size()));
            }
            else Trace("control sent", type, player);
            control.lastProgress = GetTickCount();
            if (++control.player == control.players.size()) network.controls.pop_front();
            if (type != MapAckPacketType || GetTickCount() - controlStarted >= 5) break;
        }
        if (network.controls.empty()) StopNetworkTimer();
    }
    bool SubmitLaunchControls()
    {
        if (network.failed || !network.checks.empty() || !network.pendingGo.empty() || network.deferredGo) return false;
        const DWORD started = GetTickCount();
        while (!network.controls.empty() && !network.failed && GetTickCount() - started < 1000)
        {
            PumpNetworkQueues();
            MSG paint{};
            while (PeekMessageW(&paint, nullptr, WM_PAINT, WM_PAINT, PM_REMOVE)) DispatchMessageW(&paint);
            if (!network.controls.empty()) Sleep(20);
        }
        return !network.failed && network.controls.empty();
    }
    void SelectionChanged()
    {
        try
        {
            if (network.hosting && network.send)
            {
                if (network.deferredGo || !network.pendingGo.empty() || QueuedGo()) return; // The already queued round remains frozen.
                network.failed = false;
                OutgoingMap(selected.get());
                network.checks.clear(); // Coalesce rapid selection/style/water edits.
                QueueMap(0, true);
            }
        }
        catch (const std::exception&)
        { CancelNetworkQueues(); OutputDebugStringA("fkSettings: Local map preview check failed; retrying at Go.\n"); }
    }
    bool PublishNetworkMap()
    {
        const Map* map = NetworkMap();
        return !map || Publish(*map, GameDirectory() + L"\\Data\\land.dat");
    }
    bool ReadTrailer(const void* packet, uint32_t length, MapIdentity& identity)
    {
        if (!packet || length < sizeof(identity)) return false;
        memcpy(&identity, static_cast<const BYTE*>(packet) + length - sizeof(identity), sizeof(identity));
        return identity.magic == MapTrailerMagic && identity.revision && identity.size && identity.size <= MaximumFileSize;
    }
    void QueueMapAck(const MapReference& reference, MapResult result)
    {
        MapReply reply; reply.header = reference.header; reply.header.type = MapAckPacketType;
        reply.header.version = MapProtocol; reply.result = result; reply.sourceCrc = reference.sourceCrc;
        uint32_t source = 0; memcpy(&source, image + 0x1892ac, sizeof(source));
        if (!source || source == network.host) return;
        for (const auto& control : network.controls)
            if (control.packet.size() == sizeof(reply) && !memcmp(control.packet.data(), &reply, sizeof(reply))) return;
        QueueControl(image + 0x176a40, source, network.host, false, &reply, sizeof(reply), true);
    }
    MapResult ResolveReference(const void* packet, uint32_t length, MapReference& reference, std::unique_ptr<Map>& resolved)
    {
        if (length < sizeof(reference)) return MapResult::Invalid;
        memcpy(&reference, packet, sizeof(reference));
        const auto& identity = reference.header.identity;
        if (reference.header.type != MapPacketType || reference.header.version != MapProtocol) return MapResult::Unsupported;
        if (identity.magic != MapTrailerMagic || !identity.revision || identity.size > MaximumFileSize ||
            reference.sourceSize > MaximumFileSize || reference.cavern > 1 || reference.pathLength > MAX_PATH * 3 ||
            reference.waterLength > 255 || length != sizeof(reference) + reference.pathLength + reference.waterLength) return MapResult::Invalid;
        if (!identity.size)
            return !identity.crc && !reference.sourceSize && !reference.sourceCrc && !reference.cavern &&
                !reference.pathLength && !reference.waterLength ? MapResult::Ready : MapResult::Invalid;
        if (!reference.sourceSize || !reference.pathLength) return MapResult::Invalid;
        try
        {
            const char* data = static_cast<const char*>(packet) + sizeof(reference);
            const std::wstring relative = DecodePath(std::string(data, reference.pathLength));
            const std::wstring path = ImportedPath(relative);
            TracePath("local map requested", relative.c_str());
            const DWORD attributes = GetFileAttributesW(path.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
                return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND ? MapResult::Missing : MapResult::Invalid;
            auto map = std::unique_ptr<Map>(new Map(LoadImported(relative)));
            Trace("local source hash actual/expected", map->sourceCrc, reference.sourceCrc);
            Trace("local source size actual/expected", map->sourceSize, reference.sourceSize);
            if (map->sourceSize != reference.sourceSize || map->sourceCrc != reference.sourceCrc) return MapResult::Different;
            const std::string water(data + reference.pathLength, reference.waterLength);
            if (std::any_of(water.begin(), water.end(), [](unsigned char c) { return c < 32; })) return MapResult::Invalid;
            map->bytes.resize(map->waterOffset);
            map->bytes.push_back(static_cast<BYTE>(water.size()));
            map->bytes.insert(map->bytes.end(), water.begin(), water.end());
            map->waterPath = water; map->cavern = reference.cavern != 0;
            const uint32_t size = static_cast<uint32_t>(map->bytes.size());
            memcpy(map->bytes.data() + 4, &size, 4); memcpy(map->bytes.data() + 16, &reference.cavern, 4);
            const uint32_t crc = Checksum(map->bytes);
            Trace("local effective hash actual/expected", crc, identity.crc);
            if (size != identity.size || crc != identity.crc) return MapResult::Different;
            if (!Publish(*map, GameDirectory() + L"\\Data\\land.dat")) return MapResult::CannotPublish;
            resolved = std::move(map); return MapResult::Ready;
        }
        catch (const std::exception&) { return MapResult::Invalid; }
    }
    bool ReceiveNetwork(uint32_t sender, uint32_t host, const void* packet, uint32_t& length)
    {
        const uint32_t type = PacketType(packet, length);
        if (!enabled)
        {
            if (type == MapPacketType || type == MapAckPacketType) Trace("map handler disabled", sender, length);
            return type != MapPacketType && type != MapAckPacketType;
        }
        if (type == MapAckPacketType) { ReceiveMapAck(sender, packet, length); return false; }
        if (type == MapPacketType)
        {
            MapHeader header;
            if (!ReadMapHeader(packet, length, header)) { Trace("map header rejected", sender, length); return false; }
            TraceMap("map handler received", sender, host, header, length);
            if (!host || sender != host) { TraceMap("map sender rejected", sender, host, header, length); return false; }
            SelectNetworkHost(host);
            MapReference reference;
            if (length < sizeof(reference)) return false;
            if (header.identity.magic != MapTrailerMagic || !header.identity.revision || header.version != MapProtocol) return false;
            if (network.incoming.revision && static_cast<int32_t>(header.identity.revision - network.incoming.revision) < 0) return false;
            network.incoming = header.identity; network.complete = {}; network.remote.reset();
            network.failed = false;
            std::unique_ptr<Map> resolved;
            const auto result = ResolveReference(packet, length, reference, resolved);
            if (result == MapResult::Ready)
            {
                network.remote = std::move(resolved); network.complete = reference.header.identity;
                Trace("local map ready", network.complete.revision, network.complete.crc);
            }
            else
            {
                network.failed = true;
                Trace("local map rejected", reference.header.identity.revision, static_cast<uint32_t>(result));
            }
            QueueMapAck(reference, result); RefreshAll();
            return false; // Never expose custom packets to a native decoder.
        }
        if (type != 27 && type != 29 && type != 14) return true;
        if (!host || sender != host) return false;
        SelectNetworkHost(host);
        const uint32_t nativeSize = type == 14 ? 20 : 8;
        if (length < nativeSize) return false;
        MapIdentity identity;
        const bool imported = length >= nativeSize + sizeof(identity) && ReadTrailer(packet, length, identity);
        if (type != 14 && length != nativeSize && !imported) return false;
        if (type == 27)
        {
            network.round = {}; network.roundMap.reset();
            if (!imported)
            {
                network.remote.reset(); network.incoming = {}; network.complete = {};
                network.failed = false;
                RefreshAll(); return true;
            }
            if (!SameIdentity(identity, network.complete) || !network.remote) { Trace("Go missing map", identity.revision, network.complete.revision); return false; }
            network.failed = false;
            network.roundMap.reset(new Map(*network.remote)); network.round = identity;
        }
        if (imported)
        {
            if (!SameIdentity(identity, network.round) || !NetworkMap() || !PublishNetworkMap()) return false;
            Trace("control received", type, identity.revision);
            length -= sizeof(identity);
        }
        else if (network.round.size) return false;
        return true;
    }
    void SendNetwork(void* object, uint32_t source, uint32_t target, bool broadcast,
        const void* packet, uint32_t length, NetworkSend send)
    {
        const uint32_t type = PacketType(packet, length);
        if (!enabled) { send(object, source, target, broadcast, packet, length); return; }
        try
        {
            if (network.hosting || (broadcast && type == 27 && length == 8))
            { network.transport = object; network.source = source; network.send = send; }
            if (broadcast && type == 27 && length == 8)
            {
                network.hosting = true; network.joining = false;
                network.failed = false;
                network.acknowledgements = {};
                network.controls.clear(); network.pendingGo.clear();
                network.goRecipients.clear(); network.goStarted = GetTickCount();
                network.roundMap.reset(network.preparedGo && network.startMap ? new Map(*network.startMap) : (selected ? new Map(*selected) : nullptr));
                network.round = {};
                if (network.roundMap)
                {
                    network.round.size = static_cast<uint32_t>(network.roundMap->bytes.size());
                    if (!Publish(*network.roundMap, GameDirectory() + L"\\Data\\land.dat")) { network.failed = true; return; }
                    network.round = network.preparedGo ? network.startIdentity : OutgoingMap(network.roundMap.get());
                    Trace("Go frozen local map", network.round.revision, network.round.crc);
                    if (!network.preparedGo)
                    {
                        network.round.revision = ++network.revision;
                        if (!network.round.revision) network.round.revision = ++network.revision;
                        network.outgoing = network.round; network.checks.clear();
                        QueueMap(0, true);
                    }
                }
                if (!network.checks.empty())
                {
                    network.pendingGo.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
                    network.goStarted = GetTickCount();
                    if (network.round.size)
                    {
                        const BYTE* trailer = reinterpret_cast<const BYTE*>(&network.round);
                        network.pendingGo.insert(network.pendingGo.end(), trailer, trailer + sizeof(network.round));
                    }
                    return;
                }
            }
            if (network.hosting)
            {
                if (network.round.size && (type == 27 || type == 29 || type == 14))
                {
                    if (network.failed || !network.pendingGo.empty() || QueuedGo()) { Trace("control withheld", type, network.round.revision); return; }
                    std::vector<BYTE> extended(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
                    const BYTE* trailer = reinterpret_cast<const BYTE*>(&network.round);
                    extended.insert(extended.end(), trailer, trailer + sizeof(network.round));
                    QueueControl(object, source, target, broadcast, extended.data(), static_cast<uint32_t>(extended.size()));
                    // Mode 1 closes the lobby DirectPlay session before its
                    // RunApplication call. The engine hook is too late to send
                    // packet 14: submit it here while the connection is open.
                    if (type == 14 && *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1)
                    {
                        Trace("IPX start before lobby close", network.round.revision, network.round.crc);
                        if (!SubmitLaunchControls())
                        {
                            Trace("IPX start submission failed", network.round.revision, static_cast<uint32_t>(network.controls.size()));
                            CancelNetworkQueues();
                        }
                        else Trace("IPX start submitted before lobby close", network.round.revision, network.round.crc);
                    }
                    return;
                }
            }
            if (network.round.size) Trace("native send enter", type, length);
            send(object, source, target, broadcast, packet, length);
            if (network.round.size) Trace("native send returned", type, length);
            // A late join first needs the native snapshot to establish its host ID.
            if (!broadcast && type == 5 && network.hosting)
            {
                OutgoingMap(network.deferredGo && network.startMap ? network.startMap.get() : selected.get());
                QueueMap(target, false);
            }
        }
        catch (const std::exception&)
        {
            if (type == 27 || type == 29 || type == 14) CancelNetworkQueues();
            OutputDebugStringA("fkSettings: Local map check failed; network start withheld.\n");
        }
    }
    void __fastcall SendReady(void* object, void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
    {
        const uint32_t type = PacketType(packet, length);
        if (network.joining && network.round.size && (type == 28 || type == 30))
        {
            if (length != 8 || target != network.host || network.failed || !PublishNetworkMap()) { Trace("ready withheld", type, target); return; }
            std::array<BYTE, 8 + sizeof(MapIdentity)> extended{};
            memcpy(extended.data(), packet, 8); memcpy(extended.data() + 8, &network.round, sizeof(MapIdentity));
            try { QueueControl(object, source, target, false, extended.data(), static_cast<uint32_t>(extended.size()), true); }
            catch (const std::exception&) { CancelNetworkQueues(); }
            return;
        }
        originalSendReady(object, source, target, packet, length);
    }
    bool AcceptReady(uint32_t sender, const void* packet, uint32_t length)
    {
        const uint32_t type = PacketType(packet, length);
        if (!network.hosting || !network.round.size || (type != 28 && type != 30)) return true;
        if (!network.pendingGo.empty() || network.failed || std::find(network.goRecipients.begin(), network.goRecipients.end(), sender) == network.goRecipients.end()) return false;
        MapIdentity identity;
        uint32_t claimed = 0;
        if (length != 8 + sizeof(identity) || !ReadTrailer(packet, length, identity) || !SameIdentity(identity, network.round)) return false;
        memcpy(&claimed, static_cast<const BYTE*>(packet) + 4, 4);
        if (claimed != sender || sender == network.source) return false;
        uint32_t players = 0;
        memcpy(&players, image + 0x1a7698 + 0x10d4, 4);
        if (!players) return false;
        bool member = false;
        for (size_t i = 0; i < 14; ++i)
        {
            uint32_t player = 0; memcpy(&player, image + 0x1a7698 + 0x108c + i * 4, 4);
            if (player == sender) member = true;
        }
        if (!member) return false;
        auto& acknowledged = network.acknowledgements[type == 30];
        if (type == 30 && std::find(network.acknowledgements[0].begin(), network.acknowledgements[0].end(), sender) == network.acknowledgements[0].end()) return false;
        if (acknowledged.size() >= 14 || std::find(acknowledged.begin(), acknowledged.end(), sender) != acknowledged.end()) return false;
        acknowledged.push_back(sender); Trace("ready accepted", type, sender); return true;
    }
    void ReceiveHost(void* object, uint32_t sender, const void* packet, uint32_t length, NativeReceive original)
    {
        WatchNetworkWindow(object);
        const bool reliable = *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1;
        const uint32_t payload = reliable ? (length >= 4 ? length - 4 : 0) : length;
        if (PacketType(packet, payload) == MapAckPacketType) { ReceiveMapAck(sender, packet, payload); return; }
        if (PacketType(packet, payload) == MapPacketType || !AcceptReady(sender, packet, payload)) return;
        if (payload < 4) return;
        original(object, sender, packet, length);
    }
    void __fastcall HostReceive(void* object, void*, uint32_t sender, const void* packet, uint32_t length)
    { ReceiveHost(object, sender, packet, length, originalHostReceive); }
    void __fastcall HostRoundReceive(void* object, void*, uint32_t sender, const void* packet, uint32_t length)
    { ReceiveHost(object, sender, packet, length, originalHostRoundReceive); }
    int __fastcall HostInit(void* object, void*)
    {
        ResetNetwork(); network.hosting = true;
        const int result = originalHostInit(object);
        if (result) WatchNetworkWindow(object);
        Trace("host lobby initialized", static_cast<uint32_t>(result), *reinterpret_cast<uint32_t*>(image + 0x188b14));
        Trace("NAT module loaded", GetModuleHandleW(L"fkWorm2NAT.dll") != nullptr);
        return result;
    }
    void HostLaunch(void* object, NativeVoid original)
    {
        WatchNetworkWindow(object);
        if (network.failed || !PublishNetworkMap())
        {
            MessageBoxW(nullptr, L"Error saving Data\\land.dat",
                ImportCaption().c_str(), MB_OK | MB_ICONERROR); return;
        }
        Trace("host setup enter", network.round.revision, network.round.crc);
        original(object);
        Trace("host setup exit");
    }
    void __fastcall LaunchHost(void* object, void*) { HostLaunch(object, originalHostLaunch); }
    void __fastcall LaunchHostRound(void* object, void*) { HostLaunch(object, originalHostRoundLaunch); }
    bool PrepareNetworkEngine()
    {
        if (NetworkMap())
        {
            // IPX packet 14 must already be submitted before native Close.
            // Other launch paths can still flush it here. A failed submission
            // must never reach the native infinite engine-event wait.
            try
            {
                if (SubmitLaunchControls() && PublishNetworkMap())
                    Trace("engine run enter", network.round.revision, network.round.crc);
                else network.failed = true;
            }
            catch (const std::exception&) { network.failed = true; }
            if (network.failed)
            { Trace("engine start withheld"); CancelNetworkQueues(); return false; }
        }
        return true;
    }
    HRESULT __fastcall RunApplication(void* object, void*, uint32_t* id, const void* connection, HANDLE event)
    {
        if (!PrepareNetworkEngine()) return E_ABORT;
        const HRESULT result = originalRunApplication(object, id, connection, event);
        if (NetworkMap()) Trace("engine run returned", static_cast<uint32_t>(result));
        return result;
    }
    intptr_t __cdecl Spawn(int mode, const char* path, const char* const* arguments, const char* const* environment)
    {
        const char* name = path;
        if (name) for (const char* p = path; *p; ++p) if (*p == '\\' || *p == '/') name = p + 1;
        const bool engine = name && !_stricmp(name, "worms2.exe");
        if (engine && !PrepareNetworkEngine()) return -1;
        const intptr_t result = originalSpawn(mode, path, arguments, environment);
        if (engine && NetworkMap()) Trace("engine spawn returned", static_cast<uint32_t>(result));
        return result;
    }

    const int HiddenControls[] = { 1018, 1012, 1014, 1016, 1033, 1000, 1057, 1028, 1019, 1025 };
    const int DisabledControls[] = { 1019, 1022, 1030, 1032, 1034, 1023, 1024 };
    struct ComboItem { std::wstring text; LPARAM data; };
    struct ComboState
    {
        std::vector<ComboItem> items;
        LRESULT selection = CB_ERR;
        bool enabled = true;
        bool hasStrings = true;
    };
    struct Page
    {
        HWND window, preview, importButton;
        HWND previewLabel = nullptr;
        HWND cancelButton = nullptr;
        HFONT previewFont = nullptr;
        bool active = false;
        std::array<bool, 10> visible{};
        std::array<bool, 7> enabled{};
        bool generateEnabled = true;
        ComboState style, water;
        std::array<LRESULT, 2> styleOptions{ CB_ERR, CB_ERR };
        LRESULT customWater = CB_ERR;
        ~Page() { if (previewFont) DeleteObject(previewFont); }
    };
    std::vector<Page*> pages;
    struct GamePreview { HWND window; RECT bounds; LONG style, extendedStyle; };
    std::vector<GamePreview> gamePreviews;

    RECT Units(HWND window, int x, int y, int width, int height)
    {
        RECT rect{ x, y, x + width, y + height };
        MapDialogRect(window, &rect);
        return rect;
    }
    void Position(HWND window, RECT rect)
    {
        SetWindowPos(window, HWND_TOP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE);
    }
    RECT MapRectangle(RECT bounds)
    {
        int width = bounds.right - bounds.left, height = MulDiv(width, 696, 1920);
        if (height > bounds.bottom - bounds.top)
        {
            height = bounds.bottom - bounds.top;
            width = MulDiv(height, 1920, 696);
        }
        const int x = bounds.left + (bounds.right - bounds.left - width) / 2;
        const int y = bounds.top + (bounds.bottom - bounds.top - height) / 2;
        return RECT{ x, y, x + width, y + height };
    }
    void RefreshGamePreview(const GamePreview& preview)
    {
        SetWindowLongW(preview.window, GWL_STYLE, PreviewMap() ?
            preview.style & ~(SS_SUNKEN | WS_BORDER | WS_DLGFRAME) : preview.style);
        SetWindowLongW(preview.window, GWL_EXSTYLE, PreviewMap() ?
            preview.extendedStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME) : preview.extendedStyle);
        SetWindowPos(preview.window, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        Position(preview.window, PreviewMap() ? MapRectangle(preview.bounds) : preview.bounds);
        InvalidateRect(GetParent(preview.window), &preview.bounds, TRUE);
        InvalidateRect(preview.window, nullptr, TRUE);
    }
    std::wstring ComboText(HWND combo, LRESULT index)
    {
        const LONG style = GetWindowLongW(combo, GWL_STYLE);
        if ((style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) && !(style & CBS_HASSTRINGS))
        {
            // Native water swatches store an index into the frontend's
            // CStringList, not a caption. Resolve that index without altering
            // the item data used by its owner-draw palette.
            const LRESULT item = SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0);
            if (!image || item < 0 || item >= 256) return {};
            BYTE* list = image + 0x1b5120;
            const int count = *reinterpret_cast<int*>(list + 12);
            if (item >= count || count < 0 || count > 256) return {};
            BYTE* node = *reinterpret_cast<BYTE**>(list + 4);
            for (LRESULT i = 0; i < item && node; ++i) node = *reinterpret_cast<BYTE**>(node);
            if (!node) return {};
            const char* text = *reinterpret_cast<const char**>(node + 8);
            if (!text) return {};
            size_t size = 0; while (size < 256 && text[size]) ++size;
            if (size == 256) return {};
            const int length = MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(size), nullptr, 0);
            std::wstring wide(length, L'\0');
            if (length) MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(size), &wide[0], length);
            return wide;
        }
        const LRESULT length = SendMessageW(combo, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0);
        if (length == CB_ERR) return {};
        std::vector<wchar_t> text(static_cast<size_t>(length) + 1);
        SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text.data()));
        return text.data();
    }
    void SaveCombo(HWND combo, ComboState& state)
    {
        state.enabled = IsWindowEnabled(combo) != FALSE;
        const LONG style = GetWindowLongW(combo, GWL_STYLE);
        state.hasStrings = !(style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) || (style & CBS_HASSTRINGS);
        state.selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT i = 0; i < count; ++i)
            state.items.push_back(ComboItem{ ComboText(combo, i), SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(i), 0) });
    }
    void RestoreCombo(HWND combo, ComboState& state)
    {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const auto& item : state.items)
        {
            const LRESULT index = SendMessageW(combo, CB_INSERTSTRING, static_cast<WPARAM>(-1),
                state.hasStrings ? reinterpret_cast<LPARAM>(item.text.c_str()) : item.data);
            SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), item.data);
        }
        SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(state.selection), 0);
        EnableWindow(combo, state.enabled);
        state.items.clear();
    }
    LRESULT FindCaption(HWND page, int control, unsigned stringId)
    {
        wchar_t text[256]{};
        const HINSTANCE resources = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(page, GWLP_HINSTANCE));
        if (!LoadStringW(resources, stringId, text, sizeof(text) / sizeof(text[0]))) return CB_ERR;
        HWND combo = GetDlgItem(page, control);
        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT i = 0; i < count; ++i)
            if (_wcsicmp(ComboText(combo, i).c_str(), text) == 0) return i;
        return CB_ERR;
    }
    LRESULT FindStyle(HWND page, unsigned stringId) { return FindCaption(page, LevelStyleId, stringId); }
    void Refresh(Page& page)
    {
        const Map* map = PreviewMap();
        const bool active = map != nullptr;
        for (size_t i = 0; i < page.visible.size(); ++i)
        {
            HWND child = GetDlgItem(page.window, HiddenControls[i]);
            if (active && !page.active) page.visible[i] = (GetWindowLongW(child, GWL_STYLE) & WS_VISIBLE) != 0;
            if (active || page.active) ShowWindow(child, active ? SW_HIDE : (page.visible[i] ? SW_SHOW : SW_HIDE));
        }
        for (size_t i = 0; i < page.enabled.size(); ++i)
        {
            HWND child = GetDlgItem(page.window, DisabledControls[i]);
            if (active && !page.active) page.enabled[i] = IsWindowEnabled(child) != FALSE;
            if (active || page.active) EnableWindow(child, active ? FALSE : page.enabled[i]);
        }
        HWND generate = GetDlgItem(page.window, GenerateId);
        if (active && !page.active) page.generateEnabled = IsWindowEnabled(generate) != FALSE;
        if (active || page.active) EnableWindow(generate, active ? !network.joining : page.generateEnabled);
        HWND style = GetDlgItem(page.window, LevelStyleId);
        if (active && !page.active)
        {
            SaveCombo(style, page.style);
            // Keep the native Open/Cavern captions, order and item data. Only
            // Random is unavailable for a premade map; never add new labels.
            const LRESULT random = FindStyle(page.window, 510);
            if (random != CB_ERR) SendMessageW(style, CB_DELETESTRING, static_cast<WPARAM>(random), 0);
            page.styleOptions = { FindStyle(page.window, 501), FindStyle(page.window, 502) };
        }
        if (active)
        {
            EnableWindow(style, !network.joining);
            SendMessageW(style, CB_SETCURSEL, static_cast<WPARAM>(page.styleOptions[map->cavern ? 1 : 0]), 0);
        }
        else if (page.active)
        {
            RestoreCombo(style, page.style);
        }
        HWND water = GetDlgItem(page.window, WaterId);
        if (active && !page.active)
        {
            SaveCombo(water, page.water);
            const LRESULT random = FindCaption(page.window, WaterId, 510);
            if (random != CB_ERR) SendMessageW(water, CB_DELETESTRING, static_cast<WPARAM>(random), 0);
        }
        if (active)
        {
            EnableWindow(water, !network.joining);
            if (page.customWater != CB_ERR)
                SendMessageW(water, CB_DELETESTRING, static_cast<WPARAM>(page.customWater), 0);
            page.customWater = CB_ERR;
            const std::wstring name = WaterName(*map);
            LRESULT index = CB_ERR;
            const LRESULT count = SendMessageW(water, CB_GETCOUNT, 0, 0);
            for (LRESULT i = 0; i < count; ++i)
                if (!name.empty() && _wcsicmp(ComboText(water, i).c_str(), name.c_str()) == 0) { index = i; break; }
            if (index == CB_ERR && !name.empty() && page.water.hasStrings)
            {
                // Show an unrecognised imported resource faithfully. Keep its
                // original path until the user chooses an installed colour.
                index = SendMessageW(water, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
                page.customWater = index;
            }
            SendMessageW(water, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
        }
        else if (page.active)
        {
            RestoreCombo(water, page.water);
            page.customWater = CB_ERR;
        }
        page.active = active;
        ShowWindow(page.preview, active ? SW_SHOW : SW_HIDE);
        ShowWindow(page.previewLabel, active ? SW_SHOW : SW_HIDE);
        ShowWindow(page.cancelButton, active ? SW_SHOW : SW_HIDE);
        const bool importAvailable = ImportFolderExists();
        ShowWindow(page.importButton, importAvailable ? SW_SHOW : SW_HIDE);
        EnableWindow(page.importButton, importAvailable && !network.joining);
        EnableWindow(page.cancelButton, !network.joining);
        RECT button{}, picture{}, edit{};
        GetWindowRect(GetDlgItem(page.window, 1057), &button);
        GetWindowRect(active ? page.preview : GetDlgItem(page.window, 1033), &picture);
        if (active) GetWindowRect(GetDlgItem(page.window, EditTerrainId), &edit);
        MapWindowPoints(nullptr, page.window, reinterpret_cast<POINT*>(&button), 2);
        MapWindowPoints(nullptr, page.window, reinterpret_cast<POINT*>(&picture), 2);
        if (active) MapWindowPoints(nullptr, page.window, reinterpret_cast<POINT*>(&edit), 2);
        const LONG width = button.right - button.left;
        // Mirror Invert terrain across the native preview, or centre Import and
        // Cancel beneath the colour preview. Use the native button's size.
        const LONG gap = Units(page.window, 0, 0, 10, 0).right;
        button.left = active ? (picture.left + picture.right - 2 * width - gap) / 2 : picture.left + picture.right - button.right;
        button.right = button.left + width;
        if (active)
        {
            button.top = edit.top; button.bottom = edit.bottom;
            OffsetRect(&button, 0, Units(page.window, 0, 5 + PreviewVerticalOffset, 0, 0).top);
        }
        Position(page.importButton, button);
        if (active)
        {
            OffsetRect(&button, width + gap, 0);
            Position(page.cancelButton, button);
        }
        InvalidateRect(page.preview, nullptr, TRUE);
    }
    void RefreshAll()
    {
        for (Page* page : pages) Refresh(*page);
        for (const auto& preview : gamePreviews) RefreshGamePreview(preview);
    }

    // The native thumbnail bitmap uses this colour for air (frontend RVA 0x84E2F).
    constexpr RGBQUAD PreviewAir = { 255, 160, 0, 0 };
    constexpr COLORREF PreviewBackground = RGB(PreviewAir.rgbRed, PreviewAir.rgbGreen, PreviewAir.rgbBlue);

    void PaintPreview(HWND window, HDC dc)
    {
        RECT rect{};
        GetClientRect(window, &rect);
        HBRUSH background = CreateSolidBrush(PreviewBackground);
        FillRect(dc, &rect, background);
        DeleteObject(background);
        const Map* map = PreviewMap();
        if (!map) return;
        struct { BITMAPINFOHEADER header; RGBQUAD colors[256]; } info{};
        info.header.biSize = sizeof(BITMAPINFOHEADER);
        info.header.biWidth = 1920;
        info.header.biHeight = -696; // IMG pixels are top-down.
        info.header.biPlanes = 1;
        info.header.biBitCount = 8;
        info.header.biClrUsed = 256;
        memcpy(info.colors, map->palette.data(), sizeof(info.colors));
        // Index zero is empty terrain. Override only the display palette.
        info.colors[0] = PreviewAir;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, 0, 0, rect.right, rect.bottom,
            0, 0, 1920, 696, map->pixels.data(), reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, SRCCOPY);
    }

    LRESULT CALLBACK PreviewProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR)
    {
        if (PreviewMap() && (message == WM_PAINT || message == WM_PRINTCLIENT))
        {
            PAINTSTRUCT paint{};
            HDC dc = message == WM_PAINT ? BeginPaint(window, &paint) : reinterpret_cast<HDC>(wparam);
            PaintPreview(window, dc);
            if (message == WM_PAINT) EndPaint(window, &paint);
            return 0;
        }
        if (message == WM_SHOWWINDOW && PreviewMap()) InvalidateRect(window, nullptr, TRUE);
        if (message == WM_NCDESTROY)
        {
            gamePreviews.erase(std::remove_if(gamePreviews.begin(), gamePreviews.end(),
                [window](const GamePreview& preview) { return preview.window == window; }), gamePreviews.end());
            RemoveWindowSubclass(window, PreviewProc, id);
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }

    bool AttachGamePreview(HWND page)
    {
        HWND window = GetDlgItem(page, 1262);
        if (!window) return false;
        for (const auto& preview : gamePreviews) if (preview.window == window) return true;
        if (!SetWindowSubclass(window, PreviewProc, 1, 0)) return false;
        RECT bounds{}; GetWindowRect(window, &bounds);
        MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&bounds), 2);
        gamePreviews.push_back(GamePreview{ window, bounds, GetWindowLongW(window, GWL_STYLE), GetWindowLongW(window, GWL_EXSTYLE) });
        RefreshGamePreview(gamePreviews.back());
        return true;
    }

    UINT_PTR CALLBACK ImportPickerHook(HWND window, UINT message, WPARAM, LPARAM lparam)
    {
        if (message == WM_NOTIFY)
        {
            const auto notification = reinterpret_cast<OFNOTIFYW*>(lparam);
            if (notification->hdr.code == CDN_FILEOK)
            {
                try { RelativeImportPath(notification->lpOFN->lpstrFile); }
                catch (const std::exception& error)
                {
                    ShowImportError(GetParent(window), error, notification->lpOFN->hwndOwner);
                    SetWindowLongPtrW(window, DWLP_MSGRESULT, 1); return TRUE;
                }
            }
        }
        return 0;
    }
    void Browse(HWND owner)
    {
        if (!ImportFolderExists()) { RefreshAll(); return; }
        wchar_t path[MAX_PATH]{};
        const std::wstring caption = ImportCaption(owner);
        std::wstring filter = L"Worms 2 " + ResourceCaption(TerrainStringId, L"Terrain", owner) + L" (*.dat)";
        filter.push_back(L'\0');
        filter.append(L"*.dat");
        filter.push_back(L'\0'); // c_str() adds the second terminating null.
        const std::wstring directory = lastDirectory.empty() ? ImportRoot() : lastDirectory;
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter = filter.c_str();
        dialog.lpstrFile = path;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrInitialDir = directory.c_str();
        dialog.lpstrTitle = caption.c_str();
        dialog.lpfnHook = ImportPickerHook;
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_ENABLEHOOK;
        if (!GetOpenFileNameW(&dialog)) return;
        try
        {
            std::unique_ptr<Map> next(new Map(LoadImported(RelativeImportPath(path))));
            if (!Publish(*next, GameDirectory() + L"\\Data\\land.dat"))
                throw std::runtime_error("Error saving Data\\land.dat");
            selected.swap(next);
            lastDirectory.assign(path, wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') - path : 0);
            launchFailed = false;
            RefreshAll();
            SelectionChanged();
        }
        catch (const std::exception& error) { ShowImportError(owner, error, owner); }
    }

    LRESULT CALLBACK PageProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR reference)
    {
        Page* page = reinterpret_cast<Page*>(reference);
        if (network.joining && message == WM_COMMAND && (LOWORD(wparam) == ImportId || LOWORD(wparam) == CancelId || LOWORD(wparam) == GenerateId ||
            LOWORD(wparam) == WaterId || LOWORD(wparam) == LevelStyleId)) return 0;
        if (selected && message == WM_COMMAND && LOWORD(wparam) == WaterId && HIWORD(wparam) == CBN_SELCHANGE)
        {
            HWND water = GetDlgItem(window, WaterId);
            const LRESULT index = SendMessageW(water, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR && index != page->customWater)
                ChangeWater(*selected, ComboText(water, index));
            RefreshAll();
            SelectionChanged();
            return 0; // Changing water never regenerates the imported terrain.
        }
        if (selected && message == WM_COMMAND && LOWORD(wparam) == LevelStyleId && HIWORD(wparam) == CBN_SELCHANGE)
        {
            const LRESULT style = SendDlgItemMessageW(window, LevelStyleId, CB_GETCURSEL, 0, 0);
            if (style != CB_ERR && (style == page->styleOptions[0] || style == page->styleOptions[1]))
            {
                selected->cavern = style == page->styleOptions[1];
                const uint32_t border = selected->cavern ? 1 : 0;
                memcpy(selected->bytes.data() + 16, &border, sizeof(border));
                RefreshAll();
                SelectionChanged();
            }
            return 0; // Imported styles never enter the native generation handler.
        }
        if (message == WM_COMMAND && HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == ImportId) { Browse(window); return 0; }
        if (message == WM_COMMAND && HIWORD(wparam) == BN_CLICKED && (LOWORD(wparam) == GenerateId || LOWORD(wparam) == CancelId))
        {
            if (selected)
            {
                selected.reset();
                launchFailed = false;
                RefreshAll();
                SelectionChanged();
            }
            if (LOWORD(wparam) == CancelId) return 0;
            // The native Generate command now sees the restored controls and
            // creates normal terrain with no imported-map generation bypass.
        }
        if (message == WM_NCDESTROY)
        {
            pages.erase(std::remove(pages.begin(), pages.end(), page), pages.end());
            RemoveWindowSubclass(window, PageProc, id);
            delete page;
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }

    bool Attach(HWND window)
    {
        for (Page* existing : pages) if (existing->window == window) return true;
        std::unique_ptr<Page> page(new Page{ window, nullptr, nullptr });
        auto create = [&](const wchar_t* type, const wchar_t* text, DWORD style, int id, RECT rect) {
            HWND child = CreateWindowExW(0, type, text, WS_CHILD | style, rect.left, rect.top,
                rect.right - rect.left, rect.bottom - rect.top, window, reinterpret_cast<HMENU>(id), GetModuleHandleW(nullptr), nullptr);
            if (child) SendMessageW(child, WM_SETFONT, SendMessageW(window, WM_GETFONT, 0, 0), TRUE);
            return child;
        };
        // The frontend can give its style labels a different font from the page.
        HFONT font = reinterpret_cast<HFONT>(SendDlgItemMessageW(window, 1029, WM_GETFONT, 0, 0));
        if (!font) font = reinterpret_cast<HFONT>(SendDlgItemMessageW(window, 1027, WM_GETFONT, 0, 0));
        if (!font) font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
        if (!font) font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        LOGFONTW bold{};
        if (!GetObjectW(font, sizeof(bold), &bold)) return false;
        bold.lfWeight = FW_BOLD;
        page->previewFont = CreateFontIndirectW(&bold);
        if (!page->previewFont) return false;
        RECT previewBounds = Units(window, 13, 0, 0, 0), saveAs{}, generate{};
        GetWindowRect(GetDlgItem(window, SaveAsId), &saveAs);
        GetWindowRect(GetDlgItem(window, GenerateId), &generate);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&saveAs), 2);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&generate), 2);
        previewBounds.right = saveAs.right;
        previewBounds.bottom = generate.bottom;
        previewBounds.top = previewBounds.bottom - MulDiv(previewBounds.right - previewBounds.left, 696, 1920);
        const LONG labelHeight = Units(window, 0, 0, 0, 10).bottom, gap = Units(window, 0, 0, 0, 2).bottom;
        const RECT labelBounds{ previewBounds.left, previewBounds.top - gap - labelHeight, previewBounds.right, previewBounds.top - gap };
        // Keep the heading in place while adding space below it for the image.
        OffsetRect(&previewBounds, 0, Units(window, 0, PreviewVerticalOffset, 0, 0).top);
        page->preview = create(L"STATIC", L"", 0, PreviewId, previewBounds);
        const std::wstring previewText = ResourceCaption(PreviewLabelStringId, L"Current terrain:", window);
        page->previewLabel = create(L"STATIC", previewText.c_str(), SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, PreviewLabelId,
            labelBounds);
        if (page->previewLabel) SendMessageW(page->previewLabel, WM_SETFONT, reinterpret_cast<WPARAM>(page->previewFont), TRUE);
        const std::wstring importText = ImportCaption(window);
        page->importButton = create(L"BUTTON", importText.c_str(), WS_TABSTOP | BS_PUSHBUTTON, ImportId, RECT{});
        const std::wstring cancelText = ResourceCaption(CancelStringId, L"Cancel", window);
        page->cancelButton = create(L"BUTTON", cancelText.c_str(), WS_TABSTOP | BS_PUSHBUTTON, CancelId, RECT{});
        if (!page->preview || !page->previewLabel || !page->importButton || !page->cancelButton ||
            !SetWindowSubclass(page->preview, PreviewProc, 1, 0) ||
            !SetWindowSubclass(window, PageProc, 1, reinterpret_cast<DWORD_PTR>(page.get())))
        {
            DestroyWindow(page->preview); DestroyWindow(page->previewLabel); DestroyWindow(page->importButton);
            DestroyWindow(page->cancelButton);
            return false;
        }
        pages.push_back(page.get());
        Refresh(*page);
        page.release();
        return true;
    }

    int __fastcall CreatePage(void* object, void*)
    {
        const int result = originalCreatePage(object);
        if (result) Attach(*reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 28));
        return result;
    }
    int __fastcall CreateGameControls(void* object, void*)
    {
        const int result = originalCreateGameControls(object);
        if (result) AttachGamePreview(*reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 28));
        return result;
    }
    void __fastcall PrepareLocal(void* object, void*)
    {
        preparingLocal = true;
        launchFailed = false;
        originalPrepareLocal(object);
        preparingLocal = false;
    }
    bool LocalTerrainCaller(const BYTE* caller)
    {
        return caller == image + 0x7956c || caller == image + 0x77e04;
    }
    void PrepareTerrainForCaller(void* object, const void* terrain, const BYTE* caller)
    {
        const bool previous = preparingLocal;
        preparingLocal = LocalTerrainCaller(caller);
        const bool previousNetwork = network.generating;
        network.generating = network.hosting && NetworkMap() && (caller == image + 0x35819 || caller == image + 0x623e8);
        if (preparingLocal) launchFailed = false;
        if (network.generating) Trace("host terrain prepare enter", network.round.revision, network.failed);
        originalPrepareTerrain(object, terrain);
        if (network.generating) Trace("host terrain prepare returned", network.round.revision, network.failed);
        preparingLocal = previous;
        network.generating = previousNetwork;
    }
    void __fastcall PrepareTerrain(void* object, void*, const void* terrain)
    {
        PrepareTerrainForCaller(object, terrain, static_cast<BYTE*>(_ReturnAddress()));
    }
    void GenerateForCaller(void* object, const BYTE* caller)
    {
        if (NetworkMap() && (network.generating || (network.joining && (caller == image + 0x3e266 || caller == image + 0x6445c))))
        {
            network.failed = network.failed || !PublishNetworkMap();
            Trace("terrain generation bypassed", network.round.revision, network.failed);
            if (!network.failed) *(static_cast<BYTE*>(object) + 0x38) = 0;
            return;
        }
        if (!preparingLocal || !selected) { launchFailed = false; originalGenerate(object); return; }
        launchFailed = !Publish(*selected, GameDirectory() + L"\\Data\\land.dat");
        if (launchFailed)
        {
            MessageBoxW(nullptr, L"Error saving Data\\land.dat",
                ImportCaption().c_str(), MB_OK | MB_ICONERROR);
            return;
        }
        // Mirror the native generator's completion byte. The LND itself owns
        // the cavern flag, palette, collision data and object coordinates.
        *(static_cast<BYTE*>(object) + 0x38) = 0;
    }
    void __fastcall Generate(void* object, void*)
    { GenerateForCaller(object, static_cast<BYTE*>(_ReturnAddress())); }
    bool LocalLaunchCaller(const BYTE* caller)
    {
        return caller == image + 0x79591 || caller == image + 0x77e4b ||
            caller == image + 0x9d58 || caller == image + 0xa074 || caller == image + 0xa64d;
    }
    int LaunchForCaller(void* object, const char* command, const BYTE* caller)
    {
        if (selected && LocalLaunchCaller(caller))
        {
            if (launchFailed) { launchFailed = false; return 0; }
            // The normal Go path and subsequent rounds prepare terrain through
            // the Select Level page, rather than the quick-game helper. Publish
            // at the final launch boundary too, after every native preparation.
            if (!Publish(*selected, GameDirectory() + L"\\Data\\land.dat"))
            {
                MessageBoxW(nullptr, L"Error saving Data\\land.dat",
                    ImportCaption().c_str(), MB_OK | MB_ICONERROR);
                return 0;
            }
        }
        return originalLaunch(object, command);
    }
    int __fastcall LaunchGame(void* object, void*, const char* command)
    {
        return LaunchForCaller(object, command, static_cast<BYTE*>(_ReturnAddress()));
    }

    bool InstallInImage(BYTE* frontend)
    {
        image = frontend;
        enabled = false;
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < 0x1b5130) return false;
        struct Hook { size_t rva; const char* signature; size_t length; void* detour; void** original; };
        const Hook hooks[] = {
            { 0x105f7, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9, reinterpret_cast<void*>(DirectReceive), reinterpret_cast<void**>(&originalDirectReceive) },
            { 0x1378d, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9, reinterpret_cast<void*>(SequenceFilter), reinterpret_cast<void**>(&originalSequenceFilter) },
            { 0x42a44, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(CreatePage), reinterpret_cast<void**>(&originalCreatePage) },
            { 0x7896a, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(CreateGameControls), reinterpret_cast<void**>(&originalCreateGameControls) },
            { 0xa349, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(PrepareLocal), reinterpret_cast<void**>(&originalPrepareLocal) },
            { 0x4468b, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(PrepareTerrain), reinterpret_cast<void**>(&originalPrepareTerrain) },
            { 0x46f09, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(Generate), reinterpret_cast<void**>(&originalGenerate) },
            { 0x277c4, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9, reinterpret_cast<void*>(LaunchGame), reinterpret_cast<void**>(&originalLaunch) },
            { 0x3233d, "\x55\x8b\xec\x6a\xff\x68\x13\x98\x4e\x00", 10, reinterpret_cast<void*>(HostInit), reinterpret_cast<void**>(&originalHostInit) },
            { 0x336f1, "\x55\x8b\xec\x6a\xff\x68\x4f\x98\x4e\x00", 10, reinterpret_cast<void*>(HostGo), reinterpret_cast<void**>(&originalHostGo) },
            { 0x62214, "\x55\x8b\xec\x6a\xff\x68\x0b\xcd\x4e\x00", 10, reinterpret_cast<void*>(HostRoundGo), reinterpret_cast<void**>(&originalHostRoundGo) },
            { 0x34294, "\x55\x8b\xec\x81\xec\xc8\x00\x00\x00", 9, reinterpret_cast<void*>(HostReceive), reinterpret_cast<void**>(&originalHostReceive) },
            { 0x61865, "\x55\x8b\xec\x83\xec\x14\x89\x4d\xf0", 9, reinterpret_cast<void*>(HostRoundReceive), reinterpret_cast<void**>(&originalHostRoundReceive) },
            { 0x1e32f, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendReady), reinterpret_cast<void**>(&originalSendReady) },
            { 0x358a4, "\x55\x8b\xec\x6a\xff\x68\xaa\x98\x4e\x00", 10, reinterpret_cast<void*>(LaunchHost), reinterpret_cast<void**>(&originalHostLaunch) },
            { 0x61a81, "\x55\x8b\xec\x6a\xff\x68\xdf\xcc\x4e\x00", 10, reinterpret_cast<void*>(LaunchHostRound), reinterpret_cast<void**>(&originalHostRoundLaunch) },
            { 0x144ed, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9, reinterpret_cast<void*>(RunApplication), reinterpret_cast<void**>(&originalRunApplication) },
            { 0xa0b70, "\x83\xec\x0c\x53\x55\x8b\x6c\x24\x1c", 9, reinterpret_cast<void*>(Spawn), reinterpret_cast<void**>(&originalSpawn) },
        };
        for (const auto& hook : hooks) if (memcmp(image + hook.rva, hook.signature, hook.length)) return false;
        // Validate the local-only generator call and native completion byte.
        if (memcmp(image + 0xa3cd, "\xe8\x25\x75\xff\xff", 5) ||
            memcmp(image + 0x47301, "\xc6\x41\x38\x00", 4) ||
            memcmp(image + 0x79567, "\xe8\x0b\x86\xf8\xff", 5) ||
            memcmp(image + 0x77dff, "\xe8\x73\x9d\xf8\xff", 5) ||
            memcmp(image + 0x7958c, "\xe8\x7d\x80\xf8\xff", 5) ||
            memcmp(image + 0x77e46, "\xe8\xc3\x97\xf8\xff", 5) ||
            memcmp(image + 0x35814, "\xe8\x5e\xc3\xfc\xff", 5) ||
            memcmp(image + 0x623e3, "\xe8\x8f\xf7\xf9\xff", 5) ||
            memcmp(image + 0x3e261, "\xe8\x91\x36\xfc\xff", 5) ||
            memcmp(image + 0x64457, "\xe8\x9b\xd4\xf9\xff", 5) ||
            memcmp(image + 0x283de, "\x8b\x8c\x90\x8c\x10\x00\x00", 7) ||
            // DirectPlay Send, reliable sequence counter/envelope and channel.
            memcmp(image + 0x1058b, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9) ||
            memcmp(image + 0x12698, "\x83\xb8\x38\x04\x00\x00\x00", 7) ||
            memcmp(image + 0x126fa, "\x8b\x4d\x14\x83\xc1\x04", 6) ||
            memcmp(image + 0x1e2dc, "\x81\xc1\x50\x2d\x01\x00", 6) ||
            memcmp(image + 0x1e2c0, "\x83\xb8\xd4\x20\x01\x00\x01", 7) ||
            memcmp(image + 0x1e371, "\x81\xc1\xc8\x28\x01\x00", 6) ||
            // Native ready uses this global local ID and network object.
            memcmp(image + 0x3e227, "\x8b\x0d\xac\x92\x58\x00\x51\xb9\x40\x6a\x57\x00", 12) ||
            // Sequence filtering uses this buffer after skipping its prefix.
            memcmp(image + 0x12cc9, "\xb9\xc4\x63\x55\x00", 5) ||
            memcmp(image + 0x12cb8, "\xe8\xdb\x0e\xff\xff", 5) ||
            // The IPX launcher forwards execl arguments into this spawn entry.
            memcmp(image + 0x97d50, "\x8b\x4c\x24\x08\x8b\x54\x24\x04", 8) ||
            memcmp(image + 0x97d61, "\xe8\x0a\x8e\x00\x00", 5) ||
            // Confirm the CStringList layout used to resolve water item data.
            memcmp(image + 0xbc9af, "\x8b\x54\x24\x04\x3b\x51\x0c", 7)) return false;
        size_t created = 0;
        for (const auto& hook : hooks)
        {
            if (MH_CreateHook(image + hook.rva, hook.detour, hook.original) != MH_OK) break;
            ++created;
        }
        bool success = created == sizeof(hooks) / sizeof(hooks[0]);
        if (success)
            for (const auto& hook : hooks) if (MH_EnableHook(image + hook.rva) != MH_OK) { success = false; break; }
        if (!success)
            for (size_t i = 0; i < created; ++i) { MH_DisableHook(image + hooks[i].rva); MH_RemoveHook(image + hooks[i].rva); }
        enabled = success;
        directSend = success ? reinterpret_cast<NativeDirectSend>(image + 0x1058b) : nullptr;
        return success;
    }
}
void SetLanguage(const std::string& language)
{
    strings = MakeMapStrings(language);
}

void BeginDiagnostics(void* module)
{
    char build[160]{};
    sprintf_s(build, "map-network diagnostics 22 protocol 10 build %s %s", __DATE__, __TIME__);
    TraceText(build);
    const BYTE digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    Trace("CRC32 self-test", Checksum(digits, sizeof(digits)), 0xcbf43926);
    wchar_t path[MAX_PATH]{};
    const DWORD executableLength = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (executableLength && executableLength < MAX_PATH) TracePath("frontend path", path);
    const DWORD moduleLength = module ? GetModuleFileNameW(static_cast<HMODULE>(module), path, MAX_PATH) : 0;
    if (moduleLength && moduleLength < MAX_PATH) TracePath("DLL path", path);
    TracePath("Data log path", traceGamePath.c_str());
    TracePath("TEMP log path", traceMirrorPath.c_str());
}
void ReportNetworkHooks(bool lobby, bool round) { Trace("shared receive hooks", lobby, round); }
bool Install()
{
    const bool success = InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
    Trace("colour map hooks", success, 18); return success;
}
void Shutdown() { ResetNetwork(); }
void SendNetworkPacket(void* object, uint32_t source, uint32_t target, bool broadcast,
    const void* packet, uint32_t length, NetworkSend send)
{ SendNetwork(object, source, target, broadcast, packet, length, send); }
bool ReceiveNetworkPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t& length)
{
    try
    {
        const uint32_t type = PacketType(packet, length);
        if (type == MapPacketType || type == MapAckPacketType)
        {
            if (length > MaximumMapMessage) { Trace("map wire length rejected", sender, length); return false; }
            // Native receive buffers are shared by lobby/ready polling. Own the
            // custom packet before logging, refreshing controls or decoding it.
            const BYTE* bytes = static_cast<const BYTE*>(packet);
            const std::vector<BYTE> copy(bytes, bytes + length);
            return ReceiveNetwork(sender, host, copy.data(), length);
        }
        return ReceiveNetwork(sender, host, packet, length);
    }
    catch (const std::exception&) { network.round = {}; network.roundMap.reset(); return false; }
}
void JoiningLobby(void* object)
{
    if (!enabled) return;
    SelectNetworkHost(*reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0x163c));
    WatchNetworkWindow(object);
}
void JoiningRound(void* object)
{
    if (!enabled) return;
    SelectNetworkHost(*reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0xa8));
    WatchNetworkWindow(object);
}
}
