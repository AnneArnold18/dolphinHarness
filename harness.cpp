#include "Core/Boot/Boot.h"
#include "Core/Host.h"
#include "DiscIO/Blob.h"

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
        if (argc < 2)
        {
                printf("./harness filepath\n");
                return 1;
        }


        std::unique_ptr<DiscIO::BlobReader> blob = DiscIO::CreateBlobReader(argv[1]);

	// Check for a null blob
        if (blob == nullptr)
	{
//		printf("Blob Creation failed\n");
		return 0;
	}

	else
	{
//		printf("Blob Creation passed\n");
	}


	// Checking the ReadSwapped for the blob.
	// CreateDisc will only succeed if ReadSwapped(0x18) or ReadSwapped(0x1C) passes and is a certain value

	auto opt = blob->ReadSwapped<u32>(0x18);
	if (opt == std::nullopt)
	{
		printf("Read Swapped 0x18 failed\n");
	}
	else
	{
		printf("\n");
		printf("Read Swapped 0x18 passed, gave 0x%X\n", opt.value());
		printf("Expected value   -----------   0x5D1C9EA3\n");
		printf("\n");
	}

	auto opt2 = blob->ReadSwapped<u32>(0x1c);
	if (opt2 == std::nullopt)
	{
		printf("Read Swapped 0x1C failed\n");
	}
	else
	{
		printf("\n");
		printf("Read Swapped 0x1C passed, gave 0x%X\n", opt2.value());
		printf("Expected value   -----------   0xC2339F3D\n");
		printf("\n");
	}

	auto volume = DiscIO::CreateDisc(move(blob));
	if (!volume)
	{
//		printf("Create Disc failed\n");
		return 1;
	}
	else
	{
//		printf("Create Disc passed\n");
	}

	// I don't believe any error checking needs to be done here.
	auto part = volume->GetGamePartition();

	u8* buffer;
	u64 i = 0;
	while(volume->Read(i, 64, buffer, part))
	{
		i += 64;
	}


	return 0;
}
