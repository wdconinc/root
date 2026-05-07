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

#include "TH1.h"
#include "TH2.h"
#include "TMath.h"
#include "TRandom3.h"
#include "TROOT.h"
#include "TNamed.h"
#include "TAxis.h"

#include <cstdio>
#include <string>

using namespace emscripten;

// ─── Helper: JSON-escape a C string ─────────────────────────────────────────
static std::string jsonEscapeStr(const char *s)
{
   if (!s) return "";
   std::string out;
   for (; *s; ++s) {
      unsigned char c = static_cast<unsigned char>(*s);
      if      (c == '"')  out += "\\\"";
      else if (c == '\\') out += "\\\\";
      else if (c == '\n') out += "\\n";
      else if (c == '\r') out += "\\r";
      else if (c == '\t') out += "\\t";
      else                out += static_cast<char>(c);
   }
   return out;
}

static std::string dbl(double v)
{
   // Use %.15g so statistics round-trip faithfully.
   if (v != v)          return "null";   // NaN
   if (v == 1.0 / 0.0) return "1e308";  // +Inf → large finite
   if (v == -1.0 / 0.0)return "-1e308"; // -Inf → large negative finite
   char buf[64];
   std::snprintf(buf, sizeof(buf), "%.15g", v);
   return buf;
}

static std::string flt(float v)
{
   if (v != v)          return "null";
   if (v == 1.0f / 0.0f) return "3.4e38";
   if (v == -1.0f / 0.0f)return "-3.4e38";
   char buf[32];
   std::snprintf(buf, sizeof(buf), "%.7g", v);
   return buf;
}

// ─── Serialise a TAxis to JSROOT-compatible JSON ─────────────────────────────
static std::string axisToJSON(const TAxis *ax, const char *name)
{
   if (!ax) return "null";
   std::string j;
   j.reserve(512);
   j += "{\"_typename\":\"TAxis\"";
   j += ",\"fUniqueID\":0,\"fBits\":50331648";
   j += ",\"fName\":\""; j += jsonEscapeStr(name); j += "\"";
   j += ",\"fTitle\":\""; j += jsonEscapeStr(ax->GetTitle()); j += "\"";
   j += ",\"fNdivisions\":510,\"fAxisColor\":1";
   j += ",\"fLabelColor\":1,\"fLabelFont\":42";
   j += ",\"fLabelOffset\":0.005,\"fLabelSize\":0.035";
   j += ",\"fTickLength\":0.03,\"fTitleOffset\":1,\"fTitleSize\":0.035";
   j += ",\"fTitleColor\":1,\"fTitleFont\":42";
   j += ",\"fNbins\":";  j += std::to_string(ax->GetNbins());
   j += ",\"fXmin\":";   j += dbl(ax->GetXmin());
   j += ",\"fXmax\":";   j += dbl(ax->GetXmax());
   j += ",\"fXbins\":{\"_typename\":\"TArrayD\",\"fN\":0,\"fArray\":[]}";
   j += ",\"fFirst\":0,\"fLast\":0,\"fBits2\":0";
   j += ",\"fTimeDisplay\":false,\"fTimeFormat\":\"\"";
   j += ",\"fLabels\":null,\"fModLabs\":null}";
   return j;
}

// ─── Serialise a TH1 to JSROOT-compatible JSON ───────────────────────────────
// This bypasses TBufferJSON / TStreamerInfo entirely, which aren't available
// in WASM builds that don't load PCM files via Cling.
static std::string th1ToJSROOTJSON(const TH1 &h, const char *typeName)
{
   int ncells = h.GetNbinsX() + 2;  // underflow + bins + overflow

   // fArray (TArrayF bin contents)
   std::string arr;
   arr.reserve(ncells * 8);
   arr = "[";
   for (int i = 0; i < ncells; ++i) {
      if (i) arr += ",";
      arr += flt(static_cast<float>(h.GetBinContent(i)));
   }
   arr += "]";

   // fSumw2 (TArrayD — populated only when Sumw2() was called)
   int nsumw2 = h.GetSumw2N();
   std::string sw2;
   sw2.reserve(nsumw2 > 0 ? nsumw2 * 12 : 2);
   sw2 = "[";
   for (int i = 0; i < nsumw2; ++i) {
      if (i) sw2 += ",";
      double e = h.GetBinError(i);
      sw2 += dbl(e * e);
   }
   sw2 += "]";

   Double_t stats[10] = {};
   const_cast<TH1 &>(h).GetStats(stats);

   std::string j;
   j.reserve(2048);
   j += "{\"_typename\":\"";    j += typeName;         j += "\"";
   j += ",\"fUniqueID\":0,\"fBits\":50331648";
   j += ",\"fName\":\"";        j += jsonEscapeStr(h.GetName());  j += "\"";
   j += ",\"fTitle\":\"";       j += jsonEscapeStr(h.GetTitle()); j += "\"";
   j += ",\"fLineColor\":602,\"fLineStyle\":1,\"fLineWidth\":1";
   j += ",\"fFillColor\":0,\"fFillStyle\":1001";
   j += ",\"fMarkerColor\":1,\"fMarkerStyle\":1,\"fMarkerSize\":1";
   j += ",\"fNcells\":";        j += std::to_string(ncells);
   j += ",\"fXaxis\":";         j += axisToJSON(h.GetXaxis(), "xaxis");
   j += ",\"fYaxis\":";         j += axisToJSON(h.GetYaxis(), "yaxis");
   j += ",\"fZaxis\":";         j += axisToJSON(h.GetZaxis(), "zaxis");
   j += ",\"fBarOffset\":0,\"fBarWidth\":1000";
   j += ",\"fEntries\":";       j += dbl(h.GetEntries());
   j += ",\"fTsumw\":";         j += dbl(stats[0]);
   j += ",\"fTsumw2\":";        j += dbl(stats[1]);
   j += ",\"fTsumwx\":";        j += dbl(stats[2]);
   j += ",\"fTsumwx2\":";       j += dbl(stats[3]);
   j += ",\"fMaximum\":-1111,\"fMinimum\":-1111,\"fNormFactor\":0";
   j += ",\"fContour\":{\"_typename\":\"TArrayD\",\"fN\":0,\"fArray\":[]}";
   j += ",\"fSumw2\":{\"_typename\":\"TArrayD\",\"fN\":";
   j += std::to_string(nsumw2); j += ",\"fArray\":"; j += sw2; j += "}";
   j += ",\"fOption\":\"\"";
   j += ",\"fFunctions\":{\"_typename\":\"TList\",\"name\":\".\",\"arr\":[],\"opt\":[]}";
   j += ",\"fBufferSize\":0,\"fBuffer\":[],\"fBinStatErrOpt\":0,\"fStatOverflows\":2";
   j += ",\"fArray\":{\"_typename\":\"TArrayF\",\"fN\":";
   j += std::to_string(ncells); j += ",\"fArray\":"; j += arr; j += "}";
   j += "}";
   return j;
}

