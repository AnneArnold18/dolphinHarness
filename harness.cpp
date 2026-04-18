#include "Core/Host.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/FileSystemGCWii.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

using std::string;

// --- Stub Host callbacks ---
std::vector<std::string> Host_GetPreferredLocales() { return {}; }
void Host_PPCSymbolsChanged() {}
void Host_PPCBreakpointsChanged() {}
void Host_Message(HostMessageID) {}
void Host_UpdateTitle(const string&) {}
void Host_UpdateDiscordClientID(const string&) {}
bool Host_UpdateDiscordPresenceRaw(const string&, const string&,
                                   const string&, const string&,
                                   const string&, const string&,
                                   const int64_t, const int64_t,
                                   const int, const int) { return false; }
void Host_UpdateDisasmDialog() {}
void Host_JitCacheInvalidation() {}
void Host_JitProfileDataWiped() {}
void Host_RequestRenderWindowSize(int, int) {}
bool Host_UIBlocksControllerState() { return false; }
bool Host_RendererHasFocus() { return false; }
bool Host_RendererHasFullFocus() { return false; }
bool Host_RendererIsFullscreen() { return false; }
bool Host_TASInputHasFocus() { return false; }
void Host_YieldToUI() {}
void Host_TitleChanged() {}
std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{ return nullptr; }

// Magic byte constants
static const u8 GC_MAGIC[4]   = {0xC2, 0x33, 0x9F, 0x3D};  // @ 0x1C
static const u8 WII_MAGIC[4]  = {0x5D, 0x1C, 0x9E, 0xA3};  // @ 0x18
static const u8 WIA_MAGIC[4]  = {'W',  'I',  'A',  0x01};  // @ 0x00
static const u8 RVZ_MAGIC[4]  = {'R',  'V',  'Z',  0x01};  // @ 0x00
static const u8 CISO_MAGIC[4] = {'C',  'I',  'S',  'O'};   // @ 0x00
static const u8 WBFS_MAGIC[4] = {'W',  'B',  'F',  'S'};   // @ 0x00

// Size limits
static const long MAX_INPUT_SIZE    = 32 * 1024 * 1024;  // 32MB absolute max
static const u64  MAX_READ_OFFSET   = 0x200000;          // 2MB max read range
static const u64  READ_CHUNK        = 0x8000;            // 32KB chunks

class MemoryBlobReader : public DiscIO::BlobReader
{
public:
    MemoryBlobReader(const u8* data, size_t size, DiscIO::BlobType type)
        : m_data(data), m_size(size), m_type(type) {}

    u64 GetDataSize() const override
    {
        return m_size;
    }

    u64 GetRawSize() const override
    {
        return m_size;
    }

    bool Read(u64 offset, u64 length, u8* out) override
    {
        if (offset > m_size || length > m_size || offset + length > m_size)
            return false;

        memcpy(out, m_data + offset, length);
        return true;
    }

    DiscIO::BlobType GetBlobType() const override
    {
        // added dynamic typing based on file format
        return m_type;
    }

    std::unique_ptr<DiscIO::BlobReader> CopyReader() const override
    {
        return std::make_unique<MemoryBlobReader>(m_data, m_size, m_type);
    }

    DiscIO::DataSizeType GetDataSizeType() const override
    {
        return DiscIO::DataSizeType::Accurate;
    }

    u64 GetBlockSize() const override
    {
        // Arbitrary but reasonable block size
        return 0x8000;
    }

    bool HasFastRandomAccessInBlock() const override
    {
        // Memory is always fast random access
        return true;
    }

    std::string GetCompressionMethod() const override
    {
        return "none";
    }

    std::optional<int> GetCompressionLevel() const override
    {
        return std::nullopt;
    }

private:
    const u8* m_data;
    size_t m_size;
    DiscIO::BlobType m_type;
};

