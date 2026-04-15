// AFL++ Fuzzing Harness for DiscIO::CreateVolume
// Target: dolphin-emu/dolphin
//
// Build instructions:
//
//   1. Clone and configure Dolphin with fuzzing flags:
//
//      git clone --recurse-submodules https://github.com/dolphin-emu/dolphin.git
//      cd dolphin
//      mkdir build-fuzz && cd build-fuzz
//
//      CC=afl-clang-fast CXX=afl-clang-fast++ cmake .. \
//        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
//        -DENABLE_LTO=OFF \
//        -DENABLE_TESTS=OFF \
//        -DENABLE_QT=OFF \
//        -DENABLE_NOGUI=OFF \
//        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
//        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
//        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
//
//      make -j$(nproc) dolphin-emu-nogui core discio common
//
//   2. Compile this harness (from the repo root):
//
//      afl-clang-fast++ -std=c++20 \
//        -fsanitize=address,undefined \
//        -fno-omit-frame-pointer \
//        -I Source/Core \
//        -I Externals/fmt/include \
//        -I Externals/mbedtls/include \
//        fuzz_create_volume.cpp \
//        -o fuzz_create_volume \
//        -L build-fuzz/Source/Core/DiscIO \
//        -L build-fuzz/Source/Core/Common \
//        -ldiscio -lcommon \
//        -Wl,-rpath,build-fuzz/Source/Core/DiscIO \
//        -Wl,-rpath,build-fuzz/Source/Core/Common
//
//   3. Create seed corpus (representative disc/WAD headers):
//
//      mkdir -p corpus/
//      # Seed 1: Minimal GameCube disc header (magic at 0x1C = 0xC2339F3D)
//      python3 -c "
//        import struct
//        buf = bytearray(0x450)
//        # Game ID (6 bytes) + padding (2) + disc number + game version + streaming
//        buf[0:6] = b'GALE01'
//        buf[0x1C:0x20] = struct.pack('>I', 0xC2339F3D)  # GC magic
//        buf[0x20:0x24] = struct.pack('>I', 0x0D96E06B)  # Wii magic (absent)
//        open('corpus/gc_header.bin', 'wb').write(buf)
//      "
//      # Seed 2: Minimal Wii disc header
//      python3 -c "
//        import struct
//        buf = bytearray(0x50000)
//        buf[0:6] = b'RSBE01'
//        buf[0x18:0x1C] = struct.pack('>I', 0x5D1C9EA3)  # Wii magic
//        open('corpus/wii_header.bin', 'wb').write(buf)
//      "
//      # Seed 3: Minimal WAD header (type 0x4973 = 'Is', little-endian header size 0x20)
//      python3 -c "
//        import struct
//        buf = bytearray(0x200)
//        buf[0:4] = struct.pack('>I', 0x20)        # header size
//        buf[4:6] = b'Is'                          # WAD type
//        open('corpus/wad_header.bin', 'wb').write(buf)
//      "
//
//   4. Run the fuzzer:
//
//      AFL_SKIP_CPUFREQ=1 afl-fuzz \
//        -i corpus/ \
//        -o findings/ \
//        -m none \
//        -- ./fuzz_create_volume @@
//
//      Or use stdin mode (harness supports both):
//
//      AFL_SKIP_CPUFREQ=1 afl-fuzz \
//        -i corpus/ \
//        -o findings/ \
//        -m none \
//        -- ./fuzz_create_volume
//
// Notes:
//   - ASAN + UBSAN are strongly recommended (-fsanitize=address,undefined).
//   - For faster throughput, consider persistent mode (see bottom of this file).
//   - The harness exercises: magic-byte detection, BlobReader dispatch, partition
//     table parsing (Wii), file system construction (GC/Wii), WAD ticket/TMD
//     parsing, and all metadata accessors on the returned Volume.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Dolphin headers — adjust include paths to match your build layout.
#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/Filesystem.h"




