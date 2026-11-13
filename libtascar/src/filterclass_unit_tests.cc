/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 * Copyright (c) 2022 Giso Grimm
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

#include "filterclass.h"
#include "tscconfig.h"
#include <complex>

TEST(filter_t, constructor)
{
  std::vector<double> A(1, 1);
  std::vector<double> B(1, 1);
  TASCAR::filter_t filter(A, B);
  EXPECT_EQ(1u, filter.get_len_A());
  EXPECT_EQ(1u, filter.get_len_B());
  EXPECT_EQ(1.0, filter.A[0]);
  EXPECT_EQ(1.0, filter.B[0]);
}

TEST(filter_t, copyconstructor)
{
  std::vector<double> A(1, 1);
  std::vector<double> B(1, 1);
  TASCAR::filter_t filter(A, B);
  EXPECT_EQ(1u, filter.get_len_A());
  EXPECT_EQ(1u, filter.get_len_B());
  EXPECT_EQ(1.0, filter.A[0]);
  EXPECT_EQ(1.0, filter.B[0]);
  TASCAR::filter_t filter2(filter);
  EXPECT_EQ(1u, filter2.get_len_A());
  EXPECT_EQ(1u, filter2.get_len_B());
  EXPECT_EQ(1.0, filter2.A[0]);
  EXPECT_EQ(1.0, filter2.B[0]);
}

TEST(filter_t, filter)
{
  std::vector<double> A(1, 1);
  std::vector<double> B(1, 1);
  TASCAR::filter_t filter(A, B);
  TASCAR::wave_t delta(1000);
  TASCAR::wave_t res(1000);
  delta[0] = 1;
  filter.filter(&res, &delta);
  EXPECT_EQ(1.0f, delta[0]);
  EXPECT_EQ(0.0f, delta[1]);
  EXPECT_EQ(1.0f, res[0]);
  EXPECT_EQ(0.0f, res[1]);
  B.push_back(1);
  TASCAR::filter_t filter2(A, B);
  filter2.filter(&res, &delta);
  EXPECT_EQ(1.0f, delta[0]);
  EXPECT_EQ(0.0f, delta[1]);
  EXPECT_EQ(1.0f, res[0]);
  EXPECT_EQ(1.0f, res[1]);
  EXPECT_EQ(0.0f, res[2]);
}

TEST(biquad_t, unitgain)
{
  TASCAR::biquad_t b;
  EXPECT_EQ(1.0, b.get_b0());
  EXPECT_EQ(0.0, b.get_b1());
  EXPECT_EQ(0.0, b.get_b2());
  EXPECT_EQ(0.0, b.get_a1());
  EXPECT_EQ(0.0, b.get_a2());
  ASSERT_NEAR(1.0, std::abs(b.response(0 * TASCAR_2PI)), 1e-9);
  ASSERT_NEAR(1.0, std::abs(b.response(0.25 * TASCAR_2PI)), 1e-9);
  ASSERT_NEAR(1.0, std::abs(b.response(0.5 * TASCAR_2PI)), 1e-9);
  ASSERT_NEAR(1.0, std::abs(b.response(0.75 * TASCAR_2PI)), 1e-9);
  ASSERT_NEAR(1.0, b.filter(1.0), 1e-9);
  ASSERT_NEAR(1.0, b.filter(1.0), 1e-9);
  ASSERT_NEAR(1.0, b.filter(1.0), 1e-9);
  ASSERT_NEAR(1.0, b.filter(1.0), 1e-9);
}

