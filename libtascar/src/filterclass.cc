/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
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

#include "filterclass.h"

#include "errorhandling.h"
#include "optim.h"
#include "tscconfig.h"
#include <string.h>

const std::complex<double> i(0.0, 1.0);
const std::complex<float> i_f(0.0f, 1.0f);

TASCAR::filter_t::filter_t(unsigned int ilen_A, unsigned int ilen_B)
    : A(NULL), B(NULL), len_A(ilen_A), len_B(ilen_B), len(0), state(NULL)
{
  unsigned int k;
  // recursive and non-recursive coefficients need at least one entry:
  len = std::max(len_A, len_B);
  if(std::min(len_A, len_B) == 0)
    throw TASCAR::ErrMsg("invalid filter length: 0");
  // allocate filter coefficient buffers and initialize to identity:
  A = new double[len_A];
  memset(A, 0, sizeof(A[0]) * len_A);
  A[0] = 1.0;
  B = new double[len_B];
  memset(B, 0, sizeof(B[0]) * len_B);
  B[0] = 1.0;
  // allocate filter state buffer and initialize to zero:
  state = new double[len];
  for(k = 0; k < len; k++)
    state[k] = 0;
}

TASCAR::filter_t::filter_t(const TASCAR::filter_t& src)
    : A(new double[src.len_A]), B(new double[src.len_B]), len_A(src.len_A),
      len_B(src.len_B), len(src.len), state(new double[len])
{
  memmove(A, src.A, len_A * sizeof(double));
  memmove(B, src.B, len_B * sizeof(double));
  memmove(state, src.state, len * sizeof(double));
}

TASCAR::filter_t::filter_t(const std::vector<double>& vA,
                           const std::vector<double>& vB)
    : A(NULL), B(NULL), len_A((unsigned int)(vA.size())),
      len_B((unsigned int)(vB.size())), len(0), state(NULL)
{
  unsigned int k;
  // recursive and non-recursive coefficients need at least one entry:
  if(vA.size() == 0)
    throw TASCAR::ErrMsg("Recursive coefficients are empty.");
  if(vB.size() == 0)
    throw TASCAR::ErrMsg("Non-recursive coefficients are empty.");
  len = std::max(len_A, len_B);
  // allocate filter coefficient buffers and initialize to identity:
  A = new double[len_A];
  B = new double[len_B];
  for(k = 0; k < len_A; k++)
    A[k] = vA[k];
  for(k = 0; k < len_B; k++)
    B[k] = vB[k];
  // allocate filter state buffer and initialize to zero:
  state = new double[len];
  for(k = 0; k < len; k++)
    state[k] = 0;
}

void TASCAR::filter_t::filter(TASCAR::wave_t* dest, const TASCAR::wave_t* src)
{
  if(dest->n != src->n)
    throw TASCAR::ErrMsg("mismatching number of frames");
  filter(dest->d, src->d, dest->n, 1);
}

double TASCAR::filter_t::filter(float x)
{
  float y = 0;
  filter(&y, &x, 1, 1);
  return y;
}

void TASCAR::filter_t::filter(float* dest, const float* src,
                              unsigned int dframes, unsigned int frame_dist)
{
  // direct form II, one delay line for each channel
  // A[k] are the recursive filter coefficients (A[0] is typically 1)
  // B[k] are the non recursive filter coefficients
  // loop through all frames, and all channels:
  for(uint32_t fr = 0; fr < dframes; ++fr) {
    // index into input/output buffer:
    uint32_t idx(frame_dist * fr);
    // shift filter delay line for current channel:
    for(uint32_t n = len - 1; n > 0; n--)
      state[n] = state[n - 1];
    // replace first delay line entry by input signal:
    // state[ch] =  src[idx] / A[0];
    state[0] = src[idx];
    // apply recursive coefficients:
    for(uint32_t n = 1; n < len_A; ++n)
      state[0] -= state[n] * A[n];
    make_friendly_number(state[0]);
    // apply non recursive coefficients to output:
    dest[idx] = 0;
    for(uint32_t n = 0; n < len_B; ++n)
      dest[idx] += (float)(state[n] * B[n]);
    // normalize by first recursive element:
    dest[idx] /= (float)(A[0]);
    make_friendly_number(dest[idx]);
  }
}

TASCAR::filter_t::~filter_t()
{
  delete[] A;
  delete[] B;
  delete[] state;
}

void normalize_vec(std::vector<float>& v)
{
  float norm(0.0f);
  for(std::vector<float>::const_iterator it = v.begin(); it != v.end(); ++it)
    norm += fabsf(*it);
  if(norm > 0) {
    for(std::vector<float>::iterator it = v.begin(); it != v.end(); ++it)
      *it *= 1.0f / norm;
  }
}