// Helper: fuzz a volume object
static void fuzz_volume(DiscIO::Volume* vol)
{
    if (!vol)   return;

    vol->GetGameID();
    vol->GetInternalName();
    vol->GetGameTDBID();
    vol->GetRevision();
    vol->GetDiscNumber();
    vol->GetVolumeType();
    vol->GetDataSize();
    vol->GetRawSize();
    vol->GetBlobType();

    auto* disc = dynamic_cast<DiscIO::VolumeDisc*>(vol);
    if (!disc) return;

    std::vector<u8> buf(READ_CHUNK);

    const auto partitions = disc->GetPartitions();

    // Wii path (partitions)
    for (const auto& partition : partitions)
    {
        vol->GetPartitionType(partition);
        vol->GetTitleID(partition);
        vol->GetTicket(partition);
        vol->GetTMD(partition);

        // Encrypted reads — exercises AES + decompression
        for (u64 off = 0; off < MAX_READ_OFFSET; off += READ_CHUNK)
            vol->Read(off, READ_CHUNK, buf.data(), partition);

        auto fs = vol->GetFileSystem(partition);
        if (fs)
        {
            auto root = fs->FindFileInfo("/");
            if (root)
            {
                for (const auto& child : *root)
                {
                    const std::string name = child.GetName();
                    child.GetSize();
                    child.IsDirectory();
                    if (!name.empty())
                        fs->FindFileInfo(name);
                }
            }
        }
    }
}

// Helper: fuzz blob-level reading 
static void fuzz_blob(DiscIO::BlobReader* blob)
{
    const u64 data_size = blob->GetDataSize();
    const u64 raw_size  = blob->GetRawSize();

    // Sanity check — skip if sizes look corrupted/huge
    if (data_size > (u64)MAX_INPUT_SIZE * 4) return;
    if (raw_size  > (u64)MAX_INPUT_SIZE * 4) return;

    std::vector<u8> buf(READ_CHUNK);
    const u64 limit = std::min(data_size, MAX_READ_OFFSET);

    for (u64 off = 0; off < limit; off += READ_CHUNK)
    {
        u64 to_read = std::min(READ_CHUNK, limit - off);
        blob->Read(off, to_read, buf.data());
    }
}

static void try_format(std::vector<u8> data,
                       const u8* magic, size_t magic_offset, size_t magic_len,
                        DiscIO::BlobType type)
{
    if (data.size() < magic_offset + magic_len) return;

    // memcpy(data.data() + magic_offset, magic, magic_len);

    auto blob = std::make_unique<MemoryBlobReader>(data.data(), data.size(), type);

    fuzz_blob(blob.get());

    auto volume = DiscIO::CreateVolume(std::move(blob));
    if (!volume) return;

    fuzz_volume(volume.get());
}


int main(int argc, const char* argv[])
{
    __AFL_FUZZ_INIT();
    
    while (__AFL_LOOP(30000))
    {
        const u8* buffer = __AFL_FUZZ_TESTCASE_BUF;
        int len = __AFL_FUZZ_TESTCASE_LEN;

        if (len < 0x40 || len > MAX_INPUT_SIZE)
            continue;

        std::vector<u8> data(buffer, buffer + len);

        // format detection
        if (len >= 4 && memcmp(data.data(), CISO_MAGIC, 4) == 0)
            try_format(data, CISO_MAGIC, 0x00, 4, DiscIO::BlobType::CISO);
        else if (len >= 4 && memcmp(data.data(), WBFS_MAGIC, 4) == 0)
            try_format(data, WBFS_MAGIC, 0x00, 4, DiscIO::BlobType::WBFS);
        else if (len >= 4 && memcmp(data.data(), WIA_MAGIC, 4) == 0)
            try_format(data, WIA_MAGIC, 0x00, 4, DiscIO::BlobType::WIA);
        else if (len >= 4 && memcmp(data.data(), RVZ_MAGIC, 4) == 0)
            try_format(data, RVZ_MAGIC, 0x00, 4, DiscIO::BlobType::RVZ);
        else if (len >= 0x1C + 4 &&
                 memcmp(data.data() + 0x18, WII_MAGIC, 4) == 0)
            try_format(data, WII_MAGIC, 0x18, 4, DiscIO::BlobType::PLAIN);
        else
            try_format(data, GC_MAGIC, 0x1C, 4, DiscIO::BlobType::PLAIN);
    }

    return 0;
}