TEST(biquadf_t, unitgain)
{
  TASCAR::biquadf_t b;
  EXPECT_EQ(1.0f, b.get_b0());
  EXPECT_EQ(0.0f, b.get_b1());
  EXPECT_EQ(0.0f, b.get_b2());
  EXPECT_EQ(0.0f, b.get_a1());
  EXPECT_EQ(0.0f, b.get_a2());
  ASSERT_NEAR(1.0f, std::abs(b.response(0.0f * TASCAR_2PIf)), 1e-7f);
  ASSERT_NEAR(1.0f, std::abs(b.response(0.25f * TASCAR_2PIf)), 1e-7f);
  ASSERT_NEAR(1.0f, std::abs(b.response(0.5f * TASCAR_2PIf)), 1e-7f);
  ASSERT_NEAR(1.0f, std::abs(b.response(0.75f * TASCAR_2PIf)), 1e-7f);
  ASSERT_NEAR(1.0f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(1.0f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(1.0f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(1.0f, b.filter(1.0f), 1e-9f);
}

TEST(biquad_t, setgzp)
{
  TASCAR::biquad_t b;
  b.set_gzp(1.0, 1.0, 0.0, 0.0, 0.0);
  ASSERT_NEAR(1.0, b.get_b0(), 1e-9);
  ASSERT_NEAR(-2.0, b.get_b1(), 1e-9);
  ASSERT_NEAR(1.0, b.get_b2(), 1e-9);
  ASSERT_NEAR(0.0, b.get_a1(), 1e-9);
  ASSERT_NEAR(0.0, b.get_a2(), 1e-9);
}

TEST(biquadf_t, setgzp)
{
  TASCAR::biquadf_t b;
  b.set_gzp(1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
  ASSERT_NEAR(1.0f, b.get_b0(), 1e-9f);
  ASSERT_NEAR(-2.0f, b.get_b1(), 1e-9f);
  ASSERT_NEAR(1.0f, b.get_b2(), 1e-9f);
  ASSERT_NEAR(0.0f, b.get_a1(), 1e-9f);
  ASSERT_NEAR(0.0f, b.get_a2(), 1e-9f);
}

TEST(biquadf_t, fresp)
{
  TASCAR::biquadf_t b;
  b.set_gzp(1.0f, 1.0f, 0.0f, 0.5f, 0.5f * TASCAR_2PIf);
  ASSERT_NEAR(0.0f, std::abs(b.response(0.0f * TASCAR_2PIf)), 1e-6f);
  ASSERT_NEAR(0.25f, std::abs(b.response_a(0.5f * TASCAR_2PIf)), 1e-6f);
  ASSERT_NEAR(4.0f, std::abs(b.response_b(0.5f * TASCAR_2PIf)), 1e-6f);
  ASSERT_NEAR(16.0f, std::abs(b.response(0.5f * TASCAR_2PIf)), 1e-5f);
  ASSERT_NEAR(1.0f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(-2.0f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(1.75f, b.filter(1.0f), 1e-9f);
  ASSERT_NEAR(-1.25f, b.filter(1.0f), 1e-9f);
}

TEST(bandpass_t, gain)
{
  double fs(44100);
  TASCAR::wave_t sin1k((uint32_t)fs);
  for(uint32_t k = 0; k < sin1k.size(); ++k)
    sin1k.d[k] =
        sqrtf(2.0f) * sinf((float)k * 1000.0f * TASCAR_2PIf / (float)fs);
  TASCAR::bandpass_t bp(500.0, 4000.0, fs);
  ASSERT_NEAR(0.0f, 10 * log10(sin1k.ms()), 3e-4f);
  for(uint32_t k = 0; k < sin1k.size(); ++k)
    sin1k.d[k] = (float)(bp.filter((double)(sin1k.d[k])));
  ASSERT_NEAR(0.0f, 10.0 * log10(sin1k.ms()), 0.04f);
  TASCAR::wave_t sin500((uint32_t)fs);
  for(uint32_t k = 0; k < sin500.size(); ++k)
    sin500.d[k] =
        sqrtf(2.0f) * sinf((float)k * 500.0f * TASCAR_2PIf / (float)fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin500.ms()), 0.04f);
  for(uint32_t k = 0; k < sin500.size(); ++k)
    sin500.d[k] = (float)(bp.filter((double)(sin500.d[k])));
  ASSERT_NEAR(-4.3f, 10.0 * log10(sin500.ms()), 0.04f);
  TASCAR::wave_t sin250((uint32_t)fs);
  for(uint32_t k = 0; k < sin250.size(); ++k)
    sin250.d[k] =
        sqrtf(2.0f) * sinf((float)k * 250.0f * TASCAR_2PIf / (float)fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin250.ms()), 0.04f);
  for(uint32_t k = 0; k < sin250.size(); ++k)
    sin250.d[k] = (float)(bp.filter((double)(sin250.d[k])));
  ASSERT_NEAR(-15.88f, 10.0 * log10(sin250.ms()), 0.04f);
  TASCAR::wave_t sin4000((uint32_t)fs);
  for(uint32_t k = 0; k < sin4000.size(); ++k)
    sin4000.d[k] =
        sqrtf(2.0f) * sinf((float)k * 4000.0f * TASCAR_2PIf / (float)fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin4000.ms()), 0.04f);
  for(uint32_t k = 0; k < sin4000.size(); ++k)
    sin4000.d[k] = (float)(bp.filter((double)(sin4000.d[k])));
  ASSERT_NEAR(-1.0f, 10.0 * log10(sin4000.ms()), 0.04f);
  TASCAR::wave_t sin8000((uint32_t)fs);
  for(uint32_t k = 0; k < sin8000.size(); ++k)
    sin8000.d[k] =
        sqrtf(2.0f) * sinf((float)k * 8000.0f * TASCAR_2PIf / (float)fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin8000.ms()), 0.04f);
  for(uint32_t k = 0; k < sin8000.size(); ++k)
    sin8000.d[k] = (float)(bp.filter((double)(sin8000.d[k])));
  ASSERT_NEAR(-10.39f, 10.0 * log10(sin8000.ms()), 0.04f);
}

TEST(bandpassf_t, gain)
{
  float fs(44100);
  TASCAR::wave_t sin1k((uint32_t)fs);
  for(uint32_t k = 0; k < sin1k.size(); ++k)
    sin1k.d[k] = sqrtf(2.0f) * sinf((float)k * 1000.0f * TASCAR_2PIf / fs);
  TASCAR::bandpassf_t bp(500.0f, 4000.0f, fs);
  ASSERT_NEAR(0.0f, 10 * log10(sin1k.ms()), 3e-4f);
  for(uint32_t k = 0; k < sin1k.size(); ++k)
    sin1k.d[k] = bp.filter(sin1k.d[k]);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin1k.ms()), 0.04f);
  TASCAR::wave_t sin500((uint32_t)fs);
  for(uint32_t k = 0; k < sin500.size(); ++k)
    sin500.d[k] = sqrtf(2.0f) * sinf((float)k * 500.0f * TASCAR_2PIf / fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin500.ms()), 0.04f);
  for(uint32_t k = 0; k < sin500.size(); ++k)
    sin500.d[k] = bp.filter(sin500.d[k]);
  ASSERT_NEAR(-4.3f, 10.0 * log10(sin500.ms()), 0.04f);
  TASCAR::wave_t sin250((uint32_t)fs);
  for(uint32_t k = 0; k < sin250.size(); ++k)
    sin250.d[k] = sqrtf(2.0f) * sinf((float)k * 250.0f * TASCAR_2PIf / fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin250.ms()), 0.04f);
  for(uint32_t k = 0; k < sin250.size(); ++k)
    sin250.d[k] = bp.filter(sin250.d[k]);
  ASSERT_NEAR(-15.88f, 10.0 * log10(sin250.ms()), 0.04f);
  TASCAR::wave_t sin4000((uint32_t)fs);
  for(uint32_t k = 0; k < sin4000.size(); ++k)
    sin4000.d[k] = sqrtf(2.0f) * sinf((float)k * 4000.0f * TASCAR_2PIf / fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin4000.ms()), 0.04f);
  for(uint32_t k = 0; k < sin4000.size(); ++k)
    sin4000.d[k] = bp.filter(sin4000.d[k]);
  ASSERT_NEAR(-1.0f, 10.0 * log10(sin4000.ms()), 0.04f);
  TASCAR::wave_t sin8000((uint32_t)fs);
  for(uint32_t k = 0; k < sin8000.size(); ++k)
    sin8000.d[k] = sqrtf(2.0f) * sinf((float)k * 8000.0f * TASCAR_2PIf / fs);
  ASSERT_NEAR(0.0f, 10.0 * log10(sin8000.ms()), 0.04f);
  for(uint32_t k = 0; k < sin8000.size(); ++k)
    sin8000.d[k] = bp.filter(sin8000.d[k]);
  ASSERT_NEAR(-10.39f, 10.0 * log10(sin8000.ms()), 0.04f);
}

