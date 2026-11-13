// SPDX-License-Identifier: AGPL-3.0-or-later
// The builder's transcendental functions (src/dsp.ts): the browsers' Math.sin,
// Math.pow, Math.log2... are not guaranteed to agree to the last bit between
// JavaScript engines, the C library inside the WebAssembly module is the same
// code everywhere (Node, Chrome, Edge, Firefox).
#include <math.h>

double dsp_sin(double x) { return sin(x); }
double dsp_cos(double x) { return cos(x); }
double dsp_pow(double x, double y) { return pow(x, y); }
double dsp_log2(double x) { return log2(x); }
double dsp_log10(double x) { return log10(x); }
double dsp_exp(double x) { return exp(x); }
