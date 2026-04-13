#include "Core/Boot/Boot.h"
#include "Core/Host.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/FileSystemGCWii.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using std::string;

// --- Stub Host callbacks required by libcore ---
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

int main(int argc, const char* argv[])
{
	if (argc < 2)  return 1;

	auto blob = DiscIO::CreateBlobReader(argv[1]);
	if (!blob) { return 0; }

	printf("blob is not null!");

	// read only the header file of the WIA file
	const u64 file_size = blob->GetDataSize();
	std::vector<u8> buf(0x10000);	// enough size to cover the header

	blob->Read(0, std::min((u64)buf.size(), file_size), buf.data());	// read header
	for (u64 offset = 0; offset < file_size; offset += 0x8000)
    	{
        	u64 to_read = std::min((u64)0x8000, file_size - offset);
        	blob->Read(offset, to_read, buf.data());
    	}

	printf("blob is read into the buffer!");

	auto volume = DiscIO::CreateDisc(std::move(blob));
	if (!volume)	return 0;

	printf("We have created the volume object from disc");

	volume->GetGameID();
	volume->GetInternalName();
	volume->GetGameTDBID();

	const auto partitions = volume->GetPartitions();
	printf("Number of partitions is %zu\n", partitions.size());

	if (partitions.empty())
	{
		auto file_sys = volume->GetFileSystem(DiscIO::PARTITION_NONE);
		if (file_sys)
    		{

			printf("Got filesystem!\n");
			auto root = file_sys->FindFileInfo("/");
			if (root)
			{
				for (const auto& child : *root)
				{
					child.GetName();
					child.GetSize();
				}
			}
		}

		std::vector<u8> sector(0x8000);
		for (u64 offset = 0; offset < 0x100000; offset += 0x8000)
		{
			volume->Read(offset, sector.size(), sector.data(), DiscIO::PARTITION_NONE);
		}
	}

	for (const auto& partition : partitions)
	{
		volume->GetPartitionType(partition);
		auto file_sys = volume->GetFileSystem(partition);
		if (file_sys)
		{
			auto root = file_sys->FindFileInfo("/");
			if (root)
			{
				for (const auto& child : *root)
				{
					child.GetName();
					child.GetSize();
				}
			}
		}

		std::vector<u8> sector(0x8000);
        	volume->Read(0, sector.size(), sector.data(), partition);

	}

	return 0;
}
