CXX = afl-clang-fast++
CXXFLAGS = -std=c++23

# Dolphin source and build root (same tree in this case)
DOLPHIN_SRC = .
DOLPHIN_BUILD = build

# Include paths: Source/Core for main headers, build/Source/Core for generated headers (scmrev.h),
# and Externals needed by Dolphin headers.
INCLUDES = -I$(DOLPHIN_SRC)/Source/Core \
           -I$(DOLPHIN_BUILD)/Source/Core \
           -I$(DOLPHIN_SRC)/Externals/fmt/fmt/include \
           -I$(DOLPHIN_SRC)/Externals/picojson \
           -I$(DOLPHIN_SRC)/Externals/minizip-ng \
           -I$(DOLPHIN_SRC)/Externals/ed25519 \
           -I$(DOLPHIN_SRC)/Externals/rcheevos/include \
           -I$(DOLPHIN_SRC)/Externals/lz4/lz4/lib \
           -I$(DOLPHIN_SRC)/Externals/zlib-ng/zlib-ng \
           -I$(DOLPHIN_BUILD)/Externals/zlib-ng/zlib-ng \
           -I$(DOLPHIN_SRC)/Externals/mbedtls/include \
           -I$(DOLPHIN_SRC)/Externals/pugixml/pugixml/src \
           -I$(DOLPHIN_SRC)/Externals/cubeb/cubeb/include \
           -I$(DOLPHIN_SRC)/Externals/xxhash \
           -I$(DOLPHIN_SRC)/Externals/libspng/libspng/spng

# Dolphin core static libraries (order matters for static linking – dependents before dependencies)
DOLPHIN_LIBS = \
    $(DOLPHIN_BUILD)/Source/Core/Core/libcore.a \
    $(DOLPHIN_BUILD)/Source/Core/UICommon/libuicommon.a \
    $(DOLPHIN_BUILD)/Source/Core/DiscIO/libdiscio.a \
    $(DOLPHIN_BUILD)/Source/Core/VideoBackends/Null/libvideonull.a \
    $(DOLPHIN_BUILD)/Source/Core/VideoBackends/OGL/libvideoogl.a \
    $(DOLPHIN_BUILD)/Source/Core/VideoBackends/Software/libvideosoftware.a \
    $(DOLPHIN_BUILD)/Source/Core/VideoBackends/Vulkan/libvideovulkan.a \
    $(DOLPHIN_BUILD)/Source/Core/VideoCommon/libvideocommon.a \
    $(DOLPHIN_BUILD)/Source/Core/AudioCommon/libaudiocommon.a \
    $(DOLPHIN_BUILD)/Source/Core/InputCommon/libinputcommon.a \
    $(DOLPHIN_BUILD)/Source/Core/Common/libcommon.a

# Bundled external static libraries
EXTERN_LIBS = \
    $(DOLPHIN_BUILD)/Externals/fmt/fmt/libfmt.a \
    $(DOLPHIN_BUILD)/Externals/pugixml/pugixml/libpugixml.a \
    $(DOLPHIN_BUILD)/Externals/minizip-ng/minizip-ng/libminizip-ng.a \
    $(DOLPHIN_BUILD)/Externals/lz4/lz4/build/cmake/liblz4.a \
    $(DOLPHIN_BUILD)/Externals/zlib-ng/zlib-ng/libz.a \
    $(DOLPHIN_BUILD)/Externals/LZO/liblzo2.a \
    $(DOLPHIN_BUILD)/Externals/mbedtls/library/libmbedtls.a \
    $(DOLPHIN_BUILD)/Externals/mbedtls/library/libmbedx509.a \
    $(DOLPHIN_BUILD)/Externals/mbedtls/library/libmbedcrypto.a \
    $(DOLPHIN_BUILD)/Externals/xxhash/libxxhash.a \
    $(DOLPHIN_BUILD)/Externals/libspng/libspng/libspng_static.a \
    $(DOLPHIN_BUILD)/Externals/SFML/libsfml-network.a \
    $(DOLPHIN_BUILD)/Externals/SFML/libsfml-system.a \
    $(DOLPHIN_BUILD)/Externals/enet/enet/libenet.a \
    $(DOLPHIN_BUILD)/Externals/cubeb/libcubeb.a \
    $(DOLPHIN_BUILD)/Externals/FreeSurround/libFreeSurround.a \
    $(DOLPHIN_BUILD)/Externals/FatFs/libFatFs.a \
    $(DOLPHIN_BUILD)/Externals/rcheevos/librcheevos.a \
    $(DOLPHIN_BUILD)/Externals/Bochs_disasm/libbdisasm.a \
    $(DOLPHIN_BUILD)/Externals/curl/curl/lib/libcurl.a \
    $(DOLPHIN_BUILD)/Externals/miniupnpc/miniupnp/miniupnpc/libminiupnpc.a \
    $(DOLPHIN_BUILD)/Externals/hidapi/libhidapi.a \
    $(DOLPHIN_BUILD)/Externals/libusb/libusb.a \
    $(DOLPHIN_BUILD)/Externals/SDL/SDL/libSDL3.a \
    $(DOLPHIN_BUILD)/Externals/cpp-optparse/libcpp-optparse.a \
    $(DOLPHIN_BUILD)/Externals/tinygltf/libtinygltf.a \
    $(DOLPHIN_BUILD)/Externals/imgui/libimgui.a \
    $(DOLPHIN_BUILD)/Externals/implot/libimplot.a \
    $(DOLPHIN_BUILD)/lib/libipc.a

# System-installed static libraries (glslang / SPIRV)
GLSLANG_LIBS = \
    /usr/lib/x86_64-linux-gnu/libSPIRV.a \
    /usr/lib/x86_64-linux-gnu/libglslang.a \
    /usr/lib/x86_64-linux-gnu/libMachineIndependent.a \
    /usr/lib/x86_64-linux-gnu/libGenericCodeGen.a \
    /usr/lib/x86_64-linux-gnu/libOSDependent.a \
    -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV-Tools-link

# System libraries needed at final link
SYS_LIBS = -lpthread -ldl -lrt -lm -levdev -ludev \
           -lbz2 -llzma -lzstd \
           -lX11 -lXi -lXrandr \
           -lEGL -lOpenGL -lGLX \
           /usr/lib/llvm-18/lib/libLLVM.so.1

SRC = harness.cpp
OUT = harness

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(OUT) \
		-Wl,--start-group $(DOLPHIN_LIBS) $(EXTERN_LIBS) $(GLSLANG_LIBS) -Wl,--end-group \
		$(SYS_LIBS)

clean:
	rm -f $(OUT)
