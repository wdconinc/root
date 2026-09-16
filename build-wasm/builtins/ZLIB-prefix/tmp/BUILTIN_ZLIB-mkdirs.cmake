# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB")
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB")
endif()
file(MAKE_DIRECTORY
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB-build"
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix"
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/tmp"
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB-stamp"
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src"
  "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/ZLIB-prefix/src/BUILTIN_ZLIB-stamp${cfgdir}") # cfgdir has leading slash
endif()