TASCAR::fsplit_t::fsplit_t(uint32_t maxdelay, shape_t shape, uint32_t tau)
    : TASCAR::wave_t(maxdelay)
{
  switch(shape) {
  case none:
    vd.resize(1);
    w1.resize(1);
    w2.resize(1);
    vd[0] = d;
    w1[0] = 1.0f;
    w2[0] = 0.0f;
    break;
  case notch:
    vd.resize(2);
    w1.resize(2);
    w2.resize(2);
    vd[0] = d;
    vd[1] = d + tau;
    w1[0] = w2[0] = w1[1] = 1.0f;
    w2[1] = -1.0f;
    break;
  case sine:
    vd.resize(3);
    w1.resize(3);
    w2.resize(3);
    vd[0] = d;
    vd[1] = d + tau;
    vd[2] = d + 2 * tau;
    w1[0] = w1[2] = 1.0f;
    w2[0] = w2[2] = -1.0f;
    w1[1] = w2[1] = 2.0f;
    break;
  case tria:
    vd.resize(5);
    w1.resize(5);
    w2.resize(5);
    vd[0] = d;
    vd[1] = d + 2 * tau;
    vd[2] = d + 3 * tau;
    vd[3] = d + 4 * tau;
    vd[4] = d + 6 * tau;
    w1[0] = w1[4] = 1.0f / 9.0f;
    w1[1] = w1[3] = 1.0f;
    w1[2] = w2[2] = 2.0f + 2.0f / 9.0f;
    w2[1] = w2[3] = -1.0f;
    w2[0] = w2[4] = -1.0f / 9.0f;
    break;
  case triald:
    vd.resize(3);
    w1.resize(3);
    w2.resize(3);
    vd[0] = d;
    vd[1] = d + tau;
    vd[2] = d + 3 * tau;
    w1[0] = w2[0] = w1[1] = 1.0f;
    w2[1] = -1.0f;
    w1[2] = 1.0f / 9.0f;
    w2[2] = -1.0f / 9.0f;
    break;
  }
  normalize_vec(w1);
  normalize_vec(w2);
  for(std::vector<float*>::const_iterator it = vd.begin(); it != vd.end(); ++it)
    if((*it) >= d + n)
      throw TASCAR::ErrMsg("Delay exceeds buffer length");
}

TASCAR::resonance_filter_t::resonance_filter_t() : statey1(0), statey2(0)
{
  set_fq(0.1, 0.5);
}

void TASCAR::resonance_filter_t::set_fq(double fresnorm, double q)
{
  const double farg(TASCAR_2PI * fresnorm);
  b1 = 2.0 * q * cos(farg);
  b2 = -q * q;
  std::complex<double> z(std::exp(i * farg));
  std::complex<double> z0(q * std::exp(-i * farg));
  a1 = (1.0 - q) * (std::abs(z - z0));
}

void TASCAR::biquad_t::set_gzp(double g, double zero_r, double zero_phi,
                               double pole_r, double pole_phi)
{
  a1_ = -2.0 * pole_r * cos(pole_phi);
  a2_ = pole_r * pole_r;
  b0_ = g;
  b1_ = -2.0 * g * zero_r * cos(zero_phi);
  b2_ = g * zero_r * zero_r;
}

void TASCAR::biquad_t::set_allpass(double r, double phi)
{
  a1_ = -2.0 * r * cos(phi);
  a2_ = r * r;
  b0_ = a2_;
  b1_ = a1_;
  b2_ = 1.0;
}

void TASCAR::biquadf_t::set_allpass(float r, float phi)
{
  a1_ = -2.0f * r * cos(phi);
  a2_ = r * r;
  b0_ = a2_;
  b1_ = a1_;
  b2_ = 1.0f;
}

void TASCAR::biquadf_t::set_gzp(float g, float zero_r, float zero_phi,
                                float pole_r, float pole_phi)
{
  a1_ = -2.0f * pole_r * cosf(pole_phi);
  a2_ = pole_r * pole_r;
  b0_ = g;
  b1_ = -2.0f * g * zero_r * cosf(zero_phi);
  b2_ = g * zero_r * zero_r;
}

// pre-warping of frequency
double fd2fa(double fs, double fd)
{
  return 2.0 * fs * tan(fd / (2.0 * fs));
}

double fa2fd(double fs, double fa)
{
  return 2.0 * fs * atan(fa / (2.0 * fs));
}

float fa2fdf(float fs, float fa)
{
  return 2.0f * fs * atanf(fa / (2.0f * fs));
}

void TASCAR::biquad_t::set_analog(double g, double z1, double z2, double p1,
                                  double p2, double fs)
{
  z1 = fa2fd(fs, z1) / fs;
  z2 = fa2fd(fs, z2) / fs;
  p1 = fa2fd(fs, p1) / fs;
  p2 = fa2fd(fs, p2) / fs;
  g *= (2.0 - z1) / (2.0 - p1) * (2.0 - z2) / (2.0 - p2);
  double z1_((2.0 + z1) / (2.0 - z1));
  double z2_((2.0 + z2) / (2.0 - z2));
  double p1_((2.0 + p1) / (2.0 - p1));
  double p2_((2.0 + p2) / (2.0 - p2));
  b1_ = -(z1_ + z2_) * g;
  b2_ = z1_ * z2_ * g;
  b0_ = g;
  a1_ = -(p1_ + p2_);
  a2_ = p1_ * p2_;
}