TEST(biquad_t, analog)
{
  TASCAR::biquad_t b;
  // in octave:
  // [ZB,ZA] = bilinear( [20,20], [1000,1000], 1, 1/44100 )
  b.set_analog(1.0, 20.0, 20.0, 1000.0, 1000.0, 44100.0);
  // boundaries:
  ASSERT_NEAR(-2.04587156, b.get_a1(), 1e-5);
  ASSERT_NEAR(1.04639761, b.get_a2(), 1e-5);
  ASSERT_NEAR(1.022603369, b.get_b0(), 1e-5);
  ASSERT_NEAR(-2.046134479, b.get_b1(), 1e-5);
  ASSERT_NEAR(1.023531321, b.get_b2(), 1e-5);
  // in octave:
  // [ZB,ZA] = bilinear( [], [1000,1000], 1, 1/44100 )
  b.set_analog_poles(1.0, 1000.0, 1000.0, 44100.0);
  // boundaries:
  ASSERT_NEAR(-2.04587156, b.get_a1(), 1e-5);
  ASSERT_NEAR(1.04639761, b.get_a2(), 1e-5);
  ASSERT_NEAR(1.315124989e-10, b.get_b0(), 1e-5);
  ASSERT_NEAR(2.630249979e-10, b.get_b1(), 1e-5);
  ASSERT_NEAR(1.315124989e-10, b.get_b2(), 1e-5);
}

