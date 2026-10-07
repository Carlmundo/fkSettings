#include "../fkSettings/ColourMaps.cpp"

extern "C" { extern __declspec(thread) int _Init_thread_epoch; }

int wmain(int argc, wchar_t** argv)
{
    try
    {
        ColourMaps::traceEnabled = false;
        // A DLL loaded on XP can see a zero CRT TLS epoch. Test the first use
        // with the production XP compiler, before any other checksum call.
        const BYTE digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
        const int savedEpoch = _Init_thread_epoch;
        _Init_thread_epoch = 0;
        const uint32_t first = ColourMaps::Checksum(digits, sizeof(digits));
        _Init_thread_epoch = savedEpoch;
        printf("First CRC32 with zero CRT TLS epoch: %08x (expected cbf43926)\n", first);
        if (first != 0xcbf43926) throw std::runtime_error("first CRC32 depends on CRT TLS initialization");
        if (ColourMaps::Checksum(nullptr, 0) != 0) throw std::runtime_error("empty CRC32");
        std::vector<BYTE> bytes(256);
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<BYTE>(i);
        if (ColourMaps::Checksum(bytes) != 0x29058c73) throw std::runtime_error("CRC32 of all byte values");
        bytes.assign(ColourMaps::ChunkSize, 0);
        if (ColourMaps::Checksum(bytes) != 0x011ffca6) throw std::runtime_error("CRC32 of zero-filled chunk");
        if (argc == 2)
        {
            const auto map = ColourMaps::Load(argv[1]);
            if (ColourMaps::Checksum(map.bytes) != 0x3a3314b7 ||
                ColourMaps::Checksum(map.bytes.data(), ColourMaps::ChunkSize) != 0x832248dd)
                throw std::runtime_error("Birthday map/chunk CRC32");
            for (uint32_t limit : { ColourMaps::TcpWireSize, ColourMaps::IpxWireSize, ColourMaps::MinimumWireSize })
            {
                ColourMaps::ResetNetwork();
                ColourMaps::MapTransfer transfer; transfer.wireLimit = limit;
                transfer.map = std::make_shared<ColourMaps::Map>(map);
                transfer.identity.revision = 1; transfer.identity.size = static_cast<uint32_t>(map.bytes.size());
                transfer.identity.crc = ColourMaps::Checksum(map.bytes);
                unsigned packets = 0;
                while (transfer.offset < transfer.identity.size)
                {
                    const auto flight = ColourMaps::PrepareTransferPacket(transfer, transfer.offset);
                    if (!flight.header.size || flight.packet.size() > sizeof(ColourMaps::CheckedMapChunk) + limit ||
                        !ColourMaps::ReceiveMapChunk(flight.packet.data(), static_cast<uint32_t>(flight.packet.size())))
                        throw std::runtime_error("XP bounded map transfer");
                    transfer.offset += flight.header.size; ++packets;
                }
                if (!ColourMaps::network.remote || ColourMaps::network.remote->bytes != map.bytes || !packets ||
                    (limit == ColourMaps::TcpWireSize && packets > 16))
                    throw std::runtime_error("XP bounded Birthday reconstruction");
                printf("PASS: XP toolset reconstructs Birthday byte-for-byte in %u packets with %u-byte payload cap\n", packets, limit);
            }
        }
        puts("PASS: XP toolset CRC32, zero TLS epoch, known vectors and optional Birthday map");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
