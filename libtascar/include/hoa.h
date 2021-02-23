#ifndef HOA_H
#define HOA_H

#include "audiochunks.h"
#include "coordinates.h"
#include "errorhandling.h"
#include "xmlconfig.h"
#include <cmath>
#include <gsl/gsl_sf.h>
#include <vector>

namespace HOA {

  class encoder_t {
  public:
    encoder_t();
    ~encoder_t();
    void set_order(uint32_t order);
    inline void operator()(float azimuth, float elevation,
                           std::vector<float>& B)
    {
      if(B.size() < n_elements)
        throw TASCAR::ErrMsg("Insufficient space for ambisonic weights.");
      gsl_sf_legendre_array(GSL_SF_LEGENDRE_SCHMIDT, M, sin(elevation), leg);
      uint32_t acn(0);
      for(int l = 0; l <= M; ++l) {
        for(int m = -l; m <= l; ++m) {
          double P(leg[gsl_sf_legendre_array_index(l, abs(m))]);
          if(m < 0)
            B[acn] = P * sin(abs(m) * azimuth);
          else if(m == 0)
            B[acn] = P;
          else
            B[acn] = P * cos(abs(m) * azimuth);
          ++acn;
        }
      }
    };

  private:
    int32_t M;
    uint32_t n_elements;
    double* leg;
  };

  class decoder_t {
  public:
    enum modifier_t { basic, maxre, inphase };
    enum method_t { pinv, allrad };
    decoder_t();
    ~decoder_t();
    void modify(const modifier_t& m);
    void create_pinv(uint32_t order, const std::vector<TASCAR::pos_t>& spkpos);
    void create_allrad(uint32_t order,
                       const std::vector<TASCAR::pos_t>& spkpos);
    inline float& operator()(uint32_t acn, uint32_t outc)
    {
      return dec[outc + acn * out_channels];
    }
    inline const float& operator()(uint32_t acn, uint32_t outc) const
    {
      return dec[outc + acn * out_channels];
    }
    inline void operator()(const std::vector<TASCAR::wave_t>& ambsig,
                           std::vector<TASCAR::wave_t>& outsig)
    {
      if(ambsig.size() != amb_channels)
        throw TASCAR::ErrMsg(
            "Invalid number of channels in ambisonics signal (got " +
            TASCAR::to_string(ambsig.size()) + ", expected " +
            TASCAR::to_string(amb_channels) + ").");
      if(outsig.size() < out_channels)
        throw TASCAR::ErrMsg(
            "Invalid number of channels in ambisonics signal (got " +
            TASCAR::to_string(outsig.size()) + ", expected " +
            TASCAR::to_string(out_channels) + ").");
      if(!out_channels)
        return;
      uint32_t n_fragment(outsig[0].n);
      float* p_dec(dec);
      for(uint32_t acn = 0; acn < amb_channels; ++acn) {
        for(uint32_t kch = 0; kch < out_channels; ++kch) {
          float* w_amb(ambsig[acn].d);
          float* w_spk(outsig[kch].d);
          for(uint32_t kt = 0; kt < n_fragment; ++kt) {
            *w_spk += *w_amb * *p_dec;
            ++w_spk;
            ++w_amb;
          }
          ++p_dec;
        }
      }
    }
    inline void operator()(const std::vector<float>& ambsig,
                           std::vector<float>& outsig)
    {
      if(ambsig.size() != amb_channels)
        throw TASCAR::ErrMsg(
            "Invalid number of channels in ambisonics signal (got " +
            TASCAR::to_string(ambsig.size()) + ", expected " +
            TASCAR::to_string(amb_channels) + ").");
      if(outsig.size() != out_channels)
        throw TASCAR::ErrMsg(
            "Invalid number of channels in ambisonics signal (got " +
            TASCAR::to_string(outsig.size()) + ", expected " +
            TASCAR::to_string(out_channels) + ").");
      if(!out_channels)
        return;
      float* p_dec(dec);
      for(uint32_t acn = 0; acn < amb_channels; ++acn) {
        for(uint32_t kch = 0; kch < out_channels; ++kch) {
          outsig[kch] += ambsig[acn] * *p_dec;
          ++p_dec;
        }
      }
    }
    std::string to_string() const;
    float maxabs() const;
    float rms() const;

  private:
    float* dec;
    uint32_t amb_channels;
    uint32_t out_channels;
    int32_t M;
    decoder_t::modifier_t dectype;
    method_t method;
  };

  std::vector<double> legendre_poly(size_t n);
  std::vector<double> roots(const std::vector<double>& P);
  std::vector<double> maxre_gm(size_t M);
  std::vector<double> inphase_gm(size_t M);

} // namespace HOA

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