// ---------------------------------------------------------------------------
// MemoryBlobReader — a BlobReader backed by an in-memory byte buffer.
// This avoids touching the filesystem and keeps the harness self-contained.
// ---------------------------------------------------------------------------
namespace
{

class MemoryBlobReader final : public DiscIO::BlobReader
{
public:
  explicit MemoryBlobReader(const uint8_t* data, size_t size)
      : m_data(data, data + size), m_size(static_cast<u64>(size))
  {
  }

	// Plain Blobs are considered ISO files.
	// Override GetBlobType()
  DiscIO::BlobType GetBlobType() const override { return DiscIO::BlobType::PLAIN; }


	// Override CopyReader()
  std::unique_ptr<DiscIO::BlobReader> CopyReader() const override
  {
    return std::make_unique<MemoryBlobReader>(m_data.data(), m_data.size());
  }


	// Override GetDataSize()
  // Total logical size of the disc image (what the Volume layer sees).
  u64 GetDataSize() const override { return m_size; }

	// Override GetDataSizeType()
  DiscIO::DataSizeType GetDataSizeType() const override
  {
    return DiscIO::DataSizeType::Accurate;
  }

	// Override GetRawSize()
  // Raw size == logical size for plain blobs.
  u64 GetRawSize() const override { return m_size; }


	// Override GetBlockSize()
  // Block size: report as 1 (no compression).
  u64 GetBlockSize() const override { return 0; }

	// Override HasFastRandomAccessInBlock()
  bool HasFastRandomAccessInBlock() const override { return true; }



	// Genuinely no idea what Claude was trying to do here. AllowCaching() doesn't exist in the parent
	// class; it fully hallucinated that. I'm wondering if it's maybe supposed to be IsCached()? That
	// exists in the parent, but I'm not sure if it's what we want. We'll try it and see.
//  bool AllowCaching() const override { return true; }
	bool IsCached() const override { return true; }

	// This also got hallucinated? BlobReader does have a GetName(), but it's not a virtual method.
  // std::string GetName() const override { return "fuzz_memory_blob"; }
	std::string GetName() const { return "fuzz_memory_blob"; }



	// Override Read()
  bool Read(u64 offset, u64 length, uint8_t* out_ptr) override
  {
    if (offset >= m_size || length > m_size - offset)
    {
      // Reads past the end: zero-fill so the Volume layer can keep going.
      const u64 available = (offset < m_size) ? (m_size - offset) : 0;
      if (available > 0)
        std::memcpy(out_ptr, m_data.data() + offset, static_cast<size_t>(available));
      if (length > available)
        std::memset(out_ptr + available, 0, static_cast<size_t>(length - available));
      return false;
    }
    std::memcpy(out_ptr, m_data.data() + offset, static_cast<size_t>(length));
    return true;
  }


	// Okay, for some reason, there's a bunch of functions that Claude did not feel
	// the need to override. That's probably why MemoryBlobReader is considered an
	// abstract class.
	// I'm gonna try to fix those, based off other BlobReader implementations in
	// the Dolphin repo.
  // Right, these are working, but they might be jank. Keep an eye on them.
	~MemoryBlobReader() override;
	std::string GetCompressionMethod() const override;
	std::optional<int> GetCompressionLevel() const override { return std::nullopt; }
	bool SupportsReadWiiDecrypted(u64 offset, u64 size, u64 partition_data_offset) const override;
	bool ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset) override;

private:
  std::vector<uint8_t> m_data;
  u64 m_size;
};