void TASCAR::biquadf_t::set_analog(float g, float z1, float z2, float p1,
                                   float p2, float fs)
{
  z1 = fa2fdf(fs, z1) / fs;
  z2 = fa2fdf(fs, z2) / fs;
  p1 = fa2fdf(fs, p1) / fs;
  p2 = fa2fdf(fs, p2) / fs;
  g *= (2.0f - z1) / (2.0f - p1) * (2.0f - z2) / (2.0f - p2);
  float z1_((2.0f + z1) / (2.0f - z1));
  float z2_((2.0f + z2) / (2.0f - z2));
  float p1_((2.0f + p1) / (2.0f - p1));
  float p2_((2.0f + p2) / (2.0f - p2));
  b1_ = -(z1_ + z2_) * g;
  b2_ = z1_ * z2_ * g;
  b0_ = g;
  a1_ = -(p1_ + p2_);
  a2_ = p1_ * p2_;
}

void TASCAR::biquad_t::set_analog_poles(double g, double p1, double p2,
                                        double fs)
{
  p1 = fa2fd(fs, p1) / fs;
  p2 = fa2fd(fs, p2) / fs;
  g *= 1.0 / (fs * (2.0 - p1) * (2.0 - p2) * fs);
  double z1_(-1.0);
  double z2_(-1.0);
  double p1_((2.0 + p1) / (2.0 - p1));
  double p2_((2.0 + p2) / (2.0 - p2));
  b1_ = -(z1_ + z2_) * g;
  b2_ = z1_ * z2_ * g;
  b0_ = g;
  a1_ = -(p1_ + p2_);
  a2_ = p1_ * p2_;
}

void TASCAR::biquadf_t::set_analog_poles(float g, float p1, float p2, float fs)
{
  p1 = fa2fdf(fs, p1) / fs;
  p2 = fa2fdf(fs, p2) / fs;
  g *= 1.0f / (fs * (2.0f - p1) * (2.0f - p2) * fs);
  float z1_(-1.0f);
  float z2_(-1.0f);
  float p1_((2.0f + p1) / (2.0f - p1));
  float p2_((2.0f + p2) / (2.0f - p2));
  b1_ = -(z1_ + z2_) * g;
  b2_ = z1_ * z2_ * g;
  b0_ = g;
  a1_ = -(p1_ + p2_);
  a2_ = p1_ * p2_;
}

std::complex<double> TASCAR::biquad_t::response(double phi) const
{
  return response_b(phi) / response_a(phi);
}

std::complex<double> TASCAR::biquad_t::response_a(double phi) const
{
  std::complex<double> z1(std::exp(-i * phi));
  std::complex<double> z2(z1 * z1);
  return 1.0 + a1_ * z1 + a2_ * z2;
}

std::complex<double> TASCAR::biquad_t::response_b(double phi) const
{
  std::complex<double> z1(std::exp(-i * phi));
  std::complex<double> z2(z1 * z1);
  return b0_ + b1_ * z1 + b2_ * z2;
}

std::complex<float> TASCAR::biquadf_t::response(float phi) const
{
  return response_b(phi) / response_a(phi);
}

std::complex<float> TASCAR::biquadf_t::response_a(float phi) const
{
  std::complex<float> z1(std::exp(-i_f * phi));
  std::complex<float> z2(z1 * z1);
  return 1.0f + a1_ * z1 + a2_ * z2;
}

std::complex<float> TASCAR::biquadf_t::response_b(float phi) const
{
  std::complex<float> z1(std::exp(-i_f * phi));
  std::complex<float> z2(z1 * z1);
  return b0_ + b1_ * z1 + b2_ * z2;
}

TASCAR::bandpass_t::bandpass_t(double f1, double f2, double fs) : fs_(fs)
{
  set_range(f1, f2);
}

TASCAR::bandpass_t::bandpass_t()
{
  set_range(0.5, 1, 2);
}

void TASCAR::bandpass_t::set_range(double f1, double f2, double fs)
{
  fs_ = fs;
  set_range(f1, f2);
}

void TASCAR::bandpass_t::set_range(double f1, double f2)
{
  b1.set_gzp(1.0, 1.0, 0.0, pow(10.0, -2.0 * f1 / fs_), f1 / fs_ * TASCAR_2PI);
  b2.set_gzp(1.0, 1.0, TASCAR_PI, pow(10.0, -2.0 * f2 / fs_),
             f2 / fs_ * TASCAR_2PI);
  double f0(sqrt(f1 * f2));
  double g(std::abs(b1.response(f0 / fs_ * TASCAR_2PI) *
                    b2.response(f0 / fs_ * TASCAR_2PI)));
  b1.set_gzp(1.0 / g, 1.0, 0.0, pow(10.0, -2.0 * f1 / fs_),
             f1 / fs_ * TASCAR_2PI);
}

TASCAR::bandpassf_t::bandpassf_t(float f1, float f2, float fs) : fs_(fs)
{
  set_range(f1, f2);
}

