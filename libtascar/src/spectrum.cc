/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "spectrum.h"
#include "tscconfig.h"
#include <algorithm>
#include <string.h>
#include "errorhandling.h"

TASCAR::spec_t::spec_t(uint32_t n)
    : n_(n), b(new std::complex<float>[std::max(1u, n_)])
{
  for(uint32_t k = 0; k < n_; ++k)
    b[k] = 0.0f;
}

TASCAR::spec_t::spec_t(const TASCAR::spec_t& src)
    : n_(src.n_), b(new std::complex<float>[std::max(1u, n_)])
{
  copy(src);
}

void TASCAR::spec_t::clear()
{
  for(uint32_t k = 0; k < n_; ++k)
    b[k] = 0.0f;
}

TASCAR::spec_t::~spec_t()
{
  delete[] b;
}

void TASCAR::spec_t::copy(const spec_t& src)
{
  memmove(b, src.b, std::min(n_, src.n_) * sizeof(std::complex<float>));
}

void TASCAR::spec_t::resize(uint32_t newsize)
{
  std::complex<float>* b_new(new std::complex<float>[std::max(1u, newsize)]);
  memmove(b_new, b, std::min(n_, newsize) * sizeof(std::complex<float>));
  for(uint32_t k = 0; k < std::min(n_, newsize); ++k)
    b_new[k] = b[k];
  for(uint32_t k = n_; k < newsize; ++k)
    b_new[k] = 0.0f;
  delete[] b;
  b = b_new;
  n_ = newsize;
}

void TASCAR::spec_t::operator/=(const spec_t& o)
{
  for(unsigned int k = 0; k < std::min(o.n_, n_); ++k) {
    if(std::abs(o.b[k]) > 0)
      b[k] /= o.b[k];
  }
}

void TASCAR::spec_t::operator*=(const spec_t& o)
{
  for(unsigned int k = 0; k < std::min(o.n_, n_); ++k)
    b[k] *= o.b[k];
}

void TASCAR::spec_t::operator+=(const spec_t& o)
{
  for(unsigned int k = 0; k < std::min(o.n_, n_); ++k)
    b[k] += o.b[k];
}

void TASCAR::spec_t::add_scaled(const spec_t& o, float gain)
{
  for(unsigned int k = 0; k < std::min(o.n_, n_); ++k)
    b[k] += o.b[k] * gain;
}

void TASCAR::spec_t::operator*=(const float& o)
{
  for(unsigned int k = 0; k < n_; ++k)
    b[k] *= o;
}

void TASCAR::spec_t::conj()
{
  for(uint32_t k = 0; k < n_; ++k)
    b[k] = std::conj(b[k]);
}

std::ostream& operator<<(std::ostream& out, const TASCAR::wave_t& p)
{
  // out << std::string("W(") << p.n << std::string("):");
  for(uint32_t k = 0; k < p.n; k++)
    out << std::string(" ") << p.d[k];
  return out;
}

std::ostream& operator<<(std::ostream& out, const TASCAR::spec_t& p)
{
  // out << std::string("S(") << p.n_ << std::string("):");
  for(uint32_t k = 0; k < p.n_; k++)
    out << std::string(" ") << p.b[k].real()
        << std::string((p.b[k].imag() >= 0.0) ? "+" : "") << p.b[k].imag()
        << "i";
  return out;
}

/**
 * Performs Cubic Spline Interpolation.
 *
 * @param xin  The input X coordinates (must be strictly increasing).
 * @param yin  The input Y values corresponding to xin.
 * @param xout The X coordinates for which we want to estimate Y values.
 * @return     A vector containing the interpolated Y values for xout.
 */
