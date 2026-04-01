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

#include "fft.h"
#include "errorhandling.h"
#include <cstring>

const std::complex<float> i(0.0, 1.0);

void TASCAR::fft_t::fft()
{
  fftwf_execute(fftwp_w2s);
}

void TASCAR::fft_t::ifft()
{
  fftwf_execute(fftwp_s2w);
  w *= 1.0f / (float)(w.size());
}

void TASCAR::fft_t::execute(const wave_t& src)
{
  w.copy(src);
  fft();
}

void TASCAR::fft_t::execute(const spec_t& src)
{
  s.copy(src);
  ifft();
}

TASCAR::fft_t::fft_t(uint32_t fftlen)
    : w(fftlen), s(fftlen / 2 + 1), fullspec(fftlen), wp(w.d),
      sp((fftwf_complex*)(s.b)), fsp((fftwf_complex*)(fullspec.b)),
      fftwp_w2s(fftwf_plan_dft_r2c_1d(w.n, wp, sp, FFTW_ESTIMATE)),
      fftwp_s2w(fftwf_plan_dft_c2r_1d(w.n, sp, wp, FFTW_ESTIMATE)),
      fftwp_s2s(fftwf_plan_dft_1d(w.n, fsp, fsp, FFTW_BACKWARD, FFTW_ESTIMATE))
{
}

TASCAR::fft_t::fft_t(const fft_t& src)
    : w(src.w.n), s(src.s.n_), fullspec(src.fullspec.n_), wp(w.d),
      sp((fftwf_complex*)(s.b)), fsp((fftwf_complex*)(fullspec.b)),
      fftwp_w2s(fftwf_plan_dft_r2c_1d(w.n, wp, sp, FFTW_ESTIMATE)),
      fftwp_s2w(fftwf_plan_dft_c2r_1d(w.n, sp, wp, FFTW_ESTIMATE)),
      fftwp_s2s(fftwf_plan_dft_1d(w.n, fsp, fsp, FFTW_BACKWARD, FFTW_ESTIMATE))
{
}

void TASCAR::fft_t::hilbert(const TASCAR::wave_t& src)
{
  float sc(2.0f / (float)(fullspec.n_));
  execute(src);
  fullspec.clear();
  for(uint32_t k = 0; k < s.n_; ++k)
    fullspec.b[k] = s.b[k];
  fftwf_execute(fftwp_s2s);
  for(uint32_t k = 0; k < w.n; ++k)
    w.d[k] = sc * fullspec.b[k].imag();
}

TASCAR::fft_t::~fft_t()
{
  fftwf_destroy_plan(fftwp_w2s);
  fftwf_destroy_plan(fftwp_s2w);
  fftwf_destroy_plan(fftwp_s2s);
}

void TASCAR::get_bandlevels(const TASCAR::wave_t& w, float cfmin, float cfmax,
                            float fs, float bpo, float overlap,
                            std::vector<float>& vF, std::vector<float>& vL)
{
  size_t numbands = (size_t)(floor(bpo * log2f(cfmax / cfmin))) + 1;
  bpo = (float)(numbands - 1) / log2f(cfmax / cfmin);
  vF.resize(0);
  vL.resize(0);
  for(size_t k = 0; k < numbands; ++k) {
    float f = cfmin * powf(2.0f, (float)k / bpo);
    vF.push_back(f);
  }
  TASCAR::fft_t fft(w.n);
  fft.execute(w);
  for(auto f : vF) {
    float f1e = f * powf(2.0f, -0.5f / bpo);
    float f2e = f * powf(2.0f, 0.5f / bpo);
    float f1 = f * powf(2.0f, -(0.5f + overlap) / bpo);
    float f2 = f * powf(2.0f, (0.5f + overlap) / bpo);
    uint32_t idx1e = std::min((uint32_t)((float)w.n * f1e / fs), fft.s.n_);
    uint32_t idx2e = std::min((uint32_t)((float)w.n * f2e / fs), fft.s.n_);
    uint32_t idx1 = std::min((uint32_t)((float)w.n * f1 / fs), fft.s.n_);
    uint32_t idx2 = std::min((uint32_t)((float)w.n * f2 / fs), fft.s.n_);
    float l = 0.0f;
    // lower overlap area from f1 to f1e:
    for(uint32_t k = idx1; k < idx1e; ++k) {
      float w = (0.5f - 0.5f * cosf((float)(k - idx1) / (float)(idx1e - idx1) *
                                    TASCAR_PIf));
      l += std::abs(fft.s[k]) * std::abs(fft.s[k]) * w * w;
    }
    // central area from f1e to f2e:
    for(uint32_t k = idx1e; k < idx2e; ++k) {
      l += std::abs(fft.s[k]) * std::abs(fft.s[k]);
    }
    // upper overlap area from f2e to f2:
    for(uint32_t k = idx2e; k < idx2; ++k) {
      float w = (0.5f + 0.5f * cosf((float)(k - idx2e) / (float)(idx2 - idx2e) *
                                    TASCAR_PIf));
      l += std::abs(fft.s[k]) * std::abs(fft.s[k]) * w * w;
    }
    // scale to Pa^2, factor 2 due to positive frequencies only:
    l *= 5e4f * 5e4f * 2.0f;
    l /= (float)w.n * (float)w.n;
    vL.push_back(10.0f * log10f(l));
  }
}