void TASCAR::bandpassf_t::set_range(float f1, float f2)
{
  b1.set_gzp(1.0f, 1.0f, 0.0f, powf(10.0f, -2.0f * f1 / fs_),
             f1 / fs_ * TASCAR_2PIf);
  b2.set_gzp(1.0f, 1.0f, TASCAR_PIf, powf(10.0f, -2.0f * f2 / fs_),
             f2 / fs_ * TASCAR_2PIf);
  float f0(sqrtf(f1 * f2));
  float g(std::abs(b1.response(f0 / fs_ * TASCAR_2PIf) *
                   b2.response(f0 / fs_ * TASCAR_2PIf)));
  b1.set_gzp(1.0f / g, 1.0f, 0.0f, powf(10.0f, -2.0f * f1 / fs_),
             f1 / fs_ * TASCAR_2PIf);
}

void TASCAR::biquad_t::set_pareq(double f, double fs, double gain, double q)
{
  // bilinear transformation
  double t = 1.0 / tan(TASCAR_PI * f / fs);
  double t_sq = t * t;
  double Bc = t / q;
  if(gain < 0.0) {
    double g = pow(10.0, (-gain / 20.0));
    double inv_a0 = 1.0 / (t_sq + 1.0 + g * Bc);
    set_coefficients(2.0 * (1.0 - t_sq) * inv_a0,
                     (t_sq + 1.0 - g * Bc) * inv_a0, (t_sq + 1.0 + Bc) * inv_a0,
                     2.0 * (1.0 - t_sq) * inv_a0, (t_sq + 1.0 - Bc) * inv_a0);
  } else {
    double g = pow(10.0, (gain / 20.0));
    double inv_a0 = 1.0 / (t_sq + 1.0 + Bc);
    set_coefficients(2.0 * (1.0 - t_sq) * inv_a0, (t_sq + 1.0 - Bc) * inv_a0,
                     (t_sq + 1.0 + g * Bc) * inv_a0,
                     2.0 * (1.0 - t_sq) * inv_a0,
                     (t_sq + 1.0 - g * Bc) * inv_a0);
  }
}

void TASCAR::biquad_t::set_highshelf(double f, double fs, double gain, double s)
{
  // https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
  double A = pow(10.0, gain / 40.0);
  double omega0 = TASCAR_2PI * f / fs;
  double alpha =
      0.5 * sin(omega0) * sqrt((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
  double a0 = (A + 1.0) - (A - 1.0) * cos(omega0) + 2.0 * sqrt(A) * alpha;
  double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos(omega0));
  double a2 = (A + 1.0) - (A - 1.0) * cos(omega0) - 2.0 * sqrt(A) * alpha;
  double b0 = A * ((A + 1.0) + (A - 1.0) * cos(omega0) + 2.0 * sqrt(A) * alpha);
  double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos(omega0));
  double b2 = A * ((A + 1.0) + (A - 1.0) * cos(omega0) - 2.0 * sqrt(A) * alpha);
  set_coefficients(a1 / a0, a2 / a0, b0 / a0, b1 / a0, b2 / a0);
}

void TASCAR::biquad_t::set_lowshelf(double f, double fs, double gain, double s)
{
  double A = pow(10.0, gain / 40.0);
  double omega0 = TASCAR_2PI * f / fs;
  double alpha =
      0.5f * sin(omega0) * sqrt((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
  double a0 = (A + 1.0) + (A - 1.0) * cos(omega0) + 2.0 * sqrt(A) * alpha;
  double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos(omega0));
  double a2 = (A + 1.0) + (A - 1.0) * cos(omega0) - 2.0 * sqrt(A) * alpha;
  double b0 = A * ((A + 1.0) - (A - 1.0) * cos(omega0) + 2.0 * sqrt(A) * alpha);
  double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos(omega0));
  double b2 = A * ((A + 1.0) - (A - 1.0) * cos(omega0) - 2.0 * sqrt(A) * alpha);
  set_coefficients(a1 / a0, a2 / a0, b0 / a0, b1 / a0, b2 / a0);
}

void bilinear(std::vector<std::complex<double>>& poles, double& gain)
{
  std::complex<double> prod_poles = 1.0;
  for(const auto& p : poles)
    prod_poles *= (1.0 - p);
  gain = std::real(gain / prod_poles);
  for(auto& p : poles)
    p = (1.0 + p) / (1.0 - p);
}

void bilinearf(std::vector<std::complex<float>>& poles, float& gain)
{
  std::complex<float> prod_poles = 1.0;
  for(const auto& p : poles)
    prod_poles *= (1.0f - p);
  gain = std::real(gain / prod_poles);
  for(auto& p : poles)
    p = (1.0f + p) / (1.0f - p);
}

void sftrans(std::vector<std::complex<double>>& poles, double& gain, double wc,
             bool highpass)
{
  if(highpass) {
    std::complex<double> prod_poles = 1.0;
    for(const auto& p : poles)
      prod_poles *= -p;
    gain *= std::real(1.0 / prod_poles);
    for(auto& p : poles)
      p = wc / p;
  } else {
    gain *= pow(1.0 / wc, -(double)poles.size());
    for(auto& p : poles)
      p = wc * p;
  }
}

void sftransf(std::vector<std::complex<float>>& poles, float& gain, float wc,
              bool highpass)
{
  if(highpass) {
    std::complex<float> prod_poles = 1.0f;
    for(const auto& p : poles)
      prod_poles *= -p;
    gain *= std::real(1.0f / prod_poles);
    for(auto& p : poles)
      p = wc / p;
  } else {
    gain *= powf(1.0f / wc, -(float)poles.size());
    for(auto& p : poles)
      p = wc * p;
  }
}

