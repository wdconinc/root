// @(#)root/io:$Id$
// Author: ROOT Team   2025

/*************************************************************************
 * Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RRawFileWasm
#define ROOT_RRawFileWasm

#include <ROOT/RRawFile.hxx>
#include <string_view>

#include <cstddef>
#include <cstdint>
#include <string>

namespace ROOT {
namespace Internal {

/**
 * \class RRawFileWasm RRawFileWasm.hxx
 * \ingroup IO
 *
 * RRawFileWasm provides read-only file access in WebAssembly / Emscripten
 * environments.  It handles two cases:
 *
 * 1. **Local (MEMFS) paths** – URLs with the `file://` scheme or bare paths
 *    are read through the Emscripten virtual file-system (MEMFS by default)
 *    using standard POSIX read calls, exactly like RRawFileUnix.
 *
 * 2. **HTTP(S) URLs** – Files served over HTTP or HTTPS are fetched with
 *    `emscripten_fetch()` using byte-range requests so that only the needed
 *    portions of a large remote ROOT file are transferred.  The synchronous
 *    fetch mode (`EMSCRIPTEN_FETCH_SYNCHRONOUS`) is used; it requires either:
 *      - that the file is accessed from a Web Worker thread, or
 *      - that the build is compiled with `-s ASYNCIFY=1`.
 *    When called from the browser main thread without Asyncify, an error is
 *    reported and an empty read is returned.
 *
 * This class is compiled only when `__EMSCRIPTEN__` is defined.
 */
class RRawFileWasm : public RRawFile {
private:
   int fFileDes = -1;           ///< File descriptor for local (MEMFS) access
   std::string fResolvedUrl;    ///< URL with scheme normalised
   bool fIsHttp = false;        ///< True when using HTTP(S) fetch

   /// Fetch [offset, offset+nbytes) from the remote URL via emscripten_fetch.
   size_t FetchRange(void *buffer, size_t nbytes, std::uint64_t offset);

   /// Return the remote file size via a HEAD request.
   std::uint64_t FetchSize();

protected:
   void OpenImpl() final;
   size_t ReadAtImpl(void *buffer, size_t nbytes, std::uint64_t offset) final;
   std::uint64_t GetSizeImpl() final;

public:
   RRawFileWasm(std::string_view url, RRawFile::ROptions options);
   ~RRawFileWasm() override;
   std::unique_ptr<RRawFile> Clone() const final;
};

} // namespace Internal
} // namespace ROOT

#endif // ROOT_RRawFileWasm
