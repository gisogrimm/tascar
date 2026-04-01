/**
 * @file   fft.h
 * @author Giso Grimm
 *
 * @brief  Wrapper class for FFTW
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef FFT_H
#define FFT_H

#include "spectrum.h"
#include <fftw3.h>

namespace TASCAR {

  /**
     \brief Wrapper class around real-to-complex and complex-to-real fftw
   */
  class fft_t {
  public:
    /**
       \brief Constructor
       \param fftlen FFT length
     */
    fft_t(uint32_t fftlen);
    /**
       \brief Copy constructor
    */
    fft_t(const fft_t& src);
    /**
       \brief Perform a waveform to spectrum (real-to-complex) transformation

       \param src Input waveform

       The result is stored in the public member fft_t::s.
     */
    void execute(const TASCAR::wave_t& src);
    /**
       \brief Perform a spectrum to waveform (complex-to-real) transformation

       \param src Input spectrum

       The result is stored in the public member fft_t::w.
     */
    void execute(const TASCAR::spec_t& src);
    /**
       \brief Perform a spectrum to waveform (complex-to-real) transformation

       The input is read from the public member fft_t::s.
       The result is stored in the public member fft::w.
     */
    void ifft();

    /**
       \brief Perform a waveform to spectrum (real-to-complex) transformation

       The input is read from the public member fft_t::w.
       The result is stored in the public member fft_t::s.
     */
    void fft();

    /**
       \brief Perform Hilbert transformation of a real signal

       \param src Input waveform

       The result is stored in the public member fft_t::w.
     */
    void hilbert(const TASCAR::wave_t& src);
    ~fft_t();
    TASCAR::wave_t w; ///< waveform container
    TASCAR::spec_t s; ///< spectrum container
  private:
    TASCAR::spec_t fullspec;
    float* wp;
    fftwf_complex* sp;
    fftwf_complex* fsp;
    fftwf_plan fftwp_w2s;
    fftwf_plan fftwp_s2w;
    fftwf_plan fftwp_s2s;
  };

  class minphase_t {
  public:
    /**
     * \param fftlen Length of the FFT (must be even for standard real FFTs)
     */
    minphase_t(uint32_t fftlen);
    ~minphase_t();
    /**
     * \brief Process a half-spectrum to generate its minimum phase equivalent.
     *
     * \param spec_in Input half-spectrum (DC to Nyquist).
     *                Size must match the configured FFT length.
     * \param spec_out Output half-spectrum (Minimum phase).
     *                 Can be the same as spec_in for in-place operation.
     */
    void process(const spec_t& spec_in, spec_t& spec_out);
    void operator()(TASCAR::spec_t& s);

  private:
    TASCAR::spec_t spec_copy; // copy of input to allow in-place transformation
    uint32_t fftlen_;         // Total FFT length
    uint32_t nbins_;          // Number of bins in half-spectrum (fftlen/2 + 1)
    // FFTW Plans
    fftwf_plan plan_c2r_; // Complex to Real (Inverse FFT)
    fftwf_plan
        plan_r2c_; // Real to Complex (Forward FFT) - used for Cepstrum check
    // Buffers
    float* time_buf_;  // Time domain buffer (size fftlen_)
    spec_t* freq_buf_; // Frequency domain buffer (size nbins_)
    // Helper to compute log magnitude safely
    inline float safe_log(float x) { return std::log(std::max(x, 1e-10f)); }
  };

  /**
     @brief Return sound levels in frequency bands.

     @param w Input waveform.
     @param cfmin Lowest center frequency in Hz.
     @param cfmax Highest center frequency in Hz.
     @param fs Sampling rate in Hz.
     @param bpo Nominal bands per octave.
     @param overlap Relative overlap of bands.
     @retval vF Final center frequencies in Hz.
     @retval vL Levels in dB SPL.
   */
  void get_bandlevels(const TASCAR::wave_t& w, float cfmin, float cfmax,
                      float fs, float bpo, float overlap,
                      std::vector<float>& vF, std::vector<float>& vL);

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
