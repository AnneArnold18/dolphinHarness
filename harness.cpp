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
		return 1;
	}

/*
	else
	{
//		printf("Blob Creation passed\n");
	}
*/


	// CreateVolume() is the method we're targeting
	CreateVolume(move(blob));

	return 0;
}