TEST(biquad, butterworthlow)
{
  TASCAR::biquad_t b;
  b.set_butterworth(80.0, 44100.0);
  ASSERT_NEAR(3.221897217e-05, b.get_b0(), 1e-9);
  ASSERT_NEAR(6.443794434e-05, b.get_b1(), 1e-9);
  ASSERT_NEAR(3.221897217e-05, b.get_b2(), 1e-9);
  ASSERT_NEAR(-1.983881042, b.get_a1(), 1e-5);
  ASSERT_NEAR(0.9840099175, b.get_a2(), 1e-5);
  b.set_butterworth(8000.0, 44100.0);
  ASSERT_NEAR(0.1772450255, b.get_b0(), 1e-6);
  ASSERT_NEAR(0.3544900511, b.get_b1(), 1e-6);
  ASSERT_NEAR(0.1772450255, b.get_b2(), 1e-6);
  ASSERT_NEAR(-0.5087175281, b.get_a1(), 1e-5);
  ASSERT_NEAR(0.2176976303, b.get_a2(), 1e-5);
}

TEST(biquad, butterworthhigh)
{
  TASCAR::biquad_t b;
  b.set_butterworth(80.0, 44100.0, true);
  ASSERT_NEAR(0.9919727398, b.get_b0(), 1e-5);
  ASSERT_NEAR(-1.98394548, b.get_b1(), 1e-5);
  ASSERT_NEAR(0.9919727398, b.get_b2(), 1e-5);
  ASSERT_NEAR(-1.983881042, b.get_a1(), 1e-5);
  ASSERT_NEAR(0.9840099175, b.get_a2(), 1e-5);
  b.set_butterworth(8000.0, 44100.0, true);
  ASSERT_NEAR(0.4316037896, b.get_b0(), 1e-6);
  ASSERT_NEAR(-0.8632075792, b.get_b1(), 1e-6);
  ASSERT_NEAR(0.4316037896, b.get_b2(), 1e-6);
  ASSERT_NEAR(-0.5087175281, b.get_a1(), 1e-5);
  ASSERT_NEAR(0.2176976303, b.get_a2(), 1e-5);
}

