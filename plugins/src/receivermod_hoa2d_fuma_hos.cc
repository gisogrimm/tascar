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

#include "amb33defs.h"
#include "errorhandling.h"
#include "scene.h"
#include <fftw3.h>
#include <string.h>

const std::complex<float> i_f(0.0, 1.0);

class hoa2d_t : public TASCAR::receivermod_base_t {
public:
  class data_t : public TASCAR::receivermod_base_t::data_t {
  public:
    data_t(uint32_t chunksize, uint32_t channels);
    virtual ~data_t();
    // point source speaker weights:
    TASCAR::spec_t enc_w;
    TASCAR::spec_t enc_dw;
    double gauge = 0.0;
    double dgauge = 0.0;
    float w_centre = 0.0f;
  };
  hoa2d_t(tsccfg::node_t xmlsrc);
  virtual ~hoa2d_t();
  void add_pointsource(const TASCAR::pos_t& prel, double width,
                       const TASCAR::wave_t& chunk,
                       std::vector<TASCAR::wave_t>& output,
                       receivermod_base_t::data_t*);
  void add_diffuse_sound_field(const TASCAR::amb1wave_t& chunk,
                               std::vector<TASCAR::wave_t>& output,
                               receivermod_base_t::data_t*);
  receivermod_base_t::data_t* create_state_data(double srate,
                                                uint32_t n_fragment) const;
  // allocate buffers:
  void configure();
  // re-order HOA signals:
  void postproc(std::vector<TASCAR::wave_t>& output);
  void add_variables(TASCAR::osc_server_t* srv);

private:
  uint32_t nbins = 0;
  uint32_t order = 0;
  float rho0 = 1.0;
  float rhoc = 0.25;
  TASCAR::spec_t s_encoded;
  TASCAR::spec_t s_encoded_alt;
};

hoa2d_t::hoa2d_t(tsccfg::node_t xmlsrc)
    : TASCAR::receivermod_base_t(xmlsrc), s_encoded(1), s_encoded_alt(1)
{
  GET_ATTRIBUTE(order, "", "Ambisonics order");
  GET_ATTRIBUTE(rho0, "m", "Reference radius");
  GET_ATTRIBUTE(rhoc, "m", "Center radius");
  nbins = order + 2;
}

hoa2d_t::~hoa2d_t() {}

