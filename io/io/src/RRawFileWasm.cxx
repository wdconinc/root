// @(#)root/io:$Id$
// Author: ROOT Team   2025

/*************************************************************************
 * Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RRawFileWasm.hxx>
#include "TError.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <emscripten/fetch.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

/// Strip the "file://" scheme prefix from a path string, if present.
std::string StripFileScheme(std::string_view url)
{
   if (url.substr(0, 7) == "file://")
      return std::string(url.substr(7));
   return std::string(url);
}

/// Return true when the URL uses the http or https scheme.
bool IsHttp(std::string_view url)
{
   return url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://";
}

} // anonymous namespace

ROOT::Internal::RRawFileWasm::RRawFileWasm(std::string_view url, ROptions options)
   : RRawFile(url, options), fResolvedUrl(std::string(url)), fIsHttp(IsHttp(url))
{
}

ROOT::Internal::RRawFileWasm::~RRawFileWasm()
{
   if (fFileDes >= 0)
      close(fFileDes);
}

std::unique_ptr<ROOT::Internal::RRawFile> ROOT::Internal::RRawFileWasm::Clone() const
{
   return std::make_unique<RRawFileWasm>(fUrl, fOptions);
}

void ROOT::Internal::RRawFileWasm::OpenImpl()
{
   if (fIsHttp) {
      // HTTP: nothing to open eagerly; reads are done via emscripten_fetch().
      return;
   }

   // Local (MEMFS) path.
   std::string path = StripFileScheme(fUrl);
   fFileDes = open(path.c_str(), O_RDONLY);
   if (fFileDes < 0)
      throw std::runtime_error("Cannot open '" + fUrl + "': " + std::string(strerror(errno)));
}

std::uint64_t ROOT::Internal::RRawFileWasm::GetSizeImpl()
{
   if (fIsHttp)
      return FetchSize();

   struct stat info;
   if (fstat(fFileDes, &info) != 0)
      throw std::runtime_error("Cannot stat '" + fUrl + "': " + std::string(strerror(errno)));
   return static_cast<std::uint64_t>(info.st_size);
}

size_t ROOT::Internal::RRawFileWasm::ReadAtImpl(void *buffer, size_t nbytes, std::uint64_t offset)
{
   if (fIsHttp)
      return FetchRange(buffer, nbytes, offset);

   // Local (MEMFS) read.
   ssize_t res = pread(fFileDes, buffer, nbytes, static_cast<off_t>(offset));
   if (res < 0)
      throw std::runtime_error("Cannot read from '" + fUrl + "': " + std::string(strerror(errno)));
   return static_cast<size_t>(res);
}

//------------------------------------------------------------------------------
// HTTP fetch helpers (Emscripten-specific)
//------------------------------------------------------------------------------

size_t ROOT::Internal::RRawFileWasm::FetchRange(void *buffer, size_t nbytes, std::uint64_t offset)
{
   if (nbytes == 0)
      return 0;

   // Build a "Range: bytes=<start>-<end>" request header.
   std::uint64_t end = offset + nbytes - 1;
   std::string rangeValue =
      "bytes=" + std::to_string(offset) + "-" + std::to_string(end);
   const char *headerNames[]  = {"Range", nullptr};
   const char *headerValues[] = {rangeValue.c_str(), nullptr};

   emscripten_fetch_attr_t attr;
   emscripten_fetch_attr_init(&attr);
   attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
   attr.requestHeaders = headerNames;
   // emscripten_fetch_attr_t::requestHeaderValues is set via requestHeaders pairs
   // (name, value alternating); Emscripten 4.x uses requestHeaders as a flat
   // null-terminated pairs array when compiled with the newer API.
   // For compatibility we set both the header names array and values.
   // Note: the API changed between emsdk versions; below follows emsdk ≥3.1.
   attr.requestHeaders = headerNames;
   // Provide header values via the userData trick won't work; use the correct API:
   // In Emscripten the overloadedMimeType can carry the range header in some builds.
   // The standard way for emsdk ≥ 3.1 is to use attr.requestHeaders as interleaved pairs.
   // We reconstruct accordingly:
   const char *headerPairs[] = {"Range", rangeValue.c_str(), nullptr};
   attr.requestHeaders = headerPairs;
   ::strcpy(attr.requestMethod, "GET");

   emscripten_fetch_t *fetch = emscripten_fetch(&attr, fResolvedUrl.c_str());
   if (!fetch) {
      Error("RRawFileWasm::FetchRange", "emscripten_fetch returned null for '%s'", fResolvedUrl.c_str());
      return 0;
   }

   size_t copied = 0;
   // HTTP 206 Partial Content or 200 OK (server may ignore Range).
   if (fetch->status == 206 || fetch->status == 200) {
      copied = std::min(static_cast<size_t>(fetch->numBytes), nbytes);
      std::memcpy(buffer, fetch->data, copied);
   } else {
      Error("RRawFileWasm::FetchRange",
            "HTTP %d fetching range %s of '%s'",
            static_cast<int>(fetch->status), rangeValue.c_str(), fResolvedUrl.c_str());
   }
   emscripten_fetch_close(fetch);
   return copied;
}

std::uint64_t ROOT::Internal::RRawFileWasm::FetchSize()
{
   emscripten_fetch_attr_t attr;
   emscripten_fetch_attr_init(&attr);
   attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
   ::strcpy(attr.requestMethod, "HEAD");

   emscripten_fetch_t *fetch = emscripten_fetch(&attr, fResolvedUrl.c_str());
   if (!fetch)
      throw std::runtime_error("emscripten_fetch HEAD failed for '" + fResolvedUrl + "'");

   std::uint64_t size = 0;
   if (fetch->status == 200 || fetch->status == 206) {
      size = static_cast<std::uint64_t>(fetch->totalBytes);
   } else {
      emscripten_fetch_close(fetch);
      throw std::runtime_error("HTTP HEAD " + std::to_string(fetch->status) +
                                " for '" + fResolvedUrl + "'");
   }
   emscripten_fetch_close(fetch);
   return size;
}