TEST(biquadf_t, pareq)
{
  TASCAR::biquadf_t b;
  b.set_pareq(1000.0f, 44100.0f, 10.0f, 0.8f);
  ASSERT_NEAR(
      10.0f,
      20.0f * log10f(std::abs(b.response(1000.0f / 44100.0f * TASCAR_2PIf))),
      0.1f);
}

TEST(biquadf_t, analog)
{
  TASCAR::biquadf_t b;
  // in octave:
  // [ZB,ZA] = bilinear( [20,20], [1000,1000], 1, 1/44100 )
  b.set_analog(1.0f, 20.0f, 20.0f, 1000.0f, 1000.0f, 44100.0f);
  // boundaries:
  ASSERT_NEAR(-2.04587156f, b.get_a1(), 1e-5f);
  ASSERT_NEAR(1.04639761f, b.get_a2(), 1e-5f);
  ASSERT_NEAR(1.022603369f, b.get_b0(), 1e-5f);
  ASSERT_NEAR(-2.046134479f, b.get_b1(), 1e-5f);
  ASSERT_NEAR(1.023531321f, b.get_b2(), 1e-5f);
  // in octave:
  // [ZB,ZA] = bilinear( [], [1000,1000], 1, 1/44100 )
  b.set_analog_poles(1.0f, 1000.0f, 1000.0f, 44100.0f);
  // boundaries:
  ASSERT_NEAR(-2.04587156f, b.get_a1(), 1e-5f);
  ASSERT_NEAR(1.04639761f, b.get_a2(), 1e-5f);
  ASSERT_NEAR(1.315124989e-10f, b.get_b0(), 1e-5f);
  ASSERT_NEAR(2.630249979e-10f, b.get_b1(), 1e-5f);
  ASSERT_NEAR(1.315124989e-10f, b.get_b2(), 1e-5f);
}

TEST(aweighting_t, fresponse)
{
  uint32_t fs(44100);
  TASCAR::wave_t x(4 * fs);
  std::map<double, double> t;
  // table from here:
  // https://www.nti-audio.com/en/support/know-how/frequency-weightings-for-sound-level-measurements
  // t[8] = -77.8;
  // t[16] = -56.7;
  t[31.5] = -39.4;
  t[63] = -26.2;
  t[125] = -16.1;
  t[250] = -8.6;
  t[500] = -3.2;
  t[1000] = 0;
  t[2000] = 1.2;
  t[4000] = 1.0;
  t[5000] = 0.5;
  // t[6300] = -0.1;
  // t[8000] = -1.1;
  // t[16000] = -6.6;
  for(auto it = t.begin(); it != t.end(); ++it) {
    TASCAR::aweighting_t a(fs);
    for(uint32_t k = 0; k < x.n; ++k)
      x.d[k] = sqrtf(2.0f) *
               sinf(TASCAR_2PIf * (float)(it->first) * (float)k / (float)fs);
    ASSERT_NEAR(0.0f, 10.0f * log10f(x.ms()), 0.001f);
    a.filter(x);
    ASSERT_NEAR((float)(it->second), 10.0f * log10f(x.ms()), 0.5f);
  }
}