TASCAR::minphase_t::minphase_t(uint32_t fftlen)
    : spec_copy(fftlen / 2 + 1), fftlen_(fftlen), time_buf_(NULL),
      freq_buf_(NULL)
{
  // Ensure even length for standard r2c/c2r compatibility
  if(fftlen_ % 2 != 0) {
    fftlen_++; // Or handle error, here we just adjust to next even
    std::string warning = "FFT length adjusted to " + std::to_string(fftlen_);
    DEBUG(warning);
  }
  nbins_ = fftlen_ / 2 + 1;
  // Allocate Time Domain Buffer
  time_buf_ = (float*)fftwf_malloc(sizeof(float) * fftlen_);
  // Allocate Frequency Domain Buffer (using spec_t wrapper)
  freq_buf_ = new spec_t(nbins_);

  // Create FFTW Plans
  // 1. Inverse FFT: Complex Half-Spectrum -> Real Time
  // We use the buffer inside freq_buf_ as input
  plan_c2r_ = fftwf_plan_dft_c2r_1d(
      fftlen_, reinterpret_cast<fftwf_complex*>(freq_buf_->b), time_buf_,
      FFTW_MEASURE);
  // 2. Forward FFT: Real Time -> Complex Half-Spectrum
  // Used to transform the windowed cepstrum back to frequency domain
  plan_r2c_ = fftwf_plan_dft_r2c_1d(
      fftlen_, time_buf_, reinterpret_cast<fftwf_complex*>(freq_buf_->b),
      FFTW_MEASURE);
  // Initialize buffers to zero
  memset(time_buf_, 0, sizeof(float) * fftlen_);
  freq_buf_->clear();
}

TASCAR::minphase_t::~minphase_t()
{
  if(plan_c2r_)
    fftwf_destroy_plan(plan_c2r_);
  if(plan_r2c_)
    fftwf_destroy_plan(plan_r2c_);
  if(time_buf_)
    fftwf_free(time_buf_);
  if(freq_buf_)
    delete freq_buf_;
}

void TASCAR::minphase_t::operator()(TASCAR::spec_t& s)
{
  spec_copy.copy(s);
  process(spec_copy, s);
}

void TASCAR::minphase_t::process(const spec_t& spec_in, spec_t& spec_out)
{
  // Check sizes
  if(spec_in.size() != nbins_ || spec_out.size() != nbins_) {
    // Handle error: sizes do not match
    return;
  }
  // ---------------------------------------------------------
  // Step 1: Compute Real Cepstrum
  // Cepstrum = IFFT( log( |Spectrum| ) )
  // ---------------------------------------------------------

  // 1a. Calculate Log-Magnitude of the input spectrum
  for(uint32_t k = 0; k < nbins_; ++k) {
    // Get magnitude
    float mag = std::abs(spec_in[k]);
    // Store log magnitude in the real part of our working buffer
    // We use the freq_buf_ to prepare for the Inverse FFT
    (*freq_buf_)[k] = std::complex<float>(safe_log(mag), 0.0f);
  }
  // 1b. Perform Inverse FFT to get Cepstrum (Real valued)
  fftwf_execute(plan_c2r_);
  // Note: FFTW c2r does not normalize, so the result is scaled by fftlen_
  // We handle scaling later or implicitly.
  // The Cepstrum is now in time_buf_.

  // ---------------------------------------------------------
  // Step 2: Apply Minimum Phase Window (Causal Window)
  // ---------------------------------------------------------

  // The Real Cepstrum is symmetric. To get minimum phase, we need the causal
  // part. c_min[n] = c[n] + c[n] * u[n-1] (where u is unit step) Effectively:
  //   c_min[0] = c[0]
  //   c_min[k] = 2 * c[k] for k = 1 ... N/2 - 1
  //   c_min[N/2] = c[N/2] (Nyquist)
  //   c_min[k] = 0 for k > N/2

  // Scale by 1/fftlen_ to correct for the unnormalized IFFT
  float scale = 1.0f / (float)fftlen_;

  // DC component (index 0)
  time_buf_[0] *= scale;

  // Positive quefrencies (indices 1 to N/2 - 1)
  for(uint32_t k = 1; k < nbins_ - 1; ++k) {
    time_buf_[k] *= 2.0f * scale;
  }

  // Nyquist component (index N/2)
  time_buf_[nbins_ - 1] *= scale;

  // Negative quefrencies (indices N/2 + 1 to N - 1) -> Zero out
  // Note: time_buf_ size is fftlen_.
  // Indices nbins_ to fftlen_-1 correspond to negative times.
  memset(time_buf_ + nbins_, 0, sizeof(float) * (fftlen_ - nbins_));

  // ---------------------------------------------------------
  // Step 3: Transform back to Frequency Domain
  // ---------------------------------------------------------

  fftwf_execute(plan_r2c_);

  // ---------------------------------------------------------
  // Step 4: Exponentiate to get Minimum Phase Spectrum
  // ---------------------------------------------------------

  // The result in freq_buf_ is the complex cepstrum in frequency domain.
  // We need exp(real + j*imag) = exp(real) * (cos(imag) + j*sin(imag))

  for(uint32_t k = 0; k < nbins_; ++k) {
    std::complex<float> c = (*freq_buf_)[k];

    // Compute complex exponential
    // Using std::exp for complex numbers handles the Euler formula
    std::complex<float> min_phase_spec = std::exp(c);

    // Store result
    spec_out[k] = min_phase_spec;
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