void TASCAR::biquad_t::set_butterworth(double f, double fs, bool highpass)
{
  double wc = tan(TASCAR_PI2 * f / (0.5 * fs));
  std::vector<std::complex<double>> poles = {std::exp(i * TASCAR_PI * 0.75),
                                             std::exp(i * TASCAR_PI * 1.25)};
  double gain = 1.0;
  sftrans(poles, gain, wc, highpass);
  bilinear(poles, gain);
  if(highpass)
    set_coefficients(-std::real(poles[0] + poles[1]),
                     std::real(poles[0] * poles[1]), gain, -2.0 * gain, gain);
  else
    set_coefficients(-std::real(poles[0] + poles[1]),
                     std::real(poles[0] * poles[1]), gain, 2.0 * gain, gain);
}

void TASCAR::biquadf_t::set_butterworth(float f, float fs, bool highpass)
{
  float wc = tanf(TASCAR_PI2f * f / (0.5f * fs));
  std::vector<std::complex<float>> poles = {std::exp(i_f * TASCAR_PIf * 0.75f),
                                            std::exp(i_f * TASCAR_PIf * 1.25f)};
  float gain = 1.0f;
  sftransf(poles, gain, wc, highpass);
  bilinearf(poles, gain);
  if(highpass)
    set_coefficients(-std::real(poles[0] + poles[1]),
                     std::real(poles[0] * poles[1]), gain, -2.0f * gain, gain);
  else
    set_coefficients(-std::real(poles[0] + poles[1]),
                     std::real(poles[0] * poles[1]), gain, 2.0f * gain, gain);
}

void TASCAR::biquadf_t::set_pareq(float f, float fs, float gain, float q)
{
  // bilinear transformation
  float t = 1.0f / tanf(TASCAR_PIf * f / fs);
  float t_sq = t * t;
  float Bc = t / q;
  if(gain < 0.0f) {
    float g = powf(10.0f, (-gain / 20.0f));
    float inv_a0 = 1.0f / (t_sq + 1.0f + g * Bc);
    set_coefficients(2.0f * (1.0f - t_sq) * inv_a0,
                     (t_sq + 1.0f - g * Bc) * inv_a0,
                     (t_sq + 1.0f + Bc) * inv_a0, 2.0f * (1.0f - t_sq) * inv_a0,
                     (t_sq + 1.0f - Bc) * inv_a0);
  } else {
    float g = powf(10.0f, (gain / 20.0f));
    float inv_a0 = 1.0f / (t_sq + 1.0f + Bc);
    set_coefficients(2.0f * (1.0f - t_sq) * inv_a0, (t_sq + 1.0f - Bc) * inv_a0,
                     (t_sq + 1.0f + g * Bc) * inv_a0,
                     2.0f * (1.0f - t_sq) * inv_a0,
                     (t_sq + 1.0f - g * Bc) * inv_a0);
  }
}

void TASCAR::biquadf_t::set_highshelf(float f, float fs, float gain, float s)
{
  float A = powf(10.0f, gain / 40.0f);
  float omega0 = TASCAR_2PIf * f / fs;
  float alpha =
      0.5f * sinf(omega0) * sqrtf((A + 1.0f / A) * (1.0f / s - 1.0f) + 2.0f);
  float a0 = (A + 1.0f) - (A - 1.0f) * cosf(omega0) + 2.0f * sqrtf(A) * alpha;
  float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosf(omega0));
  float a2 = (A + 1.0f) - (A - 1.0f) * cosf(omega0) - 2.0f * sqrtf(A) * alpha;
  float b0 =
      A * ((A + 1.0f) + (A - 1.0f) * cosf(omega0) + 2.0f * sqrtf(A) * alpha);
  float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosf(omega0));
  float b2 =
      A * ((A + 1.0f) + (A - 1.0f) * cosf(omega0) - 2.0f * sqrtf(A) * alpha);
  set_coefficients(a1 / a0, a2 / a0, b0 / a0, b1 / a0, b2 / a0);
}

void TASCAR::biquadf_t::set_lowshelf(float f, float fs, float gain, float s)
{
  float A = powf(10.0f, gain / 40.0f);
  float omega0 = TASCAR_2PIf * f / fs;
  float alpha =
      0.5f * sinf(omega0) * sqrtf((A + 1.0f / A) * (1.0f / s - 1.0f) + 2.0f);
  float a0 = (A + 1.0f) + (A - 1.0f) * cosf(omega0) + 2.0f * sqrtf(A) * alpha;
  float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosf(omega0));
  float a2 = (A + 1.0f) + (A - 1.0f) * cosf(omega0) - 2.0f * sqrtf(A) * alpha;
  float b0 =
      A * ((A + 1.0f) - (A - 1.0f) * cosf(omega0) + 2.0f * sqrtf(A) * alpha);
  float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosf(omega0));
  float b2 =
      A * ((A + 1.0f) - (A - 1.0f) * cosf(omega0) - 2.0f * sqrtf(A) * alpha);
  set_coefficients(a1 / a0, a2 / a0, b0 / a0, b1 / a0, b2 / a0);
}

