// Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.
// All rights reserved.
//
// For the licensing terms see $ROOTSYS/LICENSE.
// For the list of contributors see $ROOTSYS/README/CREDITS.

/// \file test_wasm.js
/// \brief Node.js unit tests for the ROOT WebAssembly (Emscripten) build.
///
/// Run after building root.js / root.wasm:
///   node core/wasm/test/test_wasm.js <path-to-root.js>
///
/// Exit code 0 = all tests passed, 1 = at least one failure.

'use strict';

const assert = require('assert');
const path   = require('path');

const rootJsPath = process.argv[2] || path.join(__dirname, '../../../build-wasm/js/root.js');
const createROOT = require(path.resolve(rootJsPath));

// ─── Minimal test harness ─────────────────────────────────────────────────────
let passed = 0;
let failed = 0;

function test(name, fn) {
   try {
      fn();
      console.log('  \u2713 ' + name);
      ++passed;
   } catch (e) {
      console.error('  \u2717 ' + name);
      console.error('    ' + e.message);
      ++failed;
   }
}

function approxEqual(a, b, tol) {
   tol = (tol === undefined) ? 1e-9 : tol;
   assert(Math.abs(a - b) <= tol,
      `Expected ${a} \u2248 ${b} (tolerance ${tol})`);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
async function main() {
   // createROOT() accepts an optional locateFile callback so the .wasm can be
   // found even when the working directory differs from the .js directory.
   const jsDir = path.dirname(path.resolve(rootJsPath));
   let ROOT;
   try {
      ROOT = await createROOT({
         locateFile: (f) => path.join(jsDir, f),
         // Provide a minimal environment: Emscripten MODULARIZE mode does NOT
         // automatically propagate Node.js process.env to the WASM module.
         ENV: { HOME: process.env.HOME || '/tmp' },
         // Suppress ROOT's stderr chatter (gitinfo warnings etc.) in tests.
         printErr: (msg) => {
            // Only print unexpected Fatal/Error messages; filter known warnings.
            if (/Fatal|Segmentation|Abort/.test(msg)) process.stderr.write(msg + '\n');
         },
      });
   } catch (e) {
      console.error('FATAL: createROOT() rejected:', e);
      process.exit(1);
   }

   // ── 1. Module loading ────────────────────────────────────────────────────
   console.log('\n[1] Module loading');
   test('createROOT() resolves to a truthy object', () => {
      assert(ROOT, 'ROOT module is falsy');
   });

   // ── 2. ROOT version ──────────────────────────────────────────────────────
   console.log('\n[2] ROOT version');
   test('GetROOTVersion() returns a non-empty string', () => {
      const v = ROOT.GetROOTVersion();
      assert(typeof v === 'string' && v.length > 0, `Unexpected version: ${v}`);
   });
   test('GetROOTVersion() contains digits', () => {
      assert(/\d/.test(ROOT.GetROOTVersion()),
         `No digits in version: ${ROOT.GetROOTVersion()}`);
   });

   // ── 3. TH1F ─────────────────────────────────────────────────────────────
   console.log('\n[3] TH1F');
   const h1 = new ROOT.TH1F('h1', 'Test histogram;x;counts', 100, -5, 5);

   test('GetNbinsX() == 100', () => assert.strictEqual(h1.GetNbinsX(), 100));
   test('GetEntries() == 0 before filling', () => assert.strictEqual(h1.GetEntries(), 0));

   // Fill with 5 entries at known positions
   h1.Fill(0);
   h1.Fill(1);
   h1.Fill(-1);
   h1.Fill(2);
   h1.Fill(-2);

   test('GetEntries() == 5 after filling', () => assert.strictEqual(h1.GetEntries(), 5));
   test('GetMean() close to 0 for symmetric fill', () => approxEqual(h1.GetMean(), 0, 1e-9));
   test('Integral() == 5', () => approxEqual(h1.Integral(), 5));

   const binAtZero = h1.GetBinContent(51); // bin 51 is the bin containing 0 for [-5,5]/100
   test('GetBinContent(51) == 1 (single fill at 0)', () => approxEqual(binAtZero, 1));

   // Weighted fill
   h1.FillW(3, 2.5); // fill x=3 with weight 2.5
   test('GetEntries() == 6 after weighted fill', () => approxEqual(h1.GetEntries(), 6));

   // Scale
   h1.Scale(2);
   test('GetEntries() doubles after Scale(2)', () => approxEqual(h1.GetEntries(), 12));

   // Reset
   h1.Reset();
   test('GetEntries() == 0 after Reset()', () => approxEqual(h1.GetEntries(), 0));
   test('Integral() == 0 after Reset()', () => approxEqual(h1.Integral(), 0));

   // X-axis
   const xax = h1.GetXaxis();
   test('GetXaxis() is non-null', () => assert(xax));
   test('GetXaxis().GetNbins() == 100', () => assert.strictEqual(xax.GetNbins(), 100));
   test('GetXaxis().GetXmin() == -5', () => approxEqual(xax.GetXmin(), -5));
   test('GetXaxis().GetXmax() ==  5', () => approxEqual(xax.GetXmax(),  5));

   // ── 4. TH2F ─────────────────────────────────────────────────────────────
   console.log('\n[4] TH2F');
   const h2 = new ROOT.TH2F('h2', 'Test 2D;x;y', 50, -5, 5, 40, -4, 4);

   test('GetNbinsX() == 50', () => assert.strictEqual(h2.GetNbinsX(), 50));
   test('GetNbinsY() == 40', () => assert.strictEqual(h2.GetNbinsY(), 40));
   test('GetEntries() == 0 before filling', () => approxEqual(h2.GetEntries(), 0));

   h2.Fill2D(1, 2);
   h2.Fill2D(-1, -2);
   h2.Fill2DW(0, 0, 3);

   test('GetEntries() == 3 after filling (weighted counts as 1)', () => approxEqual(h2.GetEntries(), 3));
   test('Integral() == 5 (sum of weights)', () => approxEqual(h2.Integral(), 5));

   // ── 5. TRandom3 ─────────────────────────────────────────────────────────
   console.log('\n[5] TRandom3');
   const r1 = new ROOT.TRandom3(42);
   const r2 = new ROOT.TRandom3(42);

   const v1 = r1.Rndm();
   const v2 = r2.Rndm();
   test('Rndm() is in [0,1)', () => assert(v1 >= 0 && v1 < 1, `Got ${v1}`));
   test('Same seed → same first value', () => assert.strictEqual(v1, v2));
   test('Different seed gives different value', () => {
      const r3 = new ROOT.TRandom3(99);
      assert.notStrictEqual(r3.Rndm(), v1);
   });

   const gv = r1.Gaus(0, 1);
   test('Gaus(0,1) returns a finite number', () => assert(isFinite(gv)));

   const uv = r1.Uniform(-1, 1);
   test('Uniform(-1,1) is in [-1,1)', () => assert(uv >= -1 && uv < 1, `Got ${uv}`));

   const pv = r1.Poisson(5);
   test('Poisson(5) is a non-negative integer', () => {
      assert(Number.isInteger(pv) && pv >= 0, `Got ${pv}`);
   });

   // SetSeed reproducibility
   r1.SetSeed(42);
   test('After SetSeed(42) first Rndm() matches original', () => {
      assert.strictEqual(r1.Rndm(), v1);
   });

   // ── 6. TMath ────────────────────────────────────────────────────────────
   console.log('\n[6] TMath');
   test('TMath_Pi() ≈ Math.PI',    () => approxEqual(ROOT.TMath_Pi(), Math.PI, 1e-12));
   test('TMath_E()  ≈ Math.E',     () => approxEqual(ROOT.TMath_E(),  Math.E,  1e-12));
   test('TMath_Sqrt(4) == 2',      () => approxEqual(ROOT.TMath_Sqrt(4), 2));
   test('TMath_Abs(-3) == 3',      () => approxEqual(ROOT.TMath_Abs(-3), 3));
   test('TMath_Sin(0) == 0',       () => approxEqual(ROOT.TMath_Sin(0), 0));
   test('TMath_Cos(0) == 1',       () => approxEqual(ROOT.TMath_Cos(0), 1));
   test('TMath_Exp(0) == 1',       () => approxEqual(ROOT.TMath_Exp(0), 1));
   test('TMath_Log(1) == 0',       () => approxEqual(ROOT.TMath_Log(1), 0));
   test('TMath_Log10(100) == 2',   () => approxEqual(ROOT.TMath_Log10(100), 2));
   test('TMath_Log2(8) == 3',      () => approxEqual(ROOT.TMath_Log2(8), 3));
   test('TMath_Factorial(5) == 120', () => approxEqual(ROOT.TMath_Factorial(5), 120));

   // Gaus: peak at mean
   const gausAtMean = ROOT.TMath_Gaus(0, 0, 1);
   const gausExpected = 1 / Math.sqrt(2 * Math.PI);
   test('TMath_Gaus(0,0,1) ≈ 1/sqrt(2π)', () => approxEqual(gausAtMean, gausExpected, 1e-10));

   // ── 7. TNamed via TH1F ──────────────────────────────────────────────────
   console.log('\n[7] TNamed (via TH1F)');
   const hn = new ROOT.TH1F('myname', 'mytitle', 10, 0, 10);
   test('GetName()  == "myname"',  () => assert.strictEqual(hn.GetName(),  'myname'));
   test('GetTitle() == "mytitle"', () => assert.strictEqual(hn.GetTitle(), 'mytitle'));

   hn.SetName('newname');
   test('SetName() changes GetName()', () => assert.strictEqual(hn.GetName(), 'newname'));

   hn.SetTitle('newtitle');
   test('SetTitle() changes GetTitle()', () => assert.strictEqual(hn.GetTitle(), 'newtitle'));

   // ── 8. TBufferJSON via TH1.toJSON() ─────────────────────────────────────
   console.log('\n[8] TBufferJSON');
   const hj = new ROOT.TH1F('hjson', 'JSON test', 10, 0, 10);
   hj.Fill(3);
   hj.Fill(7);

   const json = hj.toJSON();
   test('toJSON() returns a non-empty string', () => {
      assert(typeof json === 'string' && json.length > 0, `Got: ${json}`);
   });
   test('toJSON() parses as valid JSON', () => {
      const obj = JSON.parse(json);
      assert(obj !== null && typeof obj === 'object');
   });
   test('toJSON() output contains _typename', () => {
      const obj = JSON.parse(json);
      assert('_typename' in obj, `No _typename in: ${JSON.stringify(obj).slice(0,200)}`);
   });
   test('toJSON() _typename is TH1F', () => {
      const obj = JSON.parse(json);
      assert.strictEqual(obj._typename, 'TH1F');
   });

   // ── Summary ──────────────────────────────────────────────────────────────
   console.log(`\n${'─'.repeat(50)}`);
   console.log(`Results: ${passed} passed, ${failed} failed`);

   process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => {
   console.error('Unhandled error in test runner:', e);
   process.exit(1);
});
