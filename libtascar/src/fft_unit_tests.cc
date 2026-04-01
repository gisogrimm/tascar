/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "fft.h"
#include "tscconfig.h"

TEST(fft_t, fftifft)
{
  TASCAR::wave_t w(4);
  TASCAR::fft_t fft(4);
  w.d[0] = -2;
  w.d[1] = -1;
  w.d[2] = 0;
  w.d[3] = -2;
  // transform to spectrum:
  fft.execute(w);
  ASSERT_NEAR(fft.s.b[0].real(), -5.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[0].imag(), 0.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[1].real(), -2.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[1].imag(), -1.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[2].real(), 1.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[2].imag(), 0.0f, 1e-7f);
  fft.ifft();
  ASSERT_NEAR(fft.w.d[0], -2.0f, 1e-7f);
  ASSERT_NEAR(fft.w.d[1], -1.0f, 1e-7f);
  ASSERT_NEAR(fft.w.d[2], 0.0f, 1e-7f);
  ASSERT_NEAR(fft.w.d[3], -2.0f, 1e-7f);
}

TEST(fft_t, hilbert)
{
  TASCAR::wave_t w(4);
  TASCAR::fft_t fft(4);
  // example 1: 1,1,0,0
  w.d[0] = 1;
  w.d[1] = 1;
  fft.hilbert(w);
  EXPECT_EQ(fft.w.d[0], -0.5);
  EXPECT_EQ(fft.w.d[1], 0.5);
  EXPECT_EQ(fft.w.d[2], 0.5);
  EXPECT_EQ(fft.w.d[3], -0.5);
  // example 2: -2,-1,0,-2
  w.d[0] = -2;
  w.d[1] = -1;
  w.d[2] = 0;
  w.d[3] = -2;
  fft.hilbert(w);
  EXPECT_EQ(fft.w.d[0], -0.5);
  EXPECT_EQ(fft.w.d[1], -1.0);
  EXPECT_EQ(fft.w.d[2], 0.5);
  EXPECT_EQ(fft.w.d[3], 1.0);
  // example 3: 1,1,0,0,1,1,0,0
  TASCAR::wave_t w2(8);
  TASCAR::fft_t fft2(8);
  w2.d[0] = 1;
  w2.d[1] = 1;
  w2.d[4] = 1;
  w2.d[5] = 1;
  fft2.hilbert(w2);
  EXPECT_EQ(fft2.w.d[0], -0.5);
  EXPECT_EQ(fft2.w.d[1], 0.5);
  EXPECT_EQ(fft2.w.d[2], 0.5);
  EXPECT_EQ(fft2.w.d[3], -0.5);
  EXPECT_EQ(fft2.w.d[4], -0.5);
  EXPECT_EQ(fft2.w.d[5], 0.5);
  EXPECT_EQ(fft2.w.d[6], 0.5);
  EXPECT_EQ(fft2.w.d[7], -0.5);
}

TEST(minphase_t, minphase)
{
  TASCAR::wave_t w(6);
  TASCAR::fft_t fft(6);
  TASCAR::spec_t mpout(4);
  w.d[0] = 9;
  w.d[1] = 2;
  w.d[2] = 0;
  w.d[3] = -1;
  w.d[4] = 0;
  w.d[5] = 2;
  // transform to spectrum:
  fft.execute(w);
  ASSERT_NEAR(fft.s.b[0].real(), 12.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[1].real(), 12.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[2].real(), 6.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[3].real(), 6.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[0].imag(), 0.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[1].imag(), 0.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[2].imag(), 0.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[3].imag(), 0.0f, 1e-7f);
  ASSERT_NEAR(std::abs(fft.s.b[0]), 12.0f, 1e-7f);
  ASSERT_NEAR(std::abs(fft.s.b[1]), 12.0f, 1e-7f);
  ASSERT_NEAR(std::abs(fft.s.b[2]), 6.0f, 1e-7f);
  ASSERT_NEAR(std::abs(fft.s.b[3]), 6.0f, 1e-7f);
  TASCAR::minphase_t minphase(6);
  minphase(fft.s);
  // amplitude spectrum should be the same:
  ASSERT_NEAR(std::abs(fft.s.b[0]), 12.0f, 1e-7f);
  ASSERT_NEAR(std::abs(fft.s.b[1]), 12.0f, 1e-5f);
  ASSERT_NEAR(std::abs(fft.s.b[2]), 6.0f, 1e-5f);
  ASSERT_NEAR(std::abs(fft.s.b[3]), 6.0f, 1e-5f);
  // phase will differ:
  ASSERT_NEAR(fft.s.b[0].real(), 12.0f, 1e-7f);
  ASSERT_NEAR(fft.s.b[1].real(), 11.0518f, 1e-4f);
  ASSERT_NEAR(fft.s.b[2].real(), 5.5259f, 1e-4f);
  ASSERT_NEAR(fft.s.b[3].real(), 6.0f, 1e-4f);
  ASSERT_NEAR(fft.s.b[0].imag(), 0.0f, 1e-4f);
  ASSERT_NEAR(fft.s.b[1].imag(), -4.67511f, 1e-4f);
  ASSERT_NEAR(fft.s.b[2].imag(), -2.33755f, 1e-4f);
  ASSERT_NEAR(fft.s.b[3].imag(), 0.0f, 1e-4f);
  fft.ifft();
  ASSERT_NEAR(8.52592f, fft.w.d[0], 1e-5f);
  ASSERT_NEAR(3.94537f, fft.w.d[1], 1e-5f);
  ASSERT_NEAR(0.91183f, fft.w.d[2], 1e-5f);
  ASSERT_NEAR(-0.84197f, fft.w.d[3], 1e-5f);
  ASSERT_NEAR(-0.43776f, fft.w.d[4], 1e-5f);
  ASSERT_NEAR(-0.10339f, fft.w.d[5], 1e-5f);
}

TEST(get_bandlevels, bandspacing)
{
  TASCAR::wave_t w(2000);
  std::vector<float> vF;
  std::vector<float> vL;
  TASCAR::get_bandlevels(w, 100.0f, 400.0f, 2000.0f, 1.0f, 0.0f, vF, vL);
  ASSERT_EQ(vF.size(), 3u);
  if(vF.size() == 3) {
    ASSERT_NEAR(vF[0], 100.0f, 0.1f);
    ASSERT_NEAR(vF[1], 200.0f, 0.1f);
    ASSERT_NEAR(vF[2], 400.0f, 0.1f);
  }
  TASCAR::get_bandlevels(w, 100.0f, 400.0f, 2000.0f, 3.0f, 0.0f, vF, vL);
  ASSERT_EQ(vF.size(), 7u);
  if(vF.size() == 3) {
    ASSERT_NEAR(vF[0], 100.0f, 0.1f);
    ASSERT_NEAR(vF[1], 126.0f, 0.1f);
    ASSERT_NEAR(vF[2], 158.7f, 0.1f);
    ASSERT_NEAR(vF[3], 200.0f, 0.1f);
    ASSERT_NEAR(vF[4], 252.0f, 0.1f);
    ASSERT_NEAR(vF[5], 317.5f, 0.1f);
    ASSERT_NEAR(vF[6], 400.0f, 0.1f);
  }
}

TEST(get_bandlevels, bandlevels)
{
  float fs = 2000.0f;
  float f0 = 200.0f;
  TASCAR::wave_t w((uint32_t)fs);
  for(size_t k = 0; k < w.n; ++k)
    w.d[k] = sinf((float)k / fs * TASCAR_2PIf * f0);
  ASSERT_NEAR(w.spldb(), 91.0f, 0.1f);
  std::vector<float> vF;
  std::vector<float> vL;
  TASCAR::get_bandlevels(w, 100.0f, 400.0f, fs, 1.0f, 0.0f, vF, vL);
  ASSERT_EQ(vL.size(), 3u);
  if(vL.size() == 3) {
    ASSERT_NEAR(vL[0], -21.0f, 12.0f);
    ASSERT_NEAR(vL[1], 91.0f, 0.1f);
    ASSERT_NEAR(vL[2], -9.0f, 12.0f);
  }
}

