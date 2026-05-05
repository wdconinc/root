// @(#)root/wasm:$Id$
// Author: ROOT Team   2025

/*************************************************************************
 * Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                       //
// TEmscriptenSystem                                                     //
//                                                                       //
// See header for full documentation.                                   //
//                                                                       //
//////////////////////////////////////////////////////////////////////////

#include "TEmscriptenSystem.h"
#include "TError.h"
#include "TInetAddress.h"

#include <cstdlib>
#include <cstring>
#include <emscripten.h>

// No ClassImp: rootcling is not available in WASM cross-compilation.

////////////////////////////////////////////////////////////////////////////////
/// Construct a TEmscriptenSystem.

TEmscriptenSystem::TEmscriptenSystem() : TUnixSystem()
{
}

TEmscriptenSystem::~TEmscriptenSystem() = default;

////////////////////////////////////////////////////////////////////////////////
/// Return a home directory that is always valid in the WASM sandbox.
/// In browser WASM, Emscripten's virtual FS always has /tmp writable.
/// In Node.js testing mode, /tmp is also reliably available.
/// We do NOT rely on getenv("HOME") because Emscripten's MODULARIZE mode
/// does not automatically propagate the host environment.

const char *TEmscriptenSystem::HomeDirectory(const char * /*userName*/)
{
   return "/tmp";
}

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// Initialise the Emscripten system layer.
/// Delegates most setup to TUnixSystem::Init(), then adapts settings that
/// are inappropriate inside a browser sandbox.

Bool_t TEmscriptenSystem::Init()
{
   Bool_t ok = TUnixSystem::Init();
   // SetDisplay() is intentionally NOT called: it may invoke getnameinfo() or
   // open a UDP socket for DNS, neither of which works in WASM's SOCKFS.
   return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// Return a host name for the WASM environment.

const char *TEmscriptenSystem::HostName()
{
   return "localhost";
}

////////////////////////////////////////////////////////////////////////////////
/// SetDisplay is a no-op in WebAssembly.
/// The base class implementation opens utmp and may call getnameinfo(), which
/// in Emscripten's SOCKFS would attempt a UDP DNS query and crash.

void TEmscriptenSystem::SetDisplay()
{
   // intentional no-op
}

////////////////////////////////////////////////////////////////////////////////
/// Return a TInetAddress without performing DNS resolution.
/// DNS via UDP is not available in Emscripten's SOCKFS environment.
/// Returns an invalid TInetAddress; callers must handle the IsValid()==false case.

TInetAddress TEmscriptenSystem::GetHostByName(const char * /*hostname*/)
{
   return TInetAddress();
}

//---- Process management — not available in WASM --------------------------------

////////////////////////////////////////////////////////////////////////////////
/// Execute a shell command — not supported in browser WASM.

Int_t TEmscriptenSystem::Exec(const char * /*shellcmd*/)
{
   Error("Exec", "process spawning is not supported in WebAssembly");
   return -1;
}

////////////////////////////////////////////////////////////////////////////////
/// Open a pipe to a shell command — not supported in browser WASM.

FILE *TEmscriptenSystem::OpenPipe(const char * /*shellcmd*/, const char * /*mode*/)
{
   Error("OpenPipe", "pipes are not supported in WebAssembly");
   return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::ClosePipe(FILE * /*pipe*/)
{
   Error("ClosePipe", "pipes are not supported in WebAssembly");
   return -1;
}

////////////////////////////////////////////////////////////////////////////////
/// Print a stack trace — no native unwinder in WASM.

void TEmscriptenSystem::StackTrace()
{
   // Emscripten provides a JS-level stack trace via emscripten_run_script.
   // Print it if available; otherwise emit a human-readable message.
   EM_ASM({
      if (typeof console !== 'undefined') {
         console.trace('ROOT StackTrace requested');
      }
   });
}

////////////////////////////////////////////////////////////////////////////////

void TEmscriptenSystem::Exit(int code, Bool_t /*mode*/)
{
   ::exit(code);
}

////////////////////////////////////////////////////////////////////////////////

void TEmscriptenSystem::Abort(int code)
{
   ::abort();
   (void)code;
}

//---- Dynamic loading — static linking only in WASM ----------------------------

////////////////////////////////////////////////////////////////////////////////
/// Dynamic loading is not supported for the static WASM build.
/// All ROOT libraries must be linked at compile time.

int TEmscriptenSystem::Load(const char *module, const char * /*entry*/, Bool_t /*system*/)
{
   if (module)
      Warning("Load", "dynamic loading of '%s' is not supported in WebAssembly; "
              "ensure all required libraries are linked statically", module);
   return -1;
}

////////////////////////////////////////////////////////////////////////////////

void TEmscriptenSystem::Unload(const char * /*module*/)
{
   // nothing to do — no dynamic loading
}

////////////////////////////////////////////////////////////////////////////////

Func_t TEmscriptenSystem::DynFindSymbol(const char * /*module*/, const char *entry)
{
   // All symbols are statically linked; use dlsym(RTLD_DEFAULT) to locate them.
   // Emscripten supports dlsym on the main module when built with
   // -s MAIN_MODULE or when the symbol is exported.
   if (!entry)
      return nullptr;
   return TUnixSystem::DynFindSymbol(nullptr, entry);
}

//---- TCP/UDP socket services — no OS-level bind/listen in browser WASM ---------

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::AnnounceTcpService(int /*port*/, Bool_t /*reuse*/,
                                           int /*backlog*/, int /*tcpwindowsize*/,
                                           ESocketBindOption /*bindOpt*/)
{
   Error("AnnounceTcpService",
         "server-side TCP sockets are not supported in WebAssembly");
   return -1;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::AnnounceUdpService(int /*port*/, int /*backlog*/,
                                           ESocketBindOption /*bindOpt*/)
{
   Error("AnnounceUdpService",
         "server-side UDP sockets are not supported in WebAssembly");
   return -1;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::AnnounceUnixService(int /*port*/, int /*backlog*/)
{
   Error("AnnounceUnixService",
         "Unix-domain sockets are not supported in WebAssembly");
   return -1;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::AnnounceUnixService(const char * /*sockpath*/, int /*backlog*/)
{
   Error("AnnounceUnixService",
         "Unix-domain sockets are not supported in WebAssembly");
   return -1;
}

//---- System / CPU / memory info — not meaningful in WASM ----------------------

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::GetSysInfo(SysInfo_t *info) const
{
   if (!info) return -1;
   // Provide minimal plausible values for the WASM sandbox.
   *info = {};
   info->fOS      = "WebAssembly";
   info->fModel   = "Emscripten";
   info->fCpus    = 1;
   info->fPhysRam = 0; // unknown inside browser
   return 0;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::GetCpuInfo(CpuInfo_t *info, Int_t /*sampleTime*/) const
{
   if (!info) return -1;
   *info = {};
   return 0;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::GetMemInfo(MemInfo_t *info) const
{
   if (!info) return -1;
   *info = {};
   return 0;
}

////////////////////////////////////////////////////////////////////////////////

int TEmscriptenSystem::GetProcInfo(ProcInfo_t *info) const
{
   if (!info) return -1;
   *info = {};
   return 0;
}
