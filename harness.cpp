#include "Core/Boot/Boot.h"
#include "Core/Host.h"

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
	printf("Running harness...\n");

	if (argc < 2)
	{
		printf("./harness filepath\n");
		return 1;
	}

	string path = argv[1];

	std::vector<string> paths = {path};
	std::optional<string> opt = "optional string";
	auto params = BootParameters::GenerateFromFile(
	    std::move(paths),
	    BootSessionData(std::move(opt), DeleteSavestateAfterBoot::Yes));

	return 0;
}