TEST(multiband_pareq_t, response1flt)
{
  TASCAR::multiband_pareq_t eq;
  float fs = 44100.0f;
  eq.set_fgq({1000.0f}, {3.0f}, {0.5f}, fs);
  std::vector<float> f = {125.0f,  157.5f,  198.4f,  250.0f,  315.0f,
                          396.9f,  500.0f,  630.0f,  793.7f,  1000.0f,
                          1259.9f, 1587.4f, 2000.0f, 2519.8f, 3174.8f,
                          4000.0f, 5039.7f, 6349.6f, 8000.0f};
  std::vector<float> g = eq.dbresponse(f, fs);
  ASSERT_NEAR(g[0], 0.25f, 0.01f);
  ASSERT_NEAR(g[1], 0.39f, 0.01f);
  ASSERT_NEAR(g[2], 0.59f, 0.01f);
  ASSERT_NEAR(g[3], 0.86f, 0.01f);
  ASSERT_NEAR(g[4], 1.23f, 0.01f);
  ASSERT_NEAR(g[5], 1.66f, 0.01f);
  ASSERT_NEAR(g[6], 2.14, 0.01f);
  ASSERT_NEAR(g[7], 2.57f, 0.01f);
  ASSERT_NEAR(g[8], 2.89f, 0.01f);
  ASSERT_NEAR(g[9], 3.00f, 0.01f);
  ASSERT_NEAR(g[10], 2.89f, 0.01f);
  ASSERT_NEAR(g[11], 2.57f, 0.01f);
  ASSERT_NEAR(g[12], 2.13f, 0.01f);
  ASSERT_NEAR(g[13], 1.65f, 0.01f);
  ASSERT_NEAR(g[14], 1.20f, 0.01f);
  ASSERT_NEAR(g[15], 0.83f, 0.01f);
  ASSERT_NEAR(g[16], 0.55f, 0.01f);
  ASSERT_NEAR(g[17], 0.34f, 0.01f);
  ASSERT_NEAR(g[18], 0.20f, 0.01f);
}

TEST(multiband_pareq_t, response3flt)
{
  TASCAR::multiband_pareq_t eq;
  float fs = 44100.0f;
  eq.set_fgq({500.0f, 1000.0f, 2000.0f}, {-3.0f, 3.0f, 1.0f},
             {0.9f, 0.5f, 0.1f}, fs);
  std::vector<float> f = {125.0f,  157.5f,  198.4f,  250.0f,  315.0f,
                          396.9f,  500.0f,  630.0f,  793.7f,  1000.0f,
                          1259.9f, 1587.4f, 2000.0f, 2519.8f, 3174.8f,
                          4000.0f, 5039.7f, 6349.6f, 8000.0f};
  std::vector<float> g = eq.dbresponse(f, fs);
  ASSERT_NEAR(g[0], 0.221956f, 0.01f);
  ASSERT_NEAR(g[1], 0.266738f, 0.01f);
  ASSERT_NEAR(g[2], 0.271461f, 0.01f);
  ASSERT_NEAR(g[3], 0.1926f, 0.01f);
  ASSERT_NEAR(g[4], 0.00667759f, 0.01f);
  ASSERT_NEAR(g[5], -0.173332f, 0.01f);
  ASSERT_NEAR(g[6], 0.0248456f, 0.01f);
  ASSERT_NEAR(g[7], 0.84333f, 0.01f);
  ASSERT_NEAR(g[8], 1.88679f, 0.01f);
  ASSERT_NEAR(g[9], 2.67132f, 0.01f);
  ASSERT_NEAR(g[10], 3.03919f, 0.01f);
  ASSERT_NEAR(g[11], 3.041f, 0.01f);
  ASSERT_NEAR(g[12], 2.79876f, 0.01f);
  ASSERT_NEAR(g[13], 2.4401f, 0.01f);
  ASSERT_NEAR(g[14], 2.06435f, 0.01f);
  ASSERT_NEAR(g[15], 1.72954f, 0.01f);
  ASSERT_NEAR(g[16], 1.45555f, 0.01f);
  ASSERT_NEAR(g[17], 1.23594f, 0.01f);
  ASSERT_NEAR(g[18], 1.04929f, 0.01f);
}

