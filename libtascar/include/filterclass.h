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
        : a1_(a1), a2_(a2), b0_(b0), b1_(b1), b2_(b2), z1(0.0), z2(0.0){};
    biquad_t() : a1_(0), a2_(0), b0_(1), b1_(0), b2_(0), z1(0.0), z2(0.0){};
    void set_gzp(double g, double zero_r, double zero_phi, double pole_r,
                 double pole_phi);
    void set_analog(double g, double z1, double z2, double p1, double p2,
                    double fs);
    void set_analog_poles(double g, double p1, double p2, double fs);
    void set_coefficients(double a1, double a2, double b0, double b1, double b2)
    {
      a1_ = a1;
      a2_ = a2;
      b0_ = b0;
      b1_ = b1;
      b2_ = b2;
    };
    // void set_analog( double g, double f_pole, double fs );
    void set_highpass(double fc, double fs, bool phaseinvert = false);
    void set_lowpass(double fc, double fs, bool phaseinvert = false);
    void set_pareq(double f, double fs, double gain, double q);
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
        : a1_(a1), a2_(a2), b0_(b0), b1_(b1), b2_(b2), z1(0.0), z2(0.0){};
    biquadf_t() : a1_(0), a2_(0), b0_(1), b1_(0), b2_(0), z1(0.0), z2(0.0){};
    void set_gzp(float g, float zero_r, float zero_phi, float pole_r,
                 float pole_phi);
    void set_analog(float g, float z1, float z2, float p1, float p2,
                    float fs);
    void set_analog_poles(float g, float p1, float p2, float fs);
    void set_coefficients(float a1, float a2, float b0, float b1, float b2)
    {
      a1_ = a1;
      a2_ = a2;
      b0_ = b0;
      b1_ = b1;
      b2_ = b2;
    };
    // void set_analog( float g, float f_pole, float fs );
    void set_highpass(float fc, float fs, bool phaseinvert = false);
    void set_lowpass(float fc, float fs, bool phaseinvert = false);
    void set_pareq(float f, float fs, float gain, float q);
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
    float z1, z2;
  };

  class bandpass_t {
  public:
    bandpass_t(double f1, double f2, double fs);
    void set_range(double f1, double f2);
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

  /**
     @brief Optimize an array of parametric equalizers to match a given
     frequency response.

     @retval flt Filters to be optimized.
     @retval g Broadband gain to be optimized.
     @param vF Frequencies at which a gain value is provided.
     @param vG Gain values in dB.
     @param fs Sampling rate in Hz.

     vF.size() and vG.size() must be equal and at least 3*flt.size()+1.
   */
  float optim_parameq(std::vector<biquadf_t>& flt, float& g,
                      const std::vector<float>& vF,
                      const std::vector<float>& vG, float fs);

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