// ---------------------------------------------------------------------------
// exercise_volume — call common accessors to maximise code coverage.
// We deliberately ignore return values: we are hunting crashes/hangs, not
// testing correctness.
// ---------------------------------------------------------------------------
void exercise_volume(DiscIO::Volume& vol)
{
  const DiscIO::Partition part = DiscIO::PARTITION_NONE;

  // Basic metadata
  (void)vol.GetGameID();
  (void)vol.GetGameID(part);
  (void)vol.GetSyncHash();
  (void)vol.GetVolumeType();
  (void)vol.GetDataSize();
  (void)vol.GetRawSize();

  // Partition enumeration (Wii discs)
  const auto partitions = vol.GetPartitions();
  for (const auto& p : partitions)
  {
    (void)vol.GetPartitionType(p);
    (void)vol.GetTitleID(p);
    (void)vol.GetGameID(p);

    // Read small chunks from each partition
    std::vector<uint8_t> buf(0x440);
    (void)vol.Read(0, buf.size(), buf.data(), p);
  }

  // File system access
  const DiscIO::FileSystem* fs = vol.GetFileSystem(part);
  if (fs && fs->IsValid())
  {
    const DiscIO::FileInfo& root = fs->GetRoot();
    // Iterate top-level entries (limit to avoid infinite loops on corrupt data)
    int count = 0;
    for (auto it = root.cbegin(); it != root.cend() && count < 64; ++it, ++count)
    {
      (void)it->GetName();
      (void)it->GetSize();
      (void)it->IsDirectory();
    }
  }

  // WAD-specific accessors (no-op on disc volumes)
  (void)vol.GetTicket(part);
  (void)vol.GetTMD(part);
  (void)vol.GetCertificateChain(part);

  // Read small regions of the raw blob
  {
    constexpr size_t kReadSize = 0x100;
    std::vector<uint8_t> buf(kReadSize);
    for (u64 offset : {u64(0), u64(0x400), u64(0x40000), u64(0x50000)})
      (void)vol.Read(offset, kReadSize, buf.data(), part);
  }

  // Country / region / language
  (void)vol.GetCountry(part);
  (void)vol.GetRegion();
}

// ---------------------------------------------------------------------------
// fuzz_one — core fuzzing entry point called with raw mutated bytes.
// ---------------------------------------------------------------------------
void fuzz_one(const uint8_t* data, size_t size)
{
  // AFL++ may feed us empty or tiny inputs; guard against them.
  if (size == 0)
    return;

  auto reader = std::make_unique<MemoryBlobReader>(data, size);

  // CreateVolume inspects magic bytes to choose between VolumeGC, VolumeWii,
  // and VolumeWAD.  A nullptr return means the input was not recognised — that
  // is expected and not a bug.
  std::unique_ptr<DiscIO::Volume> vol = DiscIO::CreateVolume(std::move(reader));
  if (!vol)
    return;

  // Drive as much of the Volume implementation as possible.
  exercise_volume(*vol);
}

// ---------------------------------------------------------------------------
// Read all bytes from a FILE* into a vector.
// ---------------------------------------------------------------------------
std::vector<uint8_t> read_file(FILE* f)
{
  std::vector<uint8_t> buf;
  uint8_t chunk[4096];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
    buf.insert(buf.end(), chunk, chunk + n);
  return buf;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// main — supports two modes:
//   1. File mode:  ./fuzz_create_volume <path>   (used with afl-fuzz @@ )
//   2. Stdin mode: ./fuzz_create_volume           (used with afl-fuzz  )
//
// AFL++ persistent-mode macro is used when available to avoid process
// re-spawning overhead; fall back to single-shot execution otherwise.
// ---------------------------------------------------------------------------

#ifdef __AFL_FUZZ_TESTCASE_BUF
// -------------------------------------------------------------------------
// AFL++ persistent mode — fastest throughput.
// Requires compiling with afl-clang-fast / afl-clang-lto.
// -------------------------------------------------------------------------
__AFL_FUZZ_INIT();

int main()
{
  __AFL_INIT();
  const uint8_t* buf = __AFL_FUZZ_TESTCASE_BUF;
  while (__AFL_LOOP(10000))
  {
    const size_t len = __AFL_FUZZ_TESTCASE_LEN;
    fuzz_one(buf, len);
  }
  return 0;
}

#else
// -------------------------------------------------------------------------
// Non-persistent / file-based mode.
// -------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  std::vector<uint8_t> data;

  if (argc >= 2)
  {
    // File path supplied (afl-fuzz @@ style).
    FILE* f = std::fopen(argv[1], "rb");
    if (!f)
    {
      std::perror("fopen");
      return 1;
    }
    data = read_file(f);
    std::fclose(f);
  }
  else
  {
    // Read from stdin (afl-fuzz pipe style).
    data = read_file(stdin);
  }

  fuzz_one(data.data(), data.size());
  return 0;
}
#endif  // __AFL_FUZZ_TESTCASE_BUF
