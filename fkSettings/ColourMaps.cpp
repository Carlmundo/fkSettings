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
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "ColourMaps.h"
#include "include/MinHook.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace ColourMaps
{
namespace
{
    constexpr int ImportId = 51010, ResetId = 51011, PreviewId = 51012, LevelStyleId = 1021, WaterId = 1020;
    constexpr size_t MaximumFileSize = 8 * 1024 * 1024;
    struct Map
    {
        std::vector<BYTE> bytes, pixels;
        std::array<RGBQUAD, 256> palette{};
        std::wstring name;
        bool cavern = false;
        unsigned spawns = 0;
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
        pixels.reserve(size);
        if (flags & 0x40)
        {
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
        map.spawns = reader.Dword();
        if (map.spawns < 18 || map.spawns > 32) throw std::runtime_error("Expected 18 to 32 object locations.");
        for (unsigned i = 0; i < map.spawns; ++i)
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
        const wchar_t* name = wcsrchr(path, L'\\');
        if (!name) name = wcsrchr(path, L'/');
        map.name = name ? name + 1 : path;
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
    void RefreshAll();

    constexpr uint32_t MapPacketType = 0x464b4d31; // FKM1, outside the native message range.
    constexpr uint32_t MapTrailerMagic = 0x314d4b46; // ASCII FKM1.
    constexpr uint32_t ChunkSize = 32 * 1024; // Native receive buffers are 0x12000 bytes.
    struct MapIdentity { uint32_t magic = MapTrailerMagic, revision = 0, size = 0, crc = 0; };
    struct MapChunk { uint32_t type = MapPacketType, version = 1; MapIdentity identity; uint32_t offset = 0, size = 0; };
    static_assert(sizeof(MapIdentity) == 16 && sizeof(MapChunk) == 32, "network layout");
    struct NetworkState
    {
        bool hosting = false, joining = false, generating = false, failed = false;
        uint32_t host = 0, revision = 0;
        HWND window = nullptr;
        MapIdentity incoming{}, complete{}, round{};
        std::vector<BYTE> bytes, chunks;
        size_t received = 0;
        std::unique_ptr<Map> remote, roundMap;
        std::array<std::vector<uint32_t>, 2> acknowledgements;
        void* transport = nullptr;
        uint32_t source = 0;
        NetworkSend send = nullptr;
    } network;

    uint32_t PacketType(const void* packet, uint32_t length)
    {
        uint32_t type = 0;
        if (packet && length >= 4) memcpy(&type, packet, 4);
        return type;
    }
    uint32_t Checksum(const std::vector<BYTE>& bytes)
    {
        static const auto table = [] {
            std::array<uint32_t, 256> result{};
            for (uint32_t i = 0; i < result.size(); ++i)
            {
                uint32_t value = i;
                for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
                result[i] = value;
            }
            return result;
        }();
        uint32_t crc = ~0u;
        for (BYTE byte : bytes) crc = table[(crc ^ byte) & 255] ^ (crc >> 8);
        return ~crc;
    }
    bool SameIdentity(const MapIdentity& a, const MapIdentity& b)
    {
        return a.magic == MapTrailerMagic && b.magic == MapTrailerMagic &&
            a.revision == b.revision && a.size == b.size && a.crc == b.crc;
    }
    const Map* PreviewMap() { return network.joining ? network.remote.get() : selected.get(); }
    const Map* NetworkMap() { return network.round.size ? network.roundMap.get() : nullptr; }

    LRESULT CALLBACK NetworkWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            if (network.window == window) { network = NetworkState{}; RefreshAll(); }
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
            network = NetworkState{};
            network.window = window;
            network.joining = true;
            network.host = host;
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

    MapIdentity TransferMap(const Map* map, void* object, uint32_t source, uint32_t target, bool broadcast, NetworkSend send)
    {
        MapIdentity identity;
        identity.revision = ++network.revision;
        if (!identity.revision) identity.revision = ++network.revision;
        if (map) { identity.size = static_cast<uint32_t>(map->bytes.size()); identity.crc = Checksum(map->bytes); }
        std::vector<BYTE> packet(sizeof(MapChunk) + ChunkSize);
        MapChunk header; header.identity = identity;
        do
        {
            header.size = (std::min)(ChunkSize, identity.size - header.offset);
            memcpy(packet.data(), &header, sizeof(header));
            if (header.size) memcpy(packet.data() + sizeof(header), map->bytes.data() + header.offset, header.size);
            send(object, source, target, broadcast, packet.data(), sizeof(header) + header.size);
            header.offset += header.size;
        } while (header.offset < identity.size);
        return identity;
    }
    void SelectionChanged()
    {
        try
        {
            if (network.hosting && network.send)
                TransferMap(selected.get(), network.transport, network.source, 0, true, network.send);
        }
        catch (const std::exception&)
        { OutputDebugStringA("fkSettings: Imported map preview transfer failed; retrying at Go.\n"); }
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
    bool ReceiveMapChunk(const void* packet, uint32_t length)
    {
        if (length < sizeof(MapChunk)) return false;
        MapChunk header; memcpy(&header, packet, sizeof(header));
        const auto& identity = header.identity;
        if (header.version != 1 || identity.magic != MapTrailerMagic || !identity.revision || identity.size > MaximumFileSize ||
            header.size > ChunkSize || length != sizeof(header) + header.size || header.offset > identity.size ||
            header.size != (std::min)(ChunkSize, identity.size - header.offset) || header.offset % ChunkSize) return false;
        if (identity.size && (!header.size || header.offset == identity.size)) return false;
        if (!identity.size)
        {
            if (identity.crc || header.offset || header.size) return false;
            if (network.incoming.revision && static_cast<int32_t>(identity.revision - network.incoming.revision) <= 0) return false;
            network.remote.reset(); network.bytes.clear(); network.chunks.clear(); network.received = 0;
            network.incoming = network.complete = identity;
            RefreshAll(); return true;
        }
        if (!SameIdentity(identity, network.incoming))
        {
            // Only the first chunk starts an assembly. Older revisions cannot replace it.
            if (header.offset || (network.incoming.revision && static_cast<int32_t>(identity.revision - network.incoming.revision) <= 0)) return false;
            network.bytes.assign(identity.size, 0);
            network.chunks.assign((identity.size + ChunkSize - 1) / ChunkSize, 0);
            network.received = 0; network.incoming = identity;
        }
        const size_t index = header.offset / ChunkSize;
        const BYTE* data = static_cast<const BYTE*>(packet) + sizeof(header);
        if (network.chunks[index]) return memcmp(network.bytes.data() + header.offset, data, header.size) == 0;
        memcpy(network.bytes.data() + header.offset, data, header.size);
        network.chunks[index] = 1; network.received += header.size;
        if (network.received == identity.size)
        {
            if (Checksum(network.bytes) != identity.crc) return false;
            std::unique_ptr<Map> map(new Map(Parse(network.bytes)));
            map->name = L"Host's imported map";
            network.remote.swap(map); network.complete = identity;
            RefreshAll();
        }
        return true;
    }
    bool ReceiveNetwork(uint32_t sender, uint32_t host, const void* packet, uint32_t& length)
    {
        const uint32_t type = PacketType(packet, length);
        if (!enabled) return type != MapPacketType;
        if (type == MapPacketType)
        {
            if (!host || sender != host) return false;
            SelectNetworkHost(host);
            try { ReceiveMapChunk(packet, length); }
            catch (const std::exception&) { network.complete = {}; }
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
            network.round = {}; network.roundMap.reset(); network.failed = false;
            if (!imported)
            {
                network.remote.reset(); RefreshAll(); return true;
            }
            if (!SameIdentity(identity, network.complete) || !network.remote) return false;
            network.roundMap.reset(new Map(*network.remote)); network.round = identity;
        }
        if (imported)
        {
            if (!SameIdentity(identity, network.round) || !NetworkMap() || !PublishNetworkMap()) return false;
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
            if (broadcast && type == 27 && length == 8)
            {
                network.hosting = true; network.joining = false;
                network.failed = false;
                network.acknowledgements = {};
                network.roundMap.reset(selected ? new Map(*selected) : nullptr);
                network.round = {};
                if (network.roundMap)
                {
                    network.round.size = static_cast<uint32_t>(network.roundMap->bytes.size());
                    if (!Publish(*network.roundMap, GameDirectory() + L"\\Data\\land.dat")) { network.failed = true; return; }
                    network.round = TransferMap(network.roundMap.get(), object, source, target, true, send);
                }
            }
            if (network.hosting)
            {
                network.transport = object; network.source = source; network.send = send;
                if (network.round.size && (type == 27 || type == 29 || type == 14))
                {
                    std::vector<BYTE> extended(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
                    const BYTE* trailer = reinterpret_cast<const BYTE*>(&network.round);
                    extended.insert(extended.end(), trailer, trailer + sizeof(network.round));
                    send(object, source, target, broadcast, extended.data(), static_cast<uint32_t>(extended.size()));
                    return;
                }
            }
            send(object, source, target, broadcast, packet, length);
            // A late join first needs the native snapshot to establish its host ID.
            if (!broadcast && type == 5 && network.hosting)
                TransferMap(selected.get(), object, source, target, false, send);
        }
        catch (const std::exception&)
        {
            if (type == 27 || type == 29 || type == 14) network.failed = true;
            OutputDebugStringA("fkSettings: Imported map transfer failed; network start withheld.\n");
        }
    }
    void __fastcall SendReady(void* object, void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
    {
        const uint32_t type = PacketType(packet, length);
        if (network.joining && network.round.size && (type == 28 || type == 30))
        {
            if (length != 8 || target != network.host || network.failed || !PublishNetworkMap()) return;
            std::array<BYTE, 8 + sizeof(MapIdentity)> extended{};
            memcpy(extended.data(), packet, 8); memcpy(extended.data() + 8, &network.round, sizeof(MapIdentity));
            originalSendReady(object, source, target, extended.data(), static_cast<uint32_t>(extended.size()));
            return;
        }
        originalSendReady(object, source, target, packet, length);
    }
    bool AcceptReady(uint32_t sender, const void* packet, uint32_t length)
    {
        const uint32_t type = PacketType(packet, length);
        if (!network.hosting || !network.round.size || (type != 28 && type != 30)) return true;
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
        acknowledged.push_back(sender); return true;
    }
    void ReceiveHost(void* object, uint32_t sender, const void* packet, uint32_t length, NativeReceive original)
    {
        WatchNetworkWindow(object);
        const bool reliable = *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1;
        const uint32_t payload = reliable ? (length >= 4 ? length - 4 : 0) : length;
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
        network = NetworkState{}; network.hosting = true;
        const int result = originalHostInit(object);
        if (result) WatchNetworkWindow(object);
        return result;
    }
    void HostLaunch(void* object, NativeVoid original)
    {
        WatchNetworkWindow(object);
        if (network.failed || !PublishNetworkMap())
        {
            MessageBoxW(nullptr, L"Cannot write Data\\land.dat. Unlock it in CTerrain and check the folder is writable. The network game was cancelled.",
                L"Import colour map", MB_OK | MB_ICONERROR); return;
        }
        original(object);
    }
    void __fastcall LaunchHost(void* object, void*) { HostLaunch(object, originalHostLaunch); }
    void __fastcall LaunchHostRound(void* object, void*) { HostLaunch(object, originalHostRoundLaunch); }

    const int HiddenControls[] = { 1018, 1012, 1014, 1016, 1033, 1000, 1057 };
    const int DisabledControls[] = { 1019, 1022, 1030, 1031, 1032, 1034, 1023, 1024 };
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
        HWND window, preview, importButton, resetButton;
        bool active = false;
        std::array<bool, 7> visible{};
        std::array<bool, 8> enabled{};
        ComboState style, water;
        std::array<LRESULT, 2> styleOptions{ CB_ERR, CB_ERR };
        LRESULT customWater = CB_ERR;
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
        ShowWindow(page.resetButton, active ? SW_SHOW : SW_HIDE);
        EnableWindow(page.importButton, !network.joining);
        EnableWindow(page.resetButton, !network.joining);
        Position(page.importButton, Units(page.window, 13, active ? 153 : 99, active ? 90 : 68, 14));
        Position(page.resetButton, Units(page.window, 207, 153, 100, 14));
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

    void Browse(HWND owner)
    {
        wchar_t path[MAX_PATH]{};
        const std::wstring directory = lastDirectory.empty() ? GameDirectory() + L"\\Levels\\Import" : lastDirectory;
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter = L"Worms 2 terrain (*.dat)\0*.dat\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrInitialDir = directory.c_str();
        dialog.lpstrTitle = L"Import colour map";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        if (!GetOpenFileNameW(&dialog)) return;
        try
        {
            std::unique_ptr<Map> next(new Map(Load(path)));
            if (!Publish(*next, GameDirectory() + L"\\Data\\land.dat"))
                throw std::runtime_error("Cannot write Data\\land.dat. Unlock it in CTerrain and check the folder is writable.");
            selected.swap(next);
            lastDirectory.assign(path, wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') - path : 0);
            launchFailed = false;
            RefreshAll();
            SelectionChanged();
        }
        catch (const std::exception& error) { MessageBoxA(owner, error.what(), "Import colour map", MB_OK | MB_ICONERROR); }
    }

    LRESULT CALLBACK PageProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR reference)
    {
        Page* page = reinterpret_cast<Page*>(reference);
        if (network.joining && message == WM_COMMAND && (LOWORD(wparam) == ImportId || LOWORD(wparam) == ResetId ||
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
        if (message == WM_COMMAND && HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == ResetId)
        {
            selected.reset();
            launchFailed = false;
            RefreshAll();
            SelectionChanged();
            return 0;
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
        std::unique_ptr<Page> page(new Page{ window, nullptr, nullptr, nullptr });
        auto create = [&](const wchar_t* type, const wchar_t* text, DWORD style, int id, RECT rect) {
            HWND child = CreateWindowExW(0, type, text, WS_CHILD | style, rect.left, rect.top,
                rect.right - rect.left, rect.bottom - rect.top, window, reinterpret_cast<HMENU>(id), GetModuleHandleW(nullptr), nullptr);
            if (child) SendMessageW(child, WM_SETFONT, SendMessageW(window, WM_GETFONT, 0, 0), TRUE);
            return child;
        };
        page->preview = create(L"STATIC", L"", 0, PreviewId, MapRectangle(Units(window, 13, 7, 294, 140)));
        page->importButton = create(L"BUTTON", L"Import map...", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, ImportId, Units(window, 13, 99, 68, 14));
        page->resetButton = create(L"BUTTON", L"Use generated map", WS_TABSTOP | BS_PUSHBUTTON, ResetId, Units(window, 207, 153, 100, 14));
        if (!page->preview || !page->importButton || !page->resetButton ||
            !SetWindowSubclass(page->preview, PreviewProc, 1, 0) ||
            !SetWindowSubclass(window, PageProc, 1, reinterpret_cast<DWORD_PTR>(page.get())))
        {
            DestroyWindow(page->preview); DestroyWindow(page->importButton); DestroyWindow(page->resetButton);
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
        originalPrepareTerrain(object, terrain);
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
            network.failed = !PublishNetworkMap();
            if (!network.failed) *(static_cast<BYTE*>(object) + 0x38) = 0;
            return;
        }
        if (!preparingLocal || !selected) { launchFailed = false; originalGenerate(object); return; }
        launchFailed = !Publish(*selected, GameDirectory() + L"\\Data\\land.dat");
        if (launchFailed)
        {
            MessageBoxW(nullptr, L"Cannot write Data\\land.dat. Unlock it in CTerrain and check the folder is writable. The local game was cancelled.",
                L"Import colour map", MB_OK | MB_ICONERROR);
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
                MessageBoxW(nullptr, L"Cannot write Data\\land.dat. Unlock it in CTerrain and check the folder is writable. The local game was cancelled.",
                    L"Import colour map", MB_OK | MB_ICONERROR);
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
            { 0x42a44, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(CreatePage), reinterpret_cast<void**>(&originalCreatePage) },
            { 0x7896a, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(CreateGameControls), reinterpret_cast<void**>(&originalCreateGameControls) },
            { 0xa349, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(PrepareLocal), reinterpret_cast<void**>(&originalPrepareLocal) },
            { 0x4468b, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(PrepareTerrain), reinterpret_cast<void**>(&originalPrepareTerrain) },
            { 0x46f09, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(Generate), reinterpret_cast<void**>(&originalGenerate) },
            { 0x277c4, "\x55\x8b\xec\x83\xec\x08\x89\x4d\xf8", 9, reinterpret_cast<void*>(LaunchGame), reinterpret_cast<void**>(&originalLaunch) },
            { 0x3233d, "\x55\x8b\xec\x6a\xff\x68\x13\x98\x4e\x00", 10, reinterpret_cast<void*>(HostInit), reinterpret_cast<void**>(&originalHostInit) },
            { 0x34294, "\x55\x8b\xec\x81\xec\xc8\x00\x00\x00", 9, reinterpret_cast<void*>(HostReceive), reinterpret_cast<void**>(&originalHostReceive) },
            { 0x61865, "\x55\x8b\xec\x83\xec\x14\x89\x4d\xf0", 9, reinterpret_cast<void*>(HostRoundReceive), reinterpret_cast<void**>(&originalHostRoundReceive) },
            { 0x1e32f, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendReady), reinterpret_cast<void**>(&originalSendReady) },
            { 0x358a4, "\x55\x8b\xec\x6a\xff\x68\xaa\x98\x4e\x00", 10, reinterpret_cast<void*>(LaunchHost), reinterpret_cast<void**>(&originalHostLaunch) },
            { 0x61a81, "\x55\x8b\xec\x6a\xff\x68\xdf\xcc\x4e\x00", 10, reinterpret_cast<void*>(LaunchHostRound), reinterpret_cast<void**>(&originalHostRoundLaunch) },
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
        return success;
    }
}
bool Install() { return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))); }
void SendNetworkPacket(void* object, uint32_t source, uint32_t target, bool broadcast,
    const void* packet, uint32_t length, NetworkSend send)
{ SendNetwork(object, source, target, broadcast, packet, length, send); }
bool ReceiveNetworkPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t& length)
{
    try { return ReceiveNetwork(sender, host, packet, length); }
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