// ─── Serialise a TH2 to JSROOT-compatible JSON ───────────────────────────────
static std::string th2ToJSROOTJSON(const TH2 &h)
{
   int nx = h.GetNbinsX();
   int ny = h.GetNbinsY();
   int ncells = (nx + 2) * (ny + 2);

   std::string arr;
   arr.reserve(ncells * 8);
   arr = "[";
   for (int i = 0; i < ncells; ++i) {
      if (i) arr += ",";
      arr += flt(static_cast<float>(h.GetBinContent(i)));
   }
   arr += "]";

   int nsumw2 = h.GetSumw2N();
   std::string sw2;
   sw2.reserve(nsumw2 > 0 ? nsumw2 * 12 : 2);
   sw2 = "[";
   for (int i = 0; i < nsumw2; ++i) {
      if (i) sw2 += ",";
      double e = h.GetBinError(i);
      sw2 += dbl(e * e);
   }
   sw2 += "]";

   Double_t stats[10] = {};
   const_cast<TH2 &>(h).GetStats(stats);

   std::string j;
   j.reserve(3072);
   j += "{\"_typename\":\"TH2F\"";
   j += ",\"fUniqueID\":0,\"fBits\":50331648";
   j += ",\"fName\":\"";  j += jsonEscapeStr(h.GetName());  j += "\"";
   j += ",\"fTitle\":\""; j += jsonEscapeStr(h.GetTitle()); j += "\"";
   j += ",\"fLineColor\":602,\"fLineStyle\":1,\"fLineWidth\":1";
   j += ",\"fFillColor\":0,\"fFillStyle\":1001";
   j += ",\"fMarkerColor\":1,\"fMarkerStyle\":1,\"fMarkerSize\":1";
   j += ",\"fNcells\":";  j += std::to_string(ncells);
   j += ",\"fXaxis\":";   j += axisToJSON(h.GetXaxis(), "xaxis");
   j += ",\"fYaxis\":";   j += axisToJSON(h.GetYaxis(), "yaxis");
   j += ",\"fZaxis\":";   j += axisToJSON(h.GetZaxis(), "zaxis");
   j += ",\"fBarOffset\":0,\"fBarWidth\":1000";
   j += ",\"fEntries\":"; j += dbl(h.GetEntries());
   j += ",\"fTsumw\":";   j += dbl(stats[0]);
   j += ",\"fTsumw2\":";  j += dbl(stats[1]);
   j += ",\"fTsumwx\":";  j += dbl(stats[2]);
   j += ",\"fTsumwx2\":"; j += dbl(stats[3]);
   j += ",\"fTsumwy\":";  j += dbl(stats[4]);
   j += ",\"fTsumwy2\":"; j += dbl(stats[5]);
   j += ",\"fTsumwxy\":"; j += dbl(stats[6]);
   j += ",\"fMaximum\":-1111,\"fMinimum\":-1111,\"fNormFactor\":0";
   j += ",\"fContour\":{\"_typename\":\"TArrayD\",\"fN\":0,\"fArray\":[]}";
   j += ",\"fSumw2\":{\"_typename\":\"TArrayD\",\"fN\":";
   j += std::to_string(nsumw2); j += ",\"fArray\":"; j += sw2; j += "}";
   j += ",\"fOption\":\"\"";
   j += ",\"fFunctions\":{\"_typename\":\"TList\",\"name\":\".\",\"arr\":[],\"opt\":[]}";
   j += ",\"fBufferSize\":0,\"fBuffer\":[],\"fBinStatErrOpt\":0,\"fStatOverflows\":2";
   j += ",\"fScalefactor\":1";
   j += ",\"fArray\":{\"_typename\":\"TArrayF\",\"fN\":";
   j += std::to_string(ncells); j += ",\"fArray\":"; j += arr; j += "}";
   j += "}";
   return j;
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
      .function("toJSON",         optional_override([](TH1 &h) {
         return th1ToJSROOTJSON(h, h.IsA()->GetName());
      }))
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
      .function("toJSON",     optional_override([](TH2 &h) {
         return th2ToJSROOTJSON(h);
      }))
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
         return static_cast<int>(r.Poisson(mean));  // ULong64_t → int for JS
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

// ─── Global utilities ─────────────────────────────────────────────────────────
EMSCRIPTEN_BINDINGS(root_utils)
{
   function("GetROOTVersion", optional_override([]() -> std::string {
      return std::string(gROOT->GetVersion());
   }));
}

#endif // __EMSCRIPTEN__