TASCAR::aweighting_t::aweighting_t(double fs)
{
  b1.set_analog_poles(7.39705e9, -76655.0, -76655.0, fs);
  b2.set_analog(sqrt(0.5), 0.0, 0.0, -676.7, -4636.0, fs);
  b3.set_analog(1.0, 0.0, 0.0, -129.4, -129.4, fs);
}

TASCAR::aweightingf_t::aweightingf_t(float fs)
{
  b1.set_analog_poles(7.39705e9f, -76655.0f, -76655.0f, fs);
  b2.set_analog(sqrtf(0.5f), 0.0f, 0.0f, -676.7f, -4636.0f, fs);
  b3.set_analog(1.0f, 0.0f, 0.0f, -129.4f, -129.4f, fs);
}

void TASCAR::multiband_pareq_t::set_fgq(const std::vector<float>& f,
                                        const std::vector<float>& g,
                                        const std::vector<float>& q, float fs)
{
  if(f.size() < 1)
    throw TASCAR::ErrMsg("At least one frequency sample needed");
  if(g.size() != f.size())
    throw TASCAR::ErrMsg(
        "Gain vector needs same number of entries as frequency vector");
  if(q.size() != f.size())
    throw TASCAR::ErrMsg(
        "Gain vector needs same number of entries as q-factor vector");
  resize(f.size());
  g0 = 1.0f;
  for(size_t k = 0; k < f.size(); ++k)
    flt[k].set_pareq(f[k], fs, g[k], q[k]);
}

void TASCAR::multiband_pareq_t::optimpar2fltsettings(
    const std::vector<float>& par, float fs, bool dump)
{
  if(par.size() != 3 * flt.size() + 1)
    throw TASCAR::ErrMsg("Invalid size of parameter space");
  g0 = powf(10.0f, 0.05f * par[0]);
  if(dump)
    std::cout << "  g0 = " << par[0] << " dB\n";
  flt_f.resize(flt.size());
  flt_g.resize(flt.size());
  flt_q.resize(flt.size());
  for(size_t k = 0; k < flt.size(); ++k) {
    float f =
        (atanf(par[3 * k + 1]) / TASCAR_PIf + 0.5f) * (fmax - fmin) + fmin;
    float g = par[3 * k + 2];
    float q = optim_maxq * (atanf(par[3 * k + 3]) / TASCAR_PIf + 0.5f);
    flt[k].set_pareq(f, fs, g, q);
    flt_f[k] = f;
    flt_g[k] = g;
    flt_q[k] = q;
    if(dump)
      std::cout << "  " << f << " Hz: g=" << g << " dB q=" << q << std::endl;
  }
}

static float mbeqerrfun(const std::vector<float>& x, void* data)
{
  return ((TASCAR::multiband_pareq_t*)data)->optim_error_fun(x);
}

float TASCAR::multiband_pareq_t::optim_error_fun(const std::vector<float>& par)
{
  optimpar2fltsettings(par, optim_fs);
  dbresponse(optim_gmeas, optim_f, optim_fs);
  float err = 0.0f;
  for(size_t k = 0; k < optim_g.size(); ++k) {
    float lerr = optim_g[k] - optim_gmeas[k];
    lerr *= lerr;
    err += lerr;
  }
  err /= (float)optim_g.size();
  // add |log10(g0)| to avoid excessive gains resulting in instable
  // filters:
  err += fabsf(log10(g0));
  return err;
}