TEST(multiband_pareq_t, responseoptim)
{
  TASCAR::multiband_pareq_t eq;
  float fs = 44100.0f;
  std::vector<float> f = {125.0f,  157.5f,  198.4f,  250.0f,  315.0f,
                          396.9f,  500.0f,  630.0f,  793.7f,  1000.0f,
                          1259.9f, 1587.4f, 2000.0f, 2519.8f, 3174.8f,
                          4000.0f, 5039.7f, 6349.6f, 8000.0f};
  std::vector<float> g = {0.221956f,   0.266738f,  0.271461f,  0.1926f,
                          0.00667759f, -0.173332f, 0.0248456f, 0.84333f,
                          1.88679f,    2.67132f,   3.03919f,   3.041f,
                          2.79876f,    2.4401f,    2.06435f,   1.72954f,
                          1.45555f,    1.23594f,   1.04929f};
  std::vector<float> gmeas = eq.optim_response(6, 1.0f, f, g, fs);
  //  for(size_t k = 0; k < gmeas.size(); ++k)
  //    std::cout << gmeas[k]-g[k] << "\n";
  //  std::cout << TASCAR::to_string(gmeas) << std::endl;
  ASSERT_NEAR(gmeas[0], 0.365156f, 0.5f);
  ASSERT_NEAR(gmeas[1], 0.208671f, 0.5f);
  ASSERT_NEAR(gmeas[2], 0.00160515f, 0.5f);
  ASSERT_NEAR(gmeas[3], -0.154365f, 0.5f);
  ASSERT_NEAR(gmeas[4], -0.154342f, 0.5f);
  ASSERT_NEAR(gmeas[5], 0.0435356f, 0.5f);
  ASSERT_NEAR(gmeas[6], 0.424508f, 0.5f);
  ASSERT_NEAR(gmeas[7], 0.979241f, 0.5f);
  ASSERT_NEAR(gmeas[8], 1.68132f, 0.5f);
  ASSERT_NEAR(gmeas[9], 2.39932f, 0.5f);
  ASSERT_NEAR(gmeas[10], 2.90098f, 0.5f);
  ASSERT_NEAR(gmeas[11], 3.0195f, 0.5f);
  ASSERT_NEAR(gmeas[12], 2.80379f, 0.5f);
  ASSERT_NEAR(gmeas[13], 2.43202f, 0.5f);
  ASSERT_NEAR(gmeas[14], 2.04437f, 0.5f);
  ASSERT_NEAR(gmeas[15], 1.70262f, 0.5f);
  ASSERT_NEAR(gmeas[16], 1.42086f, 0.5f);
  ASSERT_NEAR(gmeas[17], 1.18439f, 0.5f);
  ASSERT_NEAR(gmeas[18], 0.953375f, 0.5f);
}

TEST(rflt2alpha, vals)
{
  std::vector<float> vfreq = {125.0f,  250.0f,  500.0f,
                              1000.0f, 2000.0f, 4000.0f};
  auto alpha = TASCAR::rflt2alpha(0.7709f, 0.21f, 44100.0f, vfreq);
  ASSERT_EQ(6u, alpha.size());
  if(alpha.size() == 6) {
    ASSERT_NEAR(0.0525, alpha[0], 1e-4f);
    ASSERT_NEAR(0.0525, alpha[1], 1e-4f);
    ASSERT_NEAR(0.0528, alpha[2], 1e-4f);
    ASSERT_NEAR(0.0537, alpha[3], 1e-4f);
    ASSERT_NEAR(0.0573, alpha[4], 1e-4f);
    ASSERT_NEAR(0.0713, alpha[5], 1e-4f);
  }
}

