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
#include "errorhandling.h"
#include "filterclass.h"

class pareq4band_t : public TASCAR::audioplugin_base_t {
public:
  enum filtertype_t { lowpass, highpass, equalizer, highshelf, lowshelf };
  pareq4band_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~pareq4band_t();

private:
  float b1ls_f = 150.0;
  float b2eq_f = 560.0;
  float b3eq_f = 1500.0;
  float b4hs_f = 5600.0;
  float b1ls_Q = 1.0;
  float b2eq_Q = 1.0;
  float b3eq_Q = 1.0;
  float b4hs_Q = 1.0;
  float b1ls_g = 0.0;
  float b2eq_g = 0.0;
  float b3eq_g = 0.0;
  float b4hs_g = 0.0;
  bool b1ls_act = true;
  bool b2eq_act = true;
  bool b3eq_act = true;
  bool b4hs_act = true;
  std::vector<TASCAR::biquadf_t*> b1ls_flt;
  std::vector<TASCAR::biquadf_t*> b2eq_flt;
  std::vector<TASCAR::biquadf_t*> b3eq_flt;
  std::vector<TASCAR::biquadf_t*> b4hs_flt;
};

pareq4band_t::pareq4band_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(b1ls_f, "Hz", "low-shelf frequency 22-560");
  GET_ATTRIBUTE(b2eq_f, "Hz", "mid1 eq frequency 150-1.5k");
  GET_ATTRIBUTE(b3eq_f, "Hz", "mid2 eq frequency 560-5.6k");
  GET_ATTRIBUTE(b4hs_f, "Hz", "high-shelf frequency 5.6k-24k");
  GET_ATTRIBUTE(b1ls_g, "dB", "low-shelf gain -20,20");
  GET_ATTRIBUTE(b2eq_g, "dB", "mid1 eq gain");
  GET_ATTRIBUTE(b3eq_g, "dB", "mid2 eq gain");
  GET_ATTRIBUTE(b4hs_g, "dB", "high-shelf gain");
  GET_ATTRIBUTE(b1ls_Q, "", "slope");
  GET_ATTRIBUTE(b2eq_Q, "", "bandwidth");
  GET_ATTRIBUTE(b3eq_Q, "", "bandwidth");
  GET_ATTRIBUTE(b4hs_Q, "", "slope");
  GET_ATTRIBUTE_BOOL(b1ls_act, "activate");
  GET_ATTRIBUTE_BOOL(b2eq_act, "activate");
  GET_ATTRIBUTE_BOOL(b3eq_act, "activate");
  GET_ATTRIBUTE_BOOL(b4hs_act, "activate");
}

void pareq4band_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/b1ls_act", &b1ls_act, "active");
  srv->add_float("/b1ls_f", &b1ls_f, "[22,560]", "low-shelf frequency in Hz");
  srv->add_float("/b1ls_g", &b1ls_g, "[-20,20]", "Gain in dB");
  srv->add_float("/b1ls_Q", &b1ls_Q, "[0.1,10]", "Q-factor");
  srv->add_bool("/b2eq_act", &b2eq_act, "active");
  srv->add_float("/b2eq_f", &b2eq_f, "[150,1500]", "mid1 eq frequency in Hz");
  srv->add_float("/b2eq_g", &b2eq_g, "[-20,20]", "Gain in dB");
  srv->add_float("/b2eq_Q", &b2eq_Q, "[0.1,10]", "Q-factor");
  srv->add_bool("/b3eq_act", &b3eq_act, "active");
  srv->add_float("/b3eq_f", &b3eq_f, "[560,5600]", "mid2 eq frequency in Hz");
  srv->add_float("/b3eq_g", &b3eq_g, "[-20,20]", "Gain in dB");
  srv->add_float("/b3eq_Q", &b3eq_Q, "[0.1,10]", "Q-factor");
  srv->add_bool("/b4hs_act", &b4hs_act, "active");
  srv->add_float("/b4hs_f", &b4hs_f, "[1500,24000]",
                 "high-shelf frequency in Hz");
  srv->add_float("/b4hs_g", &b4hs_g, "[-20,20]", "Gain in dB");
  srv->add_float("/b4hs_Q", &b4hs_Q, "[0.1,10]", "Q-factor");
  srv->unset_variable_owner();
}

void pareq4band_t::configure()
{
  audioplugin_base_t::configure();
  for(uint32_t ch = 0; ch < n_channels; ++ch) {
    b1ls_flt.push_back(new TASCAR::biquadf_t());
    b2eq_flt.push_back(new TASCAR::biquadf_t());
    b3eq_flt.push_back(new TASCAR::biquadf_t());
    b4hs_flt.push_back(new TASCAR::biquadf_t());
  }
}

void pareq4band_t::release()
{
  audioplugin_base_t::release();
  for(auto& b1ls_f : b1ls_flt)
    delete(b1ls_f);
  b1ls_flt.clear();
  for(auto& b2eq_f : b2eq_flt)
    delete(b2eq_f);
  b2eq_flt.clear();
  for(auto& b3eq_f : b3eq_flt)
    delete(b3eq_f);
  b3eq_flt.clear();
  for(auto& b4hs_f : b4hs_flt)
    delete(b4hs_f);
  b4hs_flt.clear();
}

pareq4band_t::~pareq4band_t() {}

void pareq4band_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                              const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                              const TASCAR::transport_t&)
{
  for(size_t k = 0; k < chunk.size(); ++k) {
    if(b1ls_act) {
      b1ls_flt[k]->set_lowshelf(b1ls_f, (float)f_sample, b1ls_g, b1ls_Q);
      b1ls_flt[k]->filter(chunk[k]);
    }
    if(b2eq_act) {
      b2eq_flt[k]->set_pareq(b2eq_f, (float)f_sample, b2eq_g, b2eq_Q);
      b2eq_flt[k]->filter(chunk[k]);
    }
    if(b3eq_act) {
      b3eq_flt[k]->set_pareq(b3eq_f, (float)f_sample, b3eq_g, b3eq_Q);
      b3eq_flt[k]->filter(chunk[k]);
    }
    if(b4hs_act) {
      b4hs_flt[k]->set_highshelf(b4hs_f, (float)f_sample, b4hs_g, b4hs_Q);
      b4hs_flt[k]->filter(chunk[k]);
    }
  }
}

REGISTER_AUDIOPLUGIN(pareq4band_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
