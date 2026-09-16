# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4")
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4")
endif()
file(MAKE_DIRECTORY
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4-build"
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix"
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/tmp"
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4-stamp"
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src"
  "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/LZ4-prefix/src/BUILTIN_LZ4-stamp${cfgdir}") # cfgdir has leading slash
endif()