std::vector<float> cubic_spline_interpolate(const std::vector<float>& xin,
                                            const std::vector<float>& yin,
                                            const std::vector<float>& xout)
{
  // 1. Validation
  if(xin.size() != yin.size()) {
    throw std::invalid_argument("xin and yin must have the same size");
  }
  if(xin.size() < 2) {
    throw std::invalid_argument(
        "At least 2 points are required for interpolation");
  }
  const size_t n = xin.size();
  std::vector<float> yout(xout.size());
  // 2. Handle trivial case where input is just a straight line
  if(n == 2) {
    float slope = (yin[1] - yin[0]) / (xin[1] - xin[0]);
    for(size_t i = 0; i < xout.size(); ++i) {
      yout[i] = yin[0] + slope * (xout[i] - xin[0]);
    }
    return yout;
  }
  // 3. Calculate the second derivatives (sigma) of the interpolation function.
  //    We use the Thomas Algorithm (Tridiagonal Matrix Algorithm) to solve the
  //    system. System: A * sigma = B For Natural Splines, sigma[0] = 0 and
  //    sigma[n-1] = 0.
  std::vector<float> sigma(n, 0.0f);
  std::vector<float> mu(n - 1, 0.0f); // Lower diagonal
  std::vector<float> z(n - 1, 0.0f);  // Upper diagonal
  std::vector<float> h(n - 1, 0.0f);  // Interval widths
  std::vector<float> b(n - 1, 0.0f);  // RHS vector components
  // Precompute interval widths (h) and RHS components (b)
  for(size_t i = 0; i < n - 1; ++i) {
    h[i] = xin[i + 1] - xin[i];
    if(h[i] <= 0.0f) {
      throw std::invalid_argument("xin must be strictly increasing");
    }
  }
  for(size_t i = 1; i < n - 1; ++i) {
    b[i] = 3.0f *
           ((yin[i + 1] - yin[i]) / h[i] - (yin[i] - yin[i - 1]) / h[i - 1]);
  }
  // Decomposition and Forward substitution
  // We solve for internal points 1 to n-2.
  // sigma[0] and sigma[n-1] are 0 for natural splines.
  std::vector<float> l(n, 1.0f); // Main diagonal
  for(size_t i = 1; i < n - 1; ++i) {
    mu[i] = h[i - 1] / l[i - 1];
    l[i] = 2.0f * (h[i - 1] + h[i]) - mu[i] * h[i - 1];
    z[i] = b[i] - mu[i] * z[i - 1];
  }
  // Back substitution
  for(int i = static_cast<int>(n) - 2; i >= 1; --i) {
    sigma[i] = (z[i] - h[i] * sigma[i + 1]) / l[i];
  }
  // 4. Interpolate for each point in xout
  for(size_t k = 0; k < xout.size(); ++k) {
    float x = xout[k];
    // Find the interval [xin[i], xin[i+1]] containing x
    // We use a simple linear search. For large xin, binary search is preferred.
    size_t i = 0;
    if(x >= xin[n - 1]) {
      i = n - 2; // Clamp to last interval
    } else if(x <= xin[0]) {
      i = 0; // Clamp to first interval
    } else {
      while(x > xin[i + 1]) {
        i++;
        if(i >= n - 1)
          break; // Safety break
      }
    }
    // Coefficients for the cubic polynomial on this interval
    float dx = xin[i + 1] - xin[i];
    // float t = (x - xin[i]) / dx;
    // float t1 = 1.0f - t;
    // Hermite basis functions formulation
    // y = h00(t)*yi + h10(t)*hi*sigma[i] + h01(t)*yi+1 + h11(t)*hi+1*sigma[i+1]
    // Note: The standard derivation often uses sigma as second derivatives.
    // The formula below is derived from the standard cubic spline definition:
    // S(x) = A + B*(x-xi) + C*(x-xi)^2 + D*(x-xi)^3
    float dy = yin[i + 1] - yin[i];
    float c0 = yin[i];
    float c1 = dy / dx - dx * (2.0f * sigma[i] + sigma[i + 1]) / 3.0f;
    float c2 = sigma[i];
    float c3 = (sigma[i + 1] - sigma[i]) / (3.0f * dx);
    float diff = x - xin[i];
    yout[k] = c0 + c1 * diff + c2 * diff * diff + c3 * diff * diff * diff;
  }
  return yout;
}

TASCAR::spec_t
TASCAR::sampled_spec_to_smooth_spec(float f_sample, uint32_t n_bins,
                                    const std::vector<float>& vfreq,
                                    const std::vector<float>& vgaindb)
{
  if(vfreq.empty())
    throw TASCAR::ErrMsg("Empty frequency vector.");
  if(vfreq.size() != vgaindb.size())
    throw TASCAR::ErrMsg("Different size of frequencies and gains. vfreq has " +
                         std::to_string((vfreq.size())) + ", vgaindb has " +
                         std::to_string(vgaindb.size()) + ".");
  TASCAR::spec_t spectrum(n_bins-1);
  std::vector<float> vfreqlog;
  //vfreqlog.push_back(log2f(EPSf));
  vfreqlog.push_back(log2f(vfreq[0]) - 4.0f);
  vfreqlog.push_back(log2f(vfreq[0]) - 3.0f);
  vfreqlog.push_back(log2f(vfreq[0]) - 2.0f);
  vfreqlog.push_back(log2f(vfreq[0]) - 1.0f);
  for(auto f : vfreq)
    vfreqlog.push_back(log2f(f));
  vfreqlog.push_back(log2f(vfreq[vfreq.size() - 1]) + 1.0f);
  vfreqlog.push_back(log2f(vfreq[vfreq.size() - 1]) + 2.0f);
  vfreqlog.push_back(log2f(vfreq[vfreq.size() - 1]) + 3.0f);
  std::vector<float> vgain_extrap;
  vgain_extrap.push_back(vgaindb[0]);
  vgain_extrap.push_back(vgaindb[0]);
  vgain_extrap.push_back(vgaindb[0]);
  vgain_extrap.push_back(vgaindb[0]);
  for(auto g : vgaindb)
    vgain_extrap.push_back(g);
  vgain_extrap.push_back(vgaindb[vgaindb.size() - 1]);
  vgain_extrap.push_back(vgaindb[vgaindb.size() - 1]);
  vgain_extrap.push_back(vgaindb[vgaindb.size() - 1]);
  std::vector<float> vfreq_out(n_bins);
  for(uint32_t k = 1; k < spectrum.n_; ++k)
    vfreq_out[k-1] =
        log2f(std::max(EPSf, (float)k / (float)n_bins * 0.5f * f_sample));
  auto spec_lin = cubic_spline_interpolate(vfreqlog, vgain_extrap, vfreq_out);
  for(uint32_t k = 1; k < spectrum.n_; ++k)
    spectrum.b[k] = TASCAR::db2lin(spec_lin[k]);
  spectrum.b[0] = spectrum.b[1];
  return spectrum;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