std::vector<float> TASCAR::multiband_pareq_t::optim_response(
    size_t numflt, float maxq, const std::vector<float>& vF,
    const std::vector<float>& vG, float fs, size_t numiter,
    bool use_nelder_mead)
{
  if(numflt < 1)
    throw TASCAR::ErrMsg(
        "At least one filter is needed for optimization of filter fresponse");
  resize(numflt);
  if(vF.size() != vG.size())
    throw TASCAR::ErrMsg(
        "Frequency vector needs same number of elements as gain vector "
        "(optimization of parametric equalizer)\nvF.size() = " +
        std::to_string(vF.size()) +
        "\nvG.size() = " + std::to_string(vG.size()) + "\n");
  if(vF.size() < 3 * flt.size() + 1)
    throw TASCAR::ErrMsg("Not enough samples to optimize " +
                         std::to_string(flt.size()) + " filters. At least " +
                         std::to_string(3 * flt.size() + 1) +
                         " samples are required.");
  float f0 = 0.0f;
  fmin = fs;
  fmax = 0.0f;
  for(auto f : vF) {
    if(f <= 0.0f)
      throw TASCAR::ErrMsg(
          "Frequency vector contains negative or zero frequencies");
    if(f >= 0.5f * fs)
      throw TASCAR::ErrMsg("Frequency vector contains frequencies at or above "
                           "Nyquist frequency");
    if(f <= f0)
      throw TASCAR::ErrMsg("Frequency vector contains non-monotonic entries");
    f0 = f;
    fmin = std::min(f, fmin);
    fmax = std::max(f, fmax);
  }
  // fmin *= 0.5f;
  // fmax *= 2.0f;
  optim_fs = fs;
  optim_maxq = maxq;
  optim_f = vF;
  optim_g = vG;
  // find frequencies of min and max gain:
  float f_gmin = fmin;
  float f_gmax = fmin;
  float gmin = vG[0];
  float gmax = vG[0];
  for(size_t k = 0; k < vF.size(); ++k) {
    float g = vG[k];
    float f = vF[k];
    if(g > gmax) {
      gmax = g;
      f_gmax = f;
    }
    if(g < gmin) {
      gmin = g;
      f_gmin = f;
    }
  }
  std::vector<float> par;
  std::vector<float> step(3 * flt.size() + 1, 0.1f);
  par.resize(3 * flt.size() + 1);
  for(size_t k = 0; k < flt.size(); ++k) {
    float grel = 0.0f;
    float frel = 1.0f;
    if(k > 1)
      frel = 2.0f * fmin *
             powf(0.25f * fmax / fmin,
                  (float)(k - 2) /
                      ((float)std::max((size_t)2u, (flt.size() - 2)) - 1.0f));
    if(k == 0) {
      frel = f_gmin;
      grel = gmin;
    }
    if(k == 1) {
      frel = f_gmax;
      grel = gmax;
    }
    par[3 * k + 1] = tanf(TASCAR_PIf * ((frel - fmin) / (fmax - fmin) - 0.5f));
    par[3 * k + 2] = grel;
    par[3 * k + 3] = 0.5f;
  }
  optimpar2fltsettings(par, fs);
  if(use_nelder_mead) {
    TASCAR::nelmin(par, mbeqerrfun, par, 0.1f, step, 2, numiter, this);
  } else {
    float err = 10000000.0f;
    float eps = 1.0f;
    for(size_t k = 0; k < numiter; ++k) {
      float nerr = downhill_iterate(eps, par, mbeqerrfun, this, step);
      if(nerr > err)
        eps *= 0.5f;
      if(fabsf(nerr / err - 1.0f) < 1e-7f)
        k = numiter;
      err = nerr;
      if(err < 0.01f)
        k = numiter;
    }
  }
  // display final filter settings for analysis:
  optimpar2fltsettings(par, fs, true);
  return dbresponse(vF, fs);
}

std::vector<float>
TASCAR::multiband_pareq_t::dbresponse(const std::vector<float>& vF,
                                      float fs) const
{
  std::vector<float> og;
  dbresponse(og, vF, fs);
  return og;
}

void TASCAR::multiband_pareq_t::dbresponse(std::vector<float>& og,
                                           const std::vector<float>& vF,
                                           float fs) const
{
  og.resize(0);
  for(auto f : vF)
    og.push_back(20.0f * log10f(std::abs(response(f * TASCAR_2PIf / fs))));
}

std::string TASCAR::multiband_pareq_t::to_string() const
{
  std::string retv;
  retv += "g0=" + TASCAR::to_string(g0) + ";\nf=[" + TASCAR::to_string(flt_f) +
          "];\ng=[" + TASCAR::to_string(flt_g) + "];\nq=[" +
          TASCAR::to_string(flt_q) + "];\n";
  return retv;
}

std::vector<float> TASCAR::rflt2alpha(float reflectivity, float damping,
                                      float fs, const std::vector<float>& freq)
{
  std::vector<float> retv;
  reflectivity = std::max(std::min(reflectivity, 1.0f), EPSf);
  damping = std::max(std::min(damping, 1.0f - EPSf), -1.0f + EPSf);
  for(auto f : freq) {
    auto cw = std::exp(-i_f * TASCAR_2PIf * f / fs);
    auto H = std::abs(reflectivity * (1.0f - damping) / (1.0f - damping * cw));
    auto alpha = std::pow(1 - H, 2.0f);
    retv.push_back(alpha);
  }
  return retv;
}

struct optimpar_t {
  std::vector<float> alpha;
  std::vector<float> freq;
  float fs;
};

float absorptionerror(const std::vector<float>& vPar, void* data)
{
  optimpar_t* pOpt = (optimpar_t*)data;
  auto d = exp(-vPar[0] * vPar[0]);
  auto r = exp(-vPar[1] * vPar[1]);
  auto alphatest = TASCAR::rflt2alpha(r, d, pOpt->fs, pOpt->freq);
  float e = 0.0f;
  for(size_t k = 0; k < std::min(alphatest.size(), pOpt->alpha.size()); ++k) {
    auto a2 = (pOpt->alpha[k] - alphatest[k]);
    a2 *= a2;
    e += a2;
  }
  e /= (float)(pOpt->alpha.size());
  if((d > 1.0f) || (d < 0.0f))
    e = 1e6f;
  return e;
}

int TASCAR::alpha2rflt(float& reflectivity, float& damping,
                       const std::vector<float>& alpha,
                       const std::vector<float>& freq, float fs,
                       uint32_t numiter)
{
  if(alpha.empty())
    throw TASCAR::ErrMsg(
        "Invalid alpha coefficients in reflection filter (empty)");
  if(alpha.size() != freq.size())
    throw TASCAR::ErrMsg(
        "Different number of alpha coefficients and frequencies: alpha has " +
        std::to_string(alpha.size()) + " coefficients, freq has " +
        std::to_string(freq.size()) + " entries.");
  optimpar_t optpar = {alpha, freq, fs};
  std::vector<float> par = {0.5f, 0.5f};
  std::vector<float> step = {0.1f, 0.1f};
  int err = TASCAR::nelmin(par, absorptionerror, par, 0.02f, step, 2, numiter,
                           &optpar);
  damping = exp(-par[0] * par[0]);
  reflectivity = exp(-par[1] * par[1]);
  return err;
}