TEST(alpha2rflt, vals)
{
  std::vector<float> vfreq = {125.0f,  250.0f,  500.0f,
                              1000.0f, 2000.0f, 4000.0f};
  std::vector<float> alpha = {0.0400f, 0.0400f, 0.0700f,
                              0.0600f, 0.0600f, 0.0700f};
  float reflectivity = 1.0f;
  float damping = 0.5f;
  // int err =
  TASCAR::alpha2rflt(reflectivity, damping, alpha, vfreq, 44100.0f);
  ASSERT_NEAR(0.7709, reflectivity, 1e-4f);
  ASSERT_NEAR(0.21, damping, 2e-3f);
  // ASSERT_EQ(0, err);
}

TEST(acorr, vals)
{
  // Octave reference: xcorr([1,2,3,-4,-5,0,1,2,3,4]',3,'biased')
  TASCAR::wave_t x(std::vector<float>(
      {1.0f, 2.0f, 3.0f, -4.0f, -5.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f}));
  TASCAR::acorr_t r(3);
  r.corr(x);
  // expect four elements (lag + 1):
  ASSERT_EQ(4u, r.n);
  // expect values, starting at center value:
  ASSERT_NEAR(8.5f, r.d[0], 1e-6f);
  ASSERT_NEAR(3.6f, r.d[1], 1e-6f);
  ASSERT_NEAR(-1.4f, r.d[2], 1e-6f);
  ASSERT_NEAR(-2.4f, r.d[3], 1e-6f);
}

TEST(lpc,vals)
{
  // octave:74> acf = xcorr([1,2,3,-4,-5,0,1,2,3,4]',3,'biased')
  // acf =
  //
  //   -2.4000
  //   -1.4000
  //    3.6000
  //    8.5000
  //    3.6000
  //   -1.4000
  //   -2.4000
  //
  // octave:75> [A, tmp, res] = levinson(acf(4:end))
  // A =
  //
  //    1.0000e+00  -5.9754e-01   4.1417e-01   8.5197e-03
  //
  // tmp = 5.7486
  // res =
  //
  //   -4.2353e-01
  //    4.1929e-01
  //    8.5197e-03
  TASCAR::wave_t x(std::vector<float>(
      {1.0f, 2.0f, 3.0f, -4.0f, -5.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f}));
  TASCAR::lpc_t lpc(3);
  lpc.compute_coeffs(x);
  EXPECT_EQ(4u, lpc.n);
  EXPECT_NEAR(1.0f, lpc.d[0], 1e-6);
  EXPECT_NEAR(-0.597540904f, lpc.d[1], 1e-6);
  EXPECT_NEAR(0.414173686f, lpc.d[2], 1e-6);
  EXPECT_NEAR(0.00851970181f, lpc.d[3], 1e-6);

  TASCAR::wave_t res(x.n);
  lpc.analyse(x, res);

  EXPECT_NEAR(1, res.d[0], 1e-6f);
  EXPECT_NEAR(1.4024591, res.d[1], 1e-6f);
  EXPECT_NEAR(2.21909188, res.d[2], 1e-6f);
  EXPECT_NEAR(-4.95575564, res.d[3], 1e-6f);
  EXPECT_NEAR(-1.35027592, res.d[4], 1e-6f);
  EXPECT_NEAR(1.35656888, res.d[5], 1e-6f);
  EXPECT_NEAR(-1.10494724, res.d[6], 1e-6f);
  EXPECT_NEAR(1.35986059, res.d[7], 1e-6f);
  EXPECT_NEAR(2.21909188, res.d[8], 1e-6f);
  EXPECT_NEAR(3.04424436, res.d[9], 1e-6f);
}


// Local Variables:
// compile-command: "make -C ../.. unit-tests"
// coding: utf-8-unix
// c-basic-offset: 2
// indent-tabs-mode: nil
// End:
