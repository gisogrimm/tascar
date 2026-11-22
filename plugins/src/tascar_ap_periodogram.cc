/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "audioplugin.h"
#include "delayline.h"
#include "errorhandling.h"
#include "filterclass.h"
#include <lsl_cpp.h>

class periodogram_t : public TASCAR::audioplugin_base_t {
public:
  periodogram_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~periodogram_t();

private:
  double tau;
  std::vector<double> fmin;
  std::vector<double> fmax;
  bool envelope;
  double coeff;
  std::vector<double> periods;
  std::string name;
  double lpc1;
  double lpc11;
  std::vector<double> env;
  uint32_t nbands;
  uint32_t nperiods;
  //
  std::vector<TASCAR::bandpass_t*> bp;
  std::vector<TASCAR::static_delay_t> delays;
  std::vector<double> out;
  std::vector<double> out_send;
  lsl::stream_outlet* lsl_outlet;
};

void periodogram_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double("/coeff", &coeff);
  srv->add_double("/tau", &tau);
  srv->unset_variable_owner();
}

periodogram_t::periodogram_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), tau(1), fmin(50), fmax(2000), envelope(true),
      coeff(0.8), periods(1, 0.1), name("periodogram"), lpc1(0.0), lpc11(1.0),
      env(0.0), nbands(0), nperiods(0)
{
  GET_ATTRIBUTE_(tau);
  GET_ATTRIBUTE_(fmin);
  GET_ATTRIBUTE_(fmax);
  GET_ATTRIBUTE_(coeff);
  GET_ATTRIBUTE_BOOL_(envelope);
  GET_ATTRIBUTE_(periods);
  GET_ATTRIBUTE_(name);
  if(periods.size() < 2)
    throw TASCAR::ErrMsg("At least two periods are required.");
  if(fmin.empty())
    throw TASCAR::ErrMsg("No frequency bins defined");
  if(fmin.size() != fmax.size())
    throw TASCAR::ErrMsg("Not the same number of entries in fmin and fmax.");
}

void periodogram_t::configure()
{
  audioplugin_base_t::configure();
  delays.clear();
  nbands = fmin.size();
  nperiods = periods.size();
  for(auto it = periods.begin(); it != periods.end(); ++it)
    for(uint32_t band = 0; band < nbands; ++band)
      delays.emplace_back(*it * f_sample);
  for(auto it = bp.begin(); it != bp.end(); ++it)
    delete *it;
  bp.clear();
  for(uint32_t ch = 0; ch < nbands; ++ch) {
    bp.push_back(new TASCAR::bandpass_t(fmin[ch], fmax[ch], f_sample));
  }
  out = std::vector<double>(nperiods * nbands, 0.0f);
  env = std::vector<double>(nbands, 0.0f);
  out_send = std::vector<double>(nperiods * nbands, 0.0f);
  lsl_outlet = new lsl::stream_outlet(lsl::stream_info(
      name, "level", nperiods * nbands, f_fragment, lsl::cf_double64));
}

void periodogram_t::release()
{
  audioplugin_base_t::release();
  for(auto it = bp.begin(); it != bp.end(); ++it)
    delete *it;
  bp.clear();
  delete lsl_outlet;
}

periodogram_t::~periodogram_t() {}

void periodogram_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                               const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                               const TASCAR::transport_t&)
{
  uint32_t N(chunk[0].size());
  lpc1 = exp(-1.0 / (tau * f_sample));
  lpc11 = 1.0 - lpc1;
  for(uint32_t band = 0; band < nbands; ++band) {
    for(uint32_t k = 0; k < N; ++k) {
      float v2(bp[band]->filter(chunk[0][k]));
      if(envelope) {
        v2 *= v2;
        env[band] = lpc1 * env[band] + lpc11 * v2;
        v2 = sqrtf(env[band]);
      }
      for(uint32_t ch = 0; ch < nperiods; ++ch) {
        out[ch + nperiods * band] =
            coeff * out[ch + nperiods * band] +
            (1.0 - coeff) * delays[ch * nbands + band](v2);
        // lo_send( lo_addr, path.c_str(), "ssffff", "/hitAt", this_side, pos.x,
        // pos.y, pos.z, sqrt(ons) );
      }
    }
  }
  for(uint32_t band = 0; band < nbands; ++band) {
    double rms(0.0);
    for(uint32_t ch = 0; ch < nperiods; ++ch)
      rms += out[ch + nperiods * band] * out[ch + nperiods * band];
    rms = sqrt(rms);
    if(rms > 0)
      for(uint32_t ch = 0; ch < nperiods; ++ch)
        out_send[ch + nperiods * band] = out[ch + nperiods * band] / rms;
  }
  lsl_outlet->push_sample(out_send);
}

REGISTER_AUDIOPLUGIN(periodogram_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
