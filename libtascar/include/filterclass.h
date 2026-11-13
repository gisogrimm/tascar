/**
 * @file   filterclass.h
 * @author Giso Grimm
 *
 * @brief  Definition of filter classes
 *
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

#ifndef FILTERCLASS_H
#define FILTERCLASS_H

#include "audiochunks.h"
#include "errorhandling.h"
#include <complex>
#include <vector>

namespace TASCAR {

  class filter_t {
  public:
    /**
        \brief Constructor
        \param lena Number of recursive coefficients
        \param lenb Number of non-recursive coefficients
    */
    filter_t(unsigned int lena,  // length A
             unsigned int lenb); // length B
    /**
        \brief Constructor with initialization of coefficients.
        \param vA Recursive coefficients.
        \param vB Non-recursive coefficients.
    */
    filter_t(const std::vector<double>& vA, const std::vector<double>& vB);

    /// copy constructor
    filter_t(const TASCAR::filter_t& src);

    ~filter_t();
    /** \brief Filter all channels in a waveform structure.
        \param out Output signal
        \param in Input signal
    */
    void filter(TASCAR::wave_t* out, const TASCAR::wave_t* in);
    /** \brief Filter parts of a waveform structure
        \param dest Output signal.
        \param src Input signal.
        \param dframes Number of frames to be filtered.
        \param frame_dist Index distance between frames of one channel
    */
    void filter(float* dest, const float* src, unsigned int dframes,
                unsigned int frame_dist);
    /**
       \brief Filter one sample
       \param x Input value
       \return filtered value
    */
    double filter(float x);
    /** \brief Return length of recursive coefficients */
    unsigned int get_len_A() const { return len_A; };
    /** \brief Return length of non-recursive coefficients */
    unsigned int get_len_B() const { return len_B; };
    /** \brief Pointer to recursive coefficients */
    double* A;
    /** \brief Pointer to non-recursive coefficients */
    double* B;

  private:
    unsigned int len_A;
    unsigned int len_B;
    unsigned int len;
    double* state;
  };

  class fsplit_t : protected TASCAR::wave_t {
  public:
    enum shape_t { none, notch, sine, tria, triald };
    fsplit_t(uint32_t maxdelay, shape_t shape, uint32_t tau);
    inline void push(float v)
    {
      // first decrement all position pointers:
      for(std::vector<float*>::iterator it = vd.begin(); it != vd.end(); ++it) {
        if(*it == d)
          *it = d + n;
        --(*it);
      }
      // store value to first element:
      *(vd[0]) = v;
    };
    inline void get(float& v1, float& v2)
    {
      v1 = v2 = 0.0f;
      // coefficient iterators, asume same size as position pointer vector:
      std::vector<float>::iterator w1it(w1.begin());
      std::vector<float>::iterator w2it(w2.begin());
      // sum across coefficients:
      for(std::vector<float*>::iterator it = vd.begin(); it != vd.end(); ++it) {
        v1 += *w1it * *(*it);
        v2 += *w2it * *(*it);
        ++w1it;
        ++w2it;
      }
    };

  private:
    std::vector<float*> vd;
    std::vector<float> w1;
    std::vector<float> w2;
  };

  class resonance_filter_t {
  public:
    resonance_filter_t();
    void set_fq(double fresnorm, double q);
    inline float filter(float inval)
    {
      inval = (float)(a1 * (double)inval + b1 * statey1 + b2 * statey2);
      make_friendly_number_limited(inval);
      statey2 = statey1;
      statey1 = (double)inval;
      return inval;
    };
    inline float filter_unscaled(float inval)
    {
      inval = (float)((double)inval + b1 * statey1 + b2 * statey2);
      make_friendly_number_limited(inval);
      statey2 = statey1;
      statey1 = (double)inval;
      return inval;
    };

  private:
    double a1;
    double b1;
    double b2;
    double statey1;
    double statey2;
  };

  class biquad_t {
  public:
    biquad_t(double a1, double a2, double b0, double b1, double b2)
        : a1_(a1), a2_(a2), b0_(b0), b1_(b1), b2_(b2), z1(0.0), z2(0.0) {};
    biquad_t() : a1_(0), a2_(0), b0_(1), b1_(0), b2_(0), z1(0.0), z2(0.0) {};
    void set_gzp(double g, double zero_r, double zero_phi, double pole_r,
                 double pole_phi);
    /**
       @brief Set coefficients from s-domain transfer function by
       means of a bilinear transformation.

       @param g Gain of the s-domain transfer function
       @param z1 First zero of the s-domain transfer function
       @param z2 Second zero of the s-domain transfer function
       @param p1 First pole of the s-domain transfer function
       @param p2 Second pole of the s-domain transfer function
       @param fs Sampling rate
     */
    void set_analog(double g, double z1, double z2, double p1, double p2,
                    double fs);
    void set_analog_poles(double g, double p1, double p2, double fs);
    void set_allpass(double r, double phi);
    /**
       @param Set coefficiens to second order butterworth filter
     */
    void set_butterworth(double f, double fs, bool highpass = false);
    void set_coefficients(double a1, double a2, double b0, double b1, double b2)
    {
      a1_ = a1;
      a2_ = a2;
      b0_ = b0;
      b1_ = b1;
      b2_ = b2;
    };
    void set_pareq(double f, double fs, double gain, double q);
    void set_highshelf(double f, double fs, double gain, double s);
    void set_lowshelf(double f, double fs, double gain, double s);
    inline double filter(double in)
    {
      double out = z1 + b0_ * in;
      z1 = z2 + b1_ * in - a1_ * out;
      z2 = b2_ * in - a2_ * out;
      return out;
    };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = (float)(filter((double)(*v)));
    };
    std::complex<double> response(double phi) const;
    std::complex<double> response_a(double phi) const;
    std::complex<double> response_b(double phi) const;
    double get_a1() const { return a1_; };
    double get_a2() const { return a2_; };
    double get_b0() const { return b0_; };
    double get_b1() const { return b1_; };
    double get_b2() const { return b2_; };
    void clear()
    {
      z1 = 0.0;
      z2 = 0.0;
    };

  protected:
    double a1_, a2_, b0_, b1_, b2_;
    double z1, z2;
  };

  class biquadf_t {
  public:
    biquadf_t(float a1, float a2, float b0, float b1, float b2)
        : a1_(a1), a2_(a2), b0_(b0), b1_(b1), b2_(b2), z1(0.0), z2(0.0) {};
    biquadf_t() : a1_(0), a2_(0), b0_(1), b1_(0), b2_(0), z1(0.0), z2(0.0) {};
    void set_gzp(float g, float zero_r, float zero_phi, float pole_r,
                 float pole_phi);
    void set_analog(float g, float z1, float z2, float p1, float p2, float fs);
    void set_analog_poles(float g, float p1, float p2, float fs);
    void set_allpass(float r, float phi);
    void set_coefficients(float a1, float a2, float b0, float b1, float b2)
    {
      a1_ = a1;
      a2_ = a2;
      b0_ = b0;
      b1_ = b1;
      b2_ = b2;
    };
    /**
       @param Set coefficiens to second order butterworth filter
     */
    void set_butterworth(float f, float fs, bool highpass = false);
    void set_pareq(float f, float fs, float gain, float q);
    void set_highshelf(float f, float fs, float gain, float s);
    void set_lowshelf(float f, float fs, float gain, float s);
    inline float filter(float in)
    {
      float out = z1 + b0_ * in;
      z1 = z2 + b1_ * in - a1_ * out;
      z2 = b2_ * in - a2_ * out;
      return out;
    };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = filter(*v);
    };
    std::complex<float> response(float phi) const;
    std::complex<float> response_a(float phi) const;
    std::complex<float> response_b(float phi) const;
    float get_a1() const { return a1_; };
    float get_a2() const { return a2_; };
    float get_b0() const { return b0_; };
    float get_b1() const { return b1_; };
    float get_b2() const { return b2_; };
    void clear()
    {
      z1 = 0.0;
      z2 = 0.0;
    };

  protected:
    float a1_, a2_, b0_, b1_, b2_;

  public:
    float z1, z2;
  };

  class bandpass_t {
  public:
    bandpass_t();
    bandpass_t(double f1, double f2, double fs);
    void set_range(double f1, double f2);
    void set_range(double f1, double f2, double fs);
    inline double filter(double in) { return b2.filter(b1.filter(in)); };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = (float)(filter((double)(*v)));
    };
    void clear()
    {
      b1.clear();
      b2.clear();
    };

  private:
    biquad_t b1;
    biquad_t b2;
    double fs_;
  };

  class bandpassf_t {
  public:
    bandpassf_t(float f1, float f2, float fs);
    void set_range(float f1, float f2);
    inline float filter(float in) { return b2.filter(b1.filter(in)); };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = filter(*v);
    };
    void clear()
    {
      b1.clear();
      b2.clear();
    };

  private:
    biquadf_t b1;
    biquadf_t b2;
    float fs_;
  };

  class aweighting_t {
  public:
    aweighting_t(double fs);
    inline double filter(double in)
    {
      return b3.filter(b2.filter(b1.filter(in)));
    };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = (float)(filter((double)(*v)));
    };
    biquad_t b1;
    biquad_t b2;
    biquad_t b3;
  };

  class aweightingf_t {
  public:
    aweightingf_t(float fs);
    inline float filter(float in)
    {
      return b3.filter(b2.filter(b1.filter(in)));
    };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = filter((*v));
    };
    biquadf_t b1;
    biquadf_t b2;
    biquadf_t b3;
  };

  /**
     @brief Multi-band parametric equalizer with filter-design functions
   */
  class multiband_pareq_t {
  public:
    multiband_pareq_t() {};
    size_t size() const { return flt.size(); };
    void resize(size_t s) { flt.resize(s); };
    /**
       @brief Set center frequencies, gains and q-factor

       @param f Center frequencies in Hz
       @param g Filter gains in dB
       @param q Filter q-factors (dimensionless)
       @param fs Sampling frequency in Hz

       f, g and q need to have the same size.
     */
    void set_fgq(const std::vector<float>& f, const std::vector<float>& g,
                 const std::vector<float>& q, float fs);
    /**
       @brief Optimize coefficients to match given response

       @param numflt Number of filters to be used
       @param maxq Maximum Q value to be used
       @param vF Frequencies at which a gain value is provided.
       @param vG Gain values in dB.
       @param fs Sampling rate in Hz.
       @param numiter Maximum number of iterations.
       @return Resulting gains in dB.

       vF.size() and vG.size() must be equal and at least 3*numflt+1.
     */
    std::vector<float> optim_response(size_t numflt, float maxq,
                                      const std::vector<float>& vF,
                                      const std::vector<float>& vG, float fs,
                                      size_t numiter = 1000,
                                      bool use_nelder_mead = true);
    /**
       @brief Return gain response in dB
       @param f Frequencies in Hz
       @param fs Sampling frequency
     */
    std::vector<float> dbresponse(const std::vector<float>& f, float fs) const;
    /**
       @brief Return gain response in dB
       @retval resp Response vector with gains in dB
       @param f Frequencies in Hz
       @param fs Sampling frequency
     */
    void dbresponse(std::vector<float>& resp, const std::vector<float>& f,
                    float fs) const;
    std::complex<float> response(float phi) const
    {
      std::complex<float> r = g0;
      for(const auto& f : flt)
        r *= f.response(phi);
      return r;
    }
    inline float filter(float in)
    {
      in *= g0;
      for(auto& f : flt)
        in = f.filter(in);
      return in;
    };
    inline void filter(wave_t& w)
    {
      float* wend(w.d + w.n);
      for(float* v = w.d; v < wend; ++v)
        *v = filter(*v);
    };
    void clear()
    {
      for(auto& f : flt)
        f.clear();
    };
    float optim_error_fun(const std::vector<float>& par);
    std::string to_string() const;
    std::vector<float> get_f() const { return flt_f; };
    std::vector<float> get_q() const { return flt_q; };
    std::vector<float> get_g() const { return flt_g; };

  private:
    void optimpar2fltsettings(const std::vector<float>& par, float fs,
                              bool dump = false);
    std::vector<biquadf_t> flt;
    float g0 = 1.0f;
    float fmin = 0.0f;
    float fmax = 1.0f;
    float optim_fs = 1.0f;
    float optim_maxq = 1.0f;
    std::vector<float> optim_f;
    std::vector<float> optim_g;
    std::vector<float> optim_gmeas;
    // filter parameter:
    std::vector<float> flt_f;
    std::vector<float> flt_g;
    std::vector<float> flt_q;
  };

  std::vector<float> rflt2alpha(float reflectivity, float damping, float fs,
                                const std::vector<float>& freq);

  int alpha2rflt(float& reflectivity, float& damping,
                 const std::vector<float>& alpha,
                 const std::vector<float>& freq, float fs,
                 uint32_t numiter = 2000u);

  /**
   \brief First order attack-release lowpass filter

   This filter is the base of first order lowpass filter, maximum
   tracker and minimum tracker.
*/
  class o1_ar_filter_t : public TASCAR::wave_t {
  public:
    o1_ar_filter_t();
    o1_ar_filter_t(const o1_ar_filter_t&);
    o1_ar_filter_t& operator=(const o1_ar_filter_t&);
    /**
       \brief Constructor, setting all taus to zero.

       The filter state can be accessed through the member
       functions of TASCAR::wave_t.

       \param channels Number of independent filters
       \param fs Sampling rate (optional, default = 1)
       \param tau_a Attack time constants (optional, default = 0)
       \param tau_r Release time constants (optional, default = 0)
    */
    o1_ar_filter_t(unsigned int channels, float fs = 1.0f,
                   std::vector<float> tau_a = std::vector<float>(1, 0.0f),
                   std::vector<float> tau_r = std::vector<float>(1, 0.0f));
    /**
       \brief Set the attack time constant
       \param ch Channel number
       \param tau Time constant
    */
    void set_tau_attack(unsigned int ch, float tau);
    /**
       \brief Set the release time constant
       \param ch Channel number
       \param tau Time constant
    */
    void set_tau_release(unsigned int ch, float tau);
    /**
        \brief Apply filter to value x, using state channel ch
        \param ch Cannel number
        \param x Input value
        \return Output value
    */
    inline float operator()(unsigned int ch, float x)
    {
      if(ch >= n)
        throw TASCAR::ErrMsg("The filter channel is out of range (got " +
                             std::to_string(ch) + ", " + std::to_string(n) +
                             " channels).");
      if(x >= d[ch])
        d[ch] = c1_a[ch] * d[ch] + c2_a[ch] * x;
      else
        d[ch] = c1_r[ch] * d[ch] + c2_r[ch] * x;
      make_friendly_number(d[ch]);
      return d[ch];
    };

  protected:
    TASCAR::wave_t c1_a;
    TASCAR::wave_t c2_a;
    TASCAR::wave_t c1_r;
    TASCAR::wave_t c2_r;
    float fs;
  };

  /**
     \brief First order low pass filter
  */
  class o1flt_lowpass_t : public o1_ar_filter_t {
  public:
    o1flt_lowpass_t(const std::vector<float>& tau, float fs,
                    float startval = 0);
    o1flt_lowpass_t(const std::vector<float>& tau, float fs,
                    const std::vector<float>& startval);
    void set_tau(unsigned int ch,
                 float tau); //!< change the time constant in one channel
    void set_tau(float tau); //!< set time constant in all channels to tau
    float get_c1(unsigned int ch) const { return c1_a.d[ch]; };
    float get_last_output(unsigned int ch) const { return d[ch]; }
  };

  /**
   * @class acorr_t
   * @brief Computes the auto-correlation of a wave signal.
   *
   * This class inherits from wave_t and provides functionality to calculate
   * the normalized auto-correlation of a given input signal.
   */
  class acorr_t : public TASCAR::wave_t {
  public:
    /**
     * @brief Constructor for acorr_t.
     * @param lag The maximum lag to be considered in the auto-correlation.
     *            The internal buffer is allocated with size lag + 1.
     */
    acorr_t(uint32_t lag);
    /**
     * @brief Computes the auto-correlation of the provided wave signal.
     *
     * The function calculates the sum of products for each lag up to the
     * minimum of the current object's capacity and the input signal's length.
     * The result is normalized by dividing by the length of the input signal.
     *
     * @param x The input wave signal to correlate.
     * @return A reference to this object (this) to allow method chaining.
     *
     * @note This operation has a time complexity of O(n * lag).
     */
    TASCAR::wave_t& corr(const TASCAR::wave_t& x);

    void update_lag(uint32_t lag);
  };

  class lpc_t : public TASCAR::wave_t {
  public:
    /**
     * @brief Constructor for LPC analysis.
     * @param p The order of the LPC (number of coefficients).
     */
    lpc_t(uint32_t p);

    void update_p(uint32_t p);

    /**
     * @brief Computes LPC coefficients using Levinson-Durbin recursion.
     * @param x The input signal used to derive the coefficients.
     * @return Reference to this object containing the coefficients.
     */
    TASCAR::wave_t& compute_coeffs(const TASCAR::wave_t& x);

    /**
     * @brief Analysis Filter: Extracts the residual (prediction error) from a
     * signal.
     * @param x The input signal.
     * @param res [out] The wave_t object where residuals will be stored.
     * @return Reference to the residual signal.
     */
    TASCAR::wave_t& analyse(const TASCAR::wave_t& x, TASCAR::wave_t& res);

    /**
     * @brief Reconstructs the signal from the residual using the current
     * coefficients.
     * @param res The residual signal (excitation).
     * @param out [out] The wave_t object where the reconstructed signal will be
     * stored.
     * @return Reference to the output signal.
     */
    TASCAR::wave_t& synthesize(const TASCAR::wave_t& res, TASCAR::wave_t& out);

    /**
     * @brief Helper method to perform full analysis (coeffs + residuals).
     */
    TASCAR::wave_t& calc(const TASCAR::wave_t&, TASCAR::wave_t& res);

  private:
    uint32_t p; // LPC Order
    TASCAR::wave_t prev_A;

  public:
    TASCAR::acorr_t acf; // Auto-correlation object
  };

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
