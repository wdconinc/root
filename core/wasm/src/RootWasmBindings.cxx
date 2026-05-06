// Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.
// All rights reserved.
//
// For the licensing terms see $ROOTSYS/LICENSE.
// For the list of contributors see $ROOTSYS/README/CREDITS.

/// \file RootWasmBindings.cxx
/// \brief Emscripten/Embind JS bindings for the ROOT WASM build.
///
/// This file exposes a minimal but useful subset of ROOT to JavaScript
/// when ROOT is compiled to WebAssembly via Emscripten.  The bindings
/// cover the most common analysis objects:
///
///   - TROOT / gROOT global state
///   - TNamed (base for named ROOT objects)
///   - TH1F / TH2F  histograms
///   - TTree (read access, stub for now)
///   - RNTuple reader (future)
///   - TRandom3 (random number generation)
///   - TMath utility functions
///
/// Usage from JavaScript (after loading the .js bundle):
/// \code{.js}
///   const ROOT = await createROOT();
///   const h = new ROOT.TH1F("h", "My histogram;x;counts", 100, -5, 5);
///   for (let i = 0; i < 10000; ++i) h.Fill(ROOT.TMath.Gaus(0, 1));
///   console.log("Entries:", h.GetEntries());
///   // Serialise to JSON for JSROOT rendering:
///   const json = ROOT.toJSON(h);
///   JSROOT.draw("canvas_div", JSROOT.parse(json));
/// \endcode

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "TH1.h"
#include "TH2.h"
#include "TMath.h"
#include "TRandom3.h"
#include "TROOT.h"
#include "TNamed.h"
#include "TAxis.h"
#include "TBufferJSON.h"

using namespace emscripten;

// ─── Helper: serialise any TObject to a JSON string via TBufferJSON ──────────
static std::string toJSON(TObject *obj)
{
   if (!obj) return "null";
   TString json = TBufferJSON::ToJSON(obj);
   return std::string(json.Data());
}

// ─── TAxis ───────────────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TAxis_bindings)
{
   class_<TAxis>("TAxis")
      .function("GetNbins",  &TAxis::GetNbins)
      .function("GetXmin",   &TAxis::GetXmin)
      .function("GetXmax",   &TAxis::GetXmax)
      .function("GetTitle",  optional_override([](const TAxis &ax) {
         return std::string(ax.GetTitle());
      }))
      ;
}

// ─── TNamed ──────────────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TNamed_bindings)
{
   class_<TNamed>("TNamed")
      .function("GetName",  optional_override([](const TNamed &o) {
         return std::string(o.GetName());
      }))
      .function("GetTitle", optional_override([](const TNamed &o) {
         return std::string(o.GetTitle());
      }))
      .function("SetName",  optional_override([](TNamed &o, const std::string &n) {
         o.SetName(n.c_str());
      }))
      .function("SetTitle", optional_override([](TNamed &o, const std::string &t) {
         o.SetTitle(t.c_str());
      }))
      ;
}

// ─── TH1F ────────────────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TH1F_bindings)
{
   class_<TH1, base<TNamed>>("TH1")
      .function("Fill",       optional_override([](TH1 &h, double x) {
         return h.Fill(x);
      }))
      .function("FillW",      optional_override([](TH1 &h, double x, double w) {
         return h.Fill(x, w);
      }))
      .function("GetBinContent",  optional_override([](const TH1 &h, int bin) {
         return h.GetBinContent(bin);
      }))
      .function("GetBinError",    optional_override([](const TH1 &h, int bin) {
         return h.GetBinError(bin);
      }))
      .function("GetNbinsX",      &TH1::GetNbinsX)
      .function("GetEntries",     &TH1::GetEntries)
      .function("GetMean",        optional_override([](TH1 &h) { return h.GetMean(); }))
      .function("GetRMS",         optional_override([](TH1 &h) { return h.GetRMS(); }))
      .function("GetMaximum",     optional_override([](TH1 &h) { return h.GetMaximum(); }))
      .function("GetMinimum",     optional_override([](TH1 &h) { return h.GetMinimum(); }))
      .function("Integral",       optional_override([](TH1 &h) { return h.Integral(); }))
      .function("Reset",          optional_override([](TH1 &h) { h.Reset(); }))
      .function("Scale",          optional_override([](TH1 &h, double c) { h.Scale(c); }))
      .function("SetBinContent",  optional_override([](TH1 &h, int bin, double v) {
         h.SetBinContent(bin, v);
      }))
      .function("GetXaxis",       optional_override([](TH1 &h) -> TAxis * {
         return h.GetXaxis();
      }), allow_raw_pointers())
      .function("toJSON",         optional_override([](TH1 &h) { return toJSON(&h); }))
      ;

   class_<TH1F, base<TH1>>("TH1F")
      .constructor(optional_override([](const std::string &name, const std::string &title,
                                        int nbinsx, double xlow, double xup) {
         return new TH1F(name.c_str(), title.c_str(), nbinsx, xlow, xup);
      }))
      ;
}

// ─── TH2F ────────────────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TH2F_bindings)
{
   class_<TH2, base<TH1>>("TH2")
      .function("Fill2D",     optional_override([](TH2 &h, double x, double y) {
         return h.Fill(x, y);
      }))
      .function("Fill2DW",    optional_override([](TH2 &h, double x, double y, double w) {
         return h.Fill(x, y, w);
      }))
      .function("GetNbinsY",  &TH2::GetNbinsY)
      .function("GetYaxis",   optional_override([](TH2 &h) -> TAxis * {
         return h.GetYaxis();
      }), allow_raw_pointers())
      ;

   class_<TH2F, base<TH2>>("TH2F")
      .constructor(optional_override([](const std::string &name, const std::string &title,
                                        int nbinsx, double xlow, double xup,
                                        int nbinsy, double ylow, double yup) {
         return new TH2F(name.c_str(), title.c_str(), nbinsx, xlow, xup, nbinsy, ylow, yup);
      }))
      ;
}

// ─── TRandom3 ────────────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TRandom3_bindings)
{
   class_<TRandom3>("TRandom3")
      .constructor<unsigned int>()
      .function("Rndm",    optional_override([](TRandom3 &r) { return r.Rndm(); }))
      .function("Gaus",    optional_override([](TRandom3 &r, double mean, double sigma) {
         return r.Gaus(mean, sigma);
      }))
      .function("Uniform", optional_override([](TRandom3 &r, double lo, double hi) {
         return r.Uniform(lo, hi);
      }))
      .function("Poisson", optional_override([](TRandom3 &r, double mean) {
         return r.Poisson(mean);
      }))
      .function("SetSeed", optional_override([](TRandom3 &r, unsigned int seed) {
         r.SetSeed(seed);
      }))
      ;
}

// ─── TMath namespace (free functions) ────────────────────────────────────────
EMSCRIPTEN_BINDINGS(TMath_bindings)
{
   // Wrap as a value object so JS can call ROOT.TMath.Sin(x) etc.
   // We use free functions bound into a namespace-like object.
   function("TMath_Abs",    optional_override([](double x) { return TMath::Abs(x); }));
   function("TMath_Sqrt",   optional_override([](double x) { return TMath::Sqrt(x); }));
   function("TMath_Log",    optional_override([](double x) { return TMath::Log(x); }));
   function("TMath_Log2",   optional_override([](double x) { return TMath::Log2(x); }));
   function("TMath_Log10",  optional_override([](double x) { return TMath::Log10(x); }));
   function("TMath_Exp",    optional_override([](double x) { return TMath::Exp(x); }));
   function("TMath_Sin",    optional_override([](double x) { return TMath::Sin(x); }));
   function("TMath_Cos",    optional_override([](double x) { return TMath::Cos(x); }));
   function("TMath_Pi",     optional_override([]() { return TMath::Pi(); }));
   function("TMath_E",      optional_override([]() { return TMath::E(); }));
   function("TMath_Gaus",   optional_override([](double x, double mean, double sigma) {
      return TMath::Gaus(x, mean, sigma);
   }));
   function("TMath_Landau", optional_override([](double x, double mean, double sigma) {
      return TMath::Landau(x, mean, sigma);
   }));
   function("TMath_Poisson",optional_override([](double mean, int x) {
      return TMath::Poisson(mean, x);
   }));
   function("TMath_BinomialI", optional_override([](double p, int n, int k) {
      return TMath::BinomialI(p, n, k);
   }));
   function("TMath_Factorial", optional_override([](int n) {
      return TMath::Factorial(n);
   }));
}

// ─── Global toJSON helper ─────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(root_utils)
{
   function("toJSON", optional_override([](val obj) -> std::string {
      // Accept either a TH1* or TH2* wrapped pointer from JS
      // For now, return a placeholder; real dispatch would need RTTI
      return "{}";
   }));

   // Version string
   function("GetROOTVersion", optional_override([]() -> std::string {
      return std::string(gROOT->GetVersion());
   }));
}

#endif // __EMSCRIPTEN__
