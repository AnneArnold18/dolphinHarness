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

// Helper: write buffer to temp file 
static bool write_temp(const char* path, const std::vector<u8>& data)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

// Helper: fuzz a volume object
static void fuzz_volume(DiscIO::VolumeDisc* vol)
{
    // Metadata — all exercise header parsing branches
    vol->GetGameID();
    vol->GetInternalName();
    vol->GetGameTDBID();
    vol->GetRevision();
    vol->GetDiscNumber();
    vol->GetVolumeType();
    vol->GetDataSize();
    vol->GetRawSize();
    vol->GetBlobType();

    std::vector<u8> buf(READ_CHUNK);

    // GC path (no partitions)
    const auto partitions = vol->GetPartitions();
    if (partitions.empty())
    {
        // Raw reads — exercises blob decompression at multiple offsets
        for (u64 off = 0; off < MAX_READ_OFFSET; off += READ_CHUNK)
            vol->Read(off, READ_CHUNK, buf.data(), DiscIO::PARTITION_NONE);

        // Filesystem parsing
        auto fs = vol->GetFileSystem(DiscIO::PARTITION_NONE);
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
                    // Name lookup exercises hash/comparison code
                    if (!name.empty())
                        fs->FindFileInfo(name);
                }
            }
        }
        return;
    }

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

// Try a format: patch magic, write temp, parse
static void try_format(std::vector<u8> data,
                       const u8* magic, size_t magic_offset, size_t magic_len,
                       const char* tmp_path)
{
    // Patch magic bytes
    if (data.size() < magic_offset + magic_len) return;
    memcpy(data.data() + magic_offset, magic, magic_len);

    if (!write_temp(tmp_path, data)) return;

    auto blob = DiscIO::CreateBlobReader(tmp_path);
    if (!blob) return;

    fuzz_blob(blob.get());

    auto volume = DiscIO::CreateDisc(std::move(blob));
    if (!volume) return;

    fuzz_volume(volume.get());
}

int main(int argc, const char* argv[])
{
    if (argc < 2) return 1;

    FILE* f = fopen(argv[1], "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len < 0x40 || len > MAX_INPUT_SIZE)
    {
        fclose(f);
        return 0;
    }

    std::vector<u8> data(len);
    if (fread(data.data(), 1, len, f) != (size_t)len)
    {
        fclose(f);
        return 0;
    }
    fclose(f);

    // Detect format from first bytes and only try ONE path
    // This keeps exec/sec high
    if (len >= 4 && memcmp(data.data(), CISO_MAGIC, 4) == 0)
        try_format(data, CISO_MAGIC, 0x00, 4, "/tmp/fuzz.ciso");
    else if (len >= 4 && memcmp(data.data(), WBFS_MAGIC, 4) == 0)
        try_format(data, WBFS_MAGIC, 0x00, 4, "/tmp/fuzz.wbfs");
    else if (len >= 4 && memcmp(data.data(), WIA_MAGIC, 4) == 0)
        try_format(data, WIA_MAGIC, 0x00, 4, "/tmp/fuzz.wia");
    else if (len >= 4 && memcmp(data.data(), RVZ_MAGIC, 4) == 0)
        try_format(data, RVZ_MAGIC, 0x00, 4, "/tmp/fuzz.rvz");
    else if (len >= 0x1C + 4 &&
             memcmp(data.data() + 0x18, WII_MAGIC, 4) == 0)
        try_format(data, WII_MAGIC, 0x18, 4, "/tmp/fuzz_wii.iso");
    else
        // Default: treat as GC ISO
        try_format(data, GC_MAGIC, 0x1C, 4, "/tmp/fuzz_gc.iso");

    return 0;
}