void hoa2d_t::add_variables(TASCAR::osc_server_t* srv)
{
  TASCAR::receivermod_base_t::add_variables(srv);
  srv->set_variable_owner(TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/rho0", &rho0, "[0,2]", "Reference radius in m");
  srv->add_float("/rhoc", &rhoc, "[0,2]", "Centre radius in m");
  srv->unset_variable_owner();
}

void hoa2d_t::configure()
{
  TASCAR::receivermod_base_t::configure();
  s_encoded.resize(n_fragment * nbins);
  s_encoded.clear();
  s_encoded_alt.resize(n_fragment * nbins);
  s_encoded_alt.clear();
  labels.clear();
  n_channels = 2 * (order * 2 + 1);
  for(uint32_t kch = 0; kch < n_channels; ++kch) {
    uint32_t ch = kch;
    char ctmp[1024];
    ctmp[1023] = 0;
    char ctmpa[2];
    ctmpa[1] = 0;
    ctmpa[0] = 0;
    if(ch >= (2 * order + 1)) {
      ch -= 2 * order + 1;
      ctmpa[0] = 'a';
    }
    uint32_t o((ch + 1) / 2);
    int32_t s(o * (2 * ((ch + 1) % 2) - 1));
    snprintf(ctmp, 1023, ".%s%d_%d", ctmpa, o, s);
    labels.push_back(ctmp);
  }
  // add extra centre speaker channel:
  n_channels += 1;
  labels.push_back(".centre");
}

hoa2d_t::data_t::data_t(uint32_t, uint32_t order)
    : enc_w(order + 1), enc_dw(order + 1)
{
}

hoa2d_t::data_t::~data_t() {}

void hoa2d_t::add_pointsource(const TASCAR::pos_t& prel, double,
                              const TASCAR::wave_t& chunk,
                              std::vector<TASCAR::wave_t>& output,
                              receivermod_base_t::data_t* sd)
{
  data_t* d((data_t*)sd);
  auto pnorm = prel.normal();
  pnorm.z = 0.0;
  float gain_xy = pnorm.norm();
  float gain_z = (1.0f - gain_xy) * sqrtf(order) + gain_xy;
  float az(-prel.azim());
  float rho(prel.norm());
  float w_centre = std::max(0.0f, -(rho - rhoc) / rhoc);
  float dw_centre = (w_centre - d->w_centre) * (float)t_inc;
  float normd(rho / rho0);
  std::complex<float> ciazp(std::exp(i_f * az));
  std::complex<float> ckiazp(ciazp);
  d->dgauge = (1.0 / (1.0 + normd) - d->gauge) * t_inc;
  for(uint32_t ko = 1; ko <= order; ++ko) {
    d->enc_dw[ko] = (ckiazp - d->enc_w[ko]) * (float)t_inc;
    ckiazp *= ciazp;
  }
  d->enc_dw[0] = 0.0f;
  d->enc_w[0] = 1.0f;
  float* vpend(chunk.d + chunk.n);
  std::complex<float>* encp(s_encoded.b);
  std::complex<float>* encpa(s_encoded_alt.b);
  float* v_centre = output[output.size() - 1].d;
  for(float* vp = chunk.d; vp != vpend; ++vp) {
    d->w_centre += dw_centre;
    *v_centre += d->w_centre * *vp;
    //*vp *= (1.0f - d->w_centre);
    d->gauge += d->dgauge;
    float w_gauge = d->gauge * (1.0f - d->w_centre);
    float gaugea = (1.0f - d->gauge);
    std::complex<float>* pwp(d->enc_w.b);
    std::complex<float>* pdwp(d->enc_dw.b);
    {
      *pwp += *pdwp;
      *encp += *pwp * (*vp) * w_gauge * gain_z;
      *encpa += *pwp * (*vp) * gaugea * gain_z;
      ++encp;
      ++encpa;
      ++pwp;
      ++pdwp;
    }
    for(uint32_t ko = 1; ko <= order; ++ko) {
      *pwp += *pdwp;
      *encp += *pwp * (*vp) * w_gauge * gain_xy;
      *encpa += *pwp * (*vp) * gaugea * gain_xy;
      ++encp;
      ++encpa;
      ++pwp;
      ++pdwp;
    }
    for(uint32_t ko = order + 1; ko < nbins; ++ko) {
      ++encp;
      ++encpa;
    }
    ++v_centre;
  }
  d->w_centre = w_centre;
}

void hoa2d_t::postproc(std::vector<TASCAR::wave_t>& output)
{
  uint32_t ch(order * 2 + 1);
  for(uint32_t kt = 0; kt < n_fragment; ++kt) {
    output[0][kt] = MIN3DB * s_encoded[kt * nbins].real();
    output[ch][kt] = MIN3DB * s_encoded_alt[kt * nbins].real();
  }
  for(uint32_t ko = 1; ko <= order; ++ko) {
    uint32_t kc(2 * ko - 1);
    for(uint32_t kt = 0; kt < n_fragment; ++kt) {
      output[kc][kt] = s_encoded[kt * nbins + ko].imag();
      output[kc + ch][kt] = s_encoded_alt[kt * nbins + ko].imag();
    }
    ++kc;
    for(uint32_t kt = 0; kt < n_fragment; ++kt) {
      output[kc][kt] = s_encoded[kt * nbins + ko].real();
      output[kc + ch][kt] = s_encoded_alt[kt * nbins + ko].real();
    }
  }
  //
  s_encoded.clear();
  s_encoded_alt.clear();
  TASCAR::receivermod_base_t::postproc(output);
}

void hoa2d_t::add_diffuse_sound_field(const TASCAR::amb1wave_t& chunk,
                                      std::vector<TASCAR::wave_t>&,
                                      receivermod_base_t::data_t*)
{
  for(uint32_t kt = 0; kt < n_fragment; ++kt) {
    s_encoded[kt * nbins] += chunk.w()[kt];
    s_encoded[kt * nbins + 1] += (chunk.x()[kt] + i_f * chunk.y()[kt]);
  }
}

TASCAR::receivermod_base_t::data_t*
hoa2d_t::create_state_data(double, uint32_t n_fragment) const
{
  return new data_t(n_fragment, order);
}

REGISTER_RECEIVERMOD(hoa2d_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