TEST(get_bandlevels, bandlevels2)
{
  float fs = 8000.0f;
  float f0 = 200.0f;
  TASCAR::wave_t w((uint32_t)fs);
  for(size_t k = 0; k < w.n; ++k)
    w.d[k] = sinf((float)k / fs * TASCAR_2PIf * f0);
  ASSERT_NEAR(w.spldb(), 91.0f, 0.1f);
  std::vector<float> vF;
  std::vector<float> vL;
  TASCAR::get_bandlevels(w, 100.0f, 400.0f, fs, 1.0f, 0.0f, vF, vL);
  ASSERT_EQ(vL.size(), 3u);
  if(vL.size() == 3u) {
    ASSERT_NEAR(vL[0], -25.0f, 12.0f);
    ASSERT_NEAR(vL[1], 91.0f, 0.1f);
    ASSERT_NEAR(vL[2], -20.0f, 12.0f);
  }
}

TEST(get_bandlevels, bandlevelsoverlap)
{
  float fs = 8000.0f;
  float f0 = 200.0f;
  TASCAR::wave_t w((uint32_t)fs);
  for(size_t k = 0; k < w.n; ++k)
    w.d[k] = sinf((float)k / fs * TASCAR_2PIf * f0);
  ASSERT_NEAR(w.spldb(), 91.0f, 0.1f);
  std::vector<float> vF;
  std::vector<float> vL;
  // calculate band levels with 6 bands per octave and 4 bands overlap:
  TASCAR::get_bandlevels(w, 100.0f, 400.0f, fs, 6.0f, 4.0f, vF, vL);
  ASSERT_EQ(vL.size(), 13u);
  if(vL.size() == 13u) {
    ASSERT_NEAR(vL[0], -20.0f, 12.0f);
    ASSERT_NEAR(vL[1], -20.0f, 12.0f);
    ASSERT_NEAR(vL[2], 64.6f, 2.0f);
    ASSERT_NEAR(vL[3], 82.5f, 1.0f);
    ASSERT_NEAR(vL[4], 88.6f, 0.5f);
    ASSERT_NEAR(vL[5], 90.7f, 0.2f);
    ASSERT_NEAR(vL[6], 91.0f, 0.2f);
    ASSERT_NEAR(vL[7], 90.5f, 0.2f);
    ASSERT_NEAR(vL[8], 86.9f, 0.5f);
    ASSERT_NEAR(vL[9], 78.6f, 1.0f);
    ASSERT_NEAR(vL[10], 60.1f, 2.0f);
    ASSERT_NEAR(vL[11], -20.0f, 12.0f);
    ASSERT_NEAR(vL[12], -20.0f, 12.0f);
  }
}

TEST(get_bandlevels, bandlevels3)
{
  float fs = 8000.0f;
  TASCAR::wave_t w((uint32_t)fs);
  for(size_t k = 0; k < w.n; ++k)
    w.d[k] = (float)TASCAR::drand() - 0.5f;
  ASSERT_NEAR(w.spldb(), 83.2f, 0.5f);
  std::vector<float> vF;
  std::vector<float> vL;
  TASCAR::get_bandlevels(w, 50.0f, 3200.0f, fs, 1.0f, 0.0f, vF, vL);
  ASSERT_EQ(vL.size(), 7u);
  if(vL.size() == 7u) {
    ASSERT_NEAR(vL[0], 61.4f, 2.0f);
    ASSERT_NEAR(vL[1], 65.584f, 2.0f);
    ASSERT_NEAR(vL[2], 69.162f, 2.0f);
    ASSERT_NEAR(vL[3], 71.592f, 2.0f);
    ASSERT_NEAR(vL[4], 74.632f, 2.0f);
    ASSERT_NEAR(vL[5], 77.832f, 2.0f);
    ASSERT_NEAR(vL[6], 79.523f, 2.0f);
  }
}

// Local Variables:
// compile-command: "make -C ../.. unit-tests"
// coding: utf-8-unix
// c-basic-offset: 2
// indent-tabs-mode: nil
// End:
