# Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

#---Emscripten cross-compilation toolchain file for ROOT-----------------------------------
#
# Usage:
#   emcmake cmake <ROOT_SRC> -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Emscripten.cmake \
#       [options]
#
# Or let emcmake set the toolchain automatically (recommended):
#   emcmake cmake <ROOT_SRC> -Dwasm=ON [options]
#
# Recommended minimal configuration:
#   emcmake cmake <ROOT_SRC>             \
#     -DCMAKE_BUILD_TYPE=Release          \
#     -Dminimal=ON                        \
#     -Dwasm=ON                           \
#     -Dbuiltin_zlib=ON                   \
#     -Dbuiltin_lz4=ON                    \
#     -Dbuiltin_zstd=ON                   \
#     -Dbuiltin_xxhash=ON                 \
#     -Dbuiltin_pcre=ON
#
# Prerequisites:
#   - emsdk >= 4.0.9 installed and activated (source emsdk_env.sh)
#   - The EMSCRIPTEN environment variable must point to the emscripten root
#
# Notes:
#   - This toolchain is automatically detected when emcmake is used, as emcmake
#     sets CMAKE_TOOLCHAIN_FILE to the Emscripten.cmake from the emsdk.
#   - This file supplements the upstream emsdk toolchain with ROOT-specific settings.
#   - The interpreter (Cling/CppInterOp) requires a separate WASM LLVM build;
#     disable it for the minimal MVP build with -Dcling=OFF.
#--------------------------------------------------------------------------------------------

# Include the upstream Emscripten toolchain provided by the emsdk if invoked standalone.
# When using emcmake, the upstream toolchain is already loaded; avoid double-inclusion.
if(NOT DEFINED EMSCRIPTEN AND DEFINED ENV{EMSCRIPTEN})
  include("$ENV{EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake" OPTIONAL)
endif()

# Identify this as a WASM build for ROOT's CMake logic
set(ROOT_EMSCRIPTEN_BUILD TRUE CACHE BOOL "Building ROOT for WebAssembly via Emscripten" FORCE)

# Default output suffix for WASM/JS bundles
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Emscripten-specific compile/link flags
set(ROOT_EMSCRIPTEN_CXX_FLAGS
  "-fwasm-exceptions"         # Use native WASM exceptions (requires LLVM ≥19 build)
  "-fno-rtti"                 # Reduce binary size; ROOT itself enables RTTI selectively
  CACHE STRING "Extra C++ flags for WASM build")

# Use -Oz + LTO for release builds to minimize binary size
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  add_compile_options(-Oz -flto)
  add_link_options(-Oz -flto)
endif()

# Enable WASM BigInt for 64-bit integer interop with JavaScript
add_link_options("SHELL:-s WASM_BIGINT=1")

# Allow the initial heap to grow dynamically (ROOT needs significant heap)
add_link_options("SHELL:-s ALLOW_MEMORY_GROWTH=1")

# pthreads support: disabled by default for the minimal MVP.
# To enable threading, set -DROOT_WASM_THREADS=ON and ensure your web server
# sends the required COOP/COEP headers (SharedArrayBuffer).
option(ROOT_WASM_THREADS "Enable pthreads support in WASM build (requires SharedArrayBuffer)" OFF)
if(ROOT_WASM_THREADS)
  add_compile_options(-pthread)
  add_link_options(-pthread "SHELL:-s PTHREAD_POOL_SIZE=4")
endif()

# Asyncify enables async I/O patterns (e.g., fetching remote ROOT files).
# Adds binary size overhead; only enable if needed.
option(ROOT_WASM_ASYNCIFY "Enable Emscripten Asyncify for async I/O" OFF)
if(ROOT_WASM_ASYNCIFY)
  add_link_options("SHELL:-s ASYNCIFY=1")
endif()

# Emscripten virtual filesystem: enable NODEFS for access to the host FS
# in Node.js environments (useful for testing).
option(ROOT_WASM_NODEFS "Mount Emscripten NODEFS for Node.js FS access" OFF)
if(ROOT_WASM_NODEFS)
  add_link_options("SHELL:-libnodefs.js")
endif()

# Force-include Emscripten fetch support for HTTP I/O (TWebFile)
add_link_options("SHELL:-s FETCH=1")

# Modularize output so ROOT can be imported as an ES module / CommonJS module
option(ROOT_WASM_MODULARIZE "Wrap output in a JS module factory function" ON)
if(ROOT_WASM_MODULARIZE)
  add_link_options("SHELL:-s MODULARIZE=1" "SHELL:-s EXPORT_NAME=createROOT")
endif()