TASCAR::o1flt_lowpass_t::o1flt_lowpass_t(const std::vector<float>& tau,
                                         float fs, float startval)
    : TASCAR::o1_ar_filter_t((unsigned int)(tau.size()), fs)
{
  for(unsigned int k = 0; k < tau.size(); k++) {
    d[k] = startval;
    set_tau(k, tau[k]);
  }
}

/**
   \brief Constructor of low pass filter, sets sampling rate and time constants

   \param tau Vector of time constants
   \param fs Sampling rate
   \param startval Initial internal state value
*/
TASCAR::o1flt_lowpass_t::o1flt_lowpass_t(const std::vector<float>& tau,
                                         float fs,
                                         const std::vector<float>& startval)
    : TASCAR::o1_ar_filter_t((unsigned int)(tau.size()), fs)
{
  if(tau.size() != startval.size())
    throw ErrMsg(
        "o1flt_lowpass_t: Size of tau vector and initial state vector not equal"
        "(got " +
        std::to_string(tau.size()) + " and " + std::to_string(startval.size()) +
        ")");
  for(unsigned int k = 0; k < tau.size(); k++) {
    set_tau(k, tau[k]);
  }
  std::copy(std::begin(startval), std::end(startval), d);
}

void TASCAR::o1flt_lowpass_t::set_tau(unsigned int k, float tau)
{
  set_tau_attack(k, tau);
  set_tau_release(k, tau);
}

void TASCAR::o1flt_lowpass_t::set_tau(float tau)
{
  for(unsigned int k = 0; k < n; k++)
    set_tau(k, tau);
}

template <class elem_type>
std::vector<elem_type> dupvec_chk(std::vector<elem_type> vec, unsigned n)
{
  if(vec.size() == 1)
    vec.resize(n, vec[vec.size() - 1]);
  if(vec.size() != n)
    throw TASCAR::ErrMsg("Invalid vector length (expected 1 or " +
                         std::to_string(n) + ", got " +
                         std::to_string(vec.size()) + ").");
  return vec;
}

TASCAR::o1_ar_filter_t::o1_ar_filter_t(unsigned int channels, float fs_,
                                       std::vector<float> tau_a,
                                       std::vector<float> tau_r)
    : TASCAR::wave_t(channels), c1_a(channels), c2_a(channels), c1_r(channels),
      c2_r(channels), fs(fs_)
{
  if(fs < 0)
    throw ErrMsg("Invalid (negative) sampling rate");
  tau_a = dupvec_chk(tau_a, channels);
  tau_r = dupvec_chk(tau_r, channels);
  for(unsigned int k = 0; k < channels; k++) {
    d[k] = 0.0f;
    set_tau_attack(k, tau_a[k]);
    set_tau_release(k, tau_r[k]);
  }
}

void o1_lp_coeffs(const float tau, const float fs, float& c1, float& c2)
{
  if((tau > 0.0f) && (fs > 0.0f))
    c1 = expf(-1.0f / (tau * fs));
  else
    c1 = 0;
  c2 = 1.0f - c1;
}

void TASCAR::o1_ar_filter_t::set_tau_attack(unsigned int ch, float tau)
{
  if(ch >= n)
    throw ErrMsg("The filter channel is out of range.");
  o1_lp_coeffs(tau, fs, c1_a[ch], c2_a[ch]);
}

void TASCAR::o1_ar_filter_t::set_tau_release(unsigned int ch, float tau)
{
  if(ch >= n)
    throw ErrMsg("The filter channel is out of range.");
  o1_lp_coeffs(tau, fs, c1_r[ch], c2_r[ch]);
}

TASCAR::o1_ar_filter_t::o1_ar_filter_t()
    : TASCAR::wave_t(1), c1_a(1), c2_a(1), c1_r(1), c2_r(1), fs(1)
{
  d[0] = 0.0f;
  set_tau_attack(0, 1);
  set_tau_release(0, 1);
}

TASCAR::o1_ar_filter_t::o1_ar_filter_t(const o1_ar_filter_t& src)
    : TASCAR::wave_t(src.n), c1_a(src.c1_a), c2_a(src.c2_a), c1_r(src.c1_r),
      c2_r(src.c2_r), fs(src.fs)
{
}

TASCAR::o1_ar_filter_t&
TASCAR::o1_ar_filter_t::operator=(const TASCAR::o1_ar_filter_t& src)
{
  resize(src.n);
  c1_a.resize(src.n);
  c2_a.resize(src.n);
  c1_r.resize(src.n);
  c2_r.resize(src.n);
  fs = src.fs;
  c1_a.copy(src.c1_a);
  c2_a.copy(src.c2_a);
  c1_r.copy(src.c1_r);
  c2_r.copy(src.c2_r);
  fs = src.fs;
  return *this;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
