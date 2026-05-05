// @(#)root/wasm:$Id$
// Author: ROOT Team   2025

/*************************************************************************
 * Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TEmscriptenSystem
#define ROOT_TEmscriptenSystem

//////////////////////////////////////////////////////////////////////////
//                                                                       //
// TEmscriptenSystem                                                     //
//                                                                       //
// Specialisation of TUnixSystem for WebAssembly / Emscripten builds.   //
//                                                                       //
// Emscripten provides a POSIX-compatible environment inside the browser //
// (filesystem via MEMFS, environment variables, time, etc.), so most   //
// of TUnixSystem works unchanged.  This subclass overrides the parts   //
// that are unavailable or behave differently in a WASM sandbox:        //
//                                                                       //
//  - Process spawning (Exec, OpenPipe, ClosePipe) — no fork/exec       //
//  - Raw TCP/UDP socket services — no OS-level bind/listen in browser  //
//  - Dynamic library loading (Load/Unload) — static linking only       //
//  - Stack trace — no native unwinder available                        //
//  - System / CPU / memory info — not meaningful in WASM               //
//                                                                       //
// Filesystem access is delegated to TUnixSystem which in turn uses     //
// Emscripten's virtual filesystem (MEMFS by default, IDBFS or NODEFS   //
// can be mounted at startup from JavaScript).                           //
//                                                                       //
//////////////////////////////////////////////////////////////////////////

#include "TUnixSystem.h"

class TEmscriptenSystem : public TUnixSystem {

public:
   TEmscriptenSystem();
   ~TEmscriptenSystem() override;

   //---- Misc -------------------------------------------------------
   Bool_t            Init() override;
   const char       *HostName() override;
   const char       *HomeDirectory(const char *userName = nullptr) override;

   //---- Process management — unavailable in WASM ------------------
   Int_t             Exec(const char *shellcmd) override;
   FILE             *OpenPipe(const char *shellcmd, const char *mode) override;
   int               ClosePipe(FILE *pipe) override;
   void              StackTrace() override;
   void              Exit(int code, Bool_t mode = kTRUE) override;
   void              Abort(int code = 0) override;

   //---- Dynamic loading — static linking only ---------------------
   int               Load(const char *module, const char *entry = "",
                          Bool_t system = kFALSE) override;
   void              Unload(const char *module) override;
   Func_t            DynFindSymbol(const char *module,
                                   const char *entry) override;

   //---- TCP/UDP socket services — not available in browser WASM ---
   int               AnnounceTcpService(int port, Bool_t reuse, int backlog,
                                        int tcpwindowsize = -1,
                                        ESocketBindOption bindOpt =
                                          ESocketBindOption::kInaddrAny) override;
   int               AnnounceUdpService(int port, int backlog,
                                        ESocketBindOption bindOpt =
                                          ESocketBindOption::kInaddrAny) override;
   int               AnnounceUnixService(int port, int backlog) override;
   int               AnnounceUnixService(const char *sockpath,
                                         int backlog) override;

   //---- System info — not meaningful in WASM ----------------------
   int               GetSysInfo(SysInfo_t *info) const override;
   int               GetCpuInfo(CpuInfo_t *info,
                                Int_t sampleTime = 1000) const override;
   int               GetMemInfo(MemInfo_t *info) const override;
   int               GetProcInfo(ProcInfo_t *info) const override;

   // No ClassDefOverride: rootcling is not available in WASM cross-compilation,
   // so standard C++ RTTI (inherited from TUnixSystem) is used instead.
};

#endif // ROOT_TEmscriptenSystem
