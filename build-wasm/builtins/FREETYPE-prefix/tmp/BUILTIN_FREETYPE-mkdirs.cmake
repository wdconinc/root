# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE")
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE")
endif()
file(MAKE_DIRECTORY
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE-build"
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix"
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/tmp"
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE-stamp"
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src"
  "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/wdconinc/git/root/build-wasm/builtins/FREETYPE-prefix/src/BUILTIN_FREETYPE-stamp${cfgdir}") # cfgdir has leading slash
endif()
