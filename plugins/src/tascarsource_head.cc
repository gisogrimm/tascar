/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2026 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATANBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "filterclass.h"
#include "sourcemod.h"

class srchead_t : public TASCAR::sourcemod_base_t {
public:
  class data_t : public TASCAR::sourcemod_base_t::data_t {
  public:
    data_t(uint32_t chunksize);
    // float dt = 0.0f;
    // float w = 0.0f;
    TASCAR::biquadf_t flt;
    // TASCAR::biquadf_t flto;
  };
  srchead_t(tsccfg::node_t xmlsrc);
  void add_variables(TASCAR::osc_server_t* srv);
  bool read_source(TASCAR::pos_t& prel,
                   const std::vector<TASCAR::wave_t>& input,
                   TASCAR::wave_t& output, sourcemod_base_t::data_t*);
  TASCAR::sourcemod_base_t::data_t* create_state_data(double srate,
                                                      uint32_t fragsize) const;
  void configure() { n_channels = 1; };

private:
  float fc_front = 15000.0f;
  float fc_back = 500.0f;
  float fc_left = 4000.0f;
  float fc_right = 4000.0f;
  float fc_top = 5000.0f;
  float fc_bottom = 8000.0f;
};

srchead_t::data_t::data_t(uint32_t) {}

srchead_t::srchead_t(tsccfg::node_t xmlsrc) : TASCAR::sourcemod_base_t(xmlsrc)
{
  GET_ATTRIBUTE(fc_front, "Hz", "Lowpass frequency in frontal direction");
  GET_ATTRIBUTE(fc_back, "Hz", "Lowpass frequency in back direction");
  GET_ATTRIBUTE(fc_left, "Hz", "Lowpass frequency in left direction");
  GET_ATTRIBUTE(fc_right, "Hz", "Lowpass frequency in right direction");
  GET_ATTRIBUTE(fc_top, "Hz", "Lowpass frequency in top direction");
  GET_ATTRIBUTE(fc_bottom, "Hz", "Lowpass frequency in bottom direction");
}

void srchead_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/fc_front", &fc_front, "[100,10000]",
                 "Lowpass frequency in frontal direction");
  srv->add_float("/fc_back", &fc_back, "[100,10000]",
                 "Lowpass frequency in back direction");
  srv->add_float("/fc_left", &fc_left, "[100,10000]",
                 "Lowpass frequency in left direction");
  srv->add_float("/fc_right", &fc_right, "[100,10000]",
                 "Lowpass frequency in right direction");
  srv->add_float("/fc_top", &fc_top, "[100,10000]",
                 "Lowpass frequency in top direction");
  srv->add_float("/fc_bottom", &fc_bottom, "[100,10000]",
                 "Lowpass frequency in bottom direction");
  srv->unset_variable_owner();
}

bool srchead_t::read_source(TASCAR::pos_t& prel,
                            const std::vector<TASCAR::wave_t>& input,
                            TASCAR::wave_t& output,
                            sourcemod_base_t::data_t* sd)
{
  data_t* d((data_t*)sd);
  // d->flto.set_butterworth(fc, f_sample);
  TASCAR::pos_t prel_norm(prel.normal());
  // calculate panning parameters (as incremental values):
  TASCAR::pos_t weights = prel_norm;
  weights.x = fabs(weights.x);
  weights.y = fabs(weights.y);
  weights.z = fabs(weights.z);
  //weights.x *= weights.x;
  //weights.y *= weights.y;
  //weights.z *= weights.z;
  float w_total = weights.x + weights.y + weights.z;
  float fc_x = fc_front;
  if(prel_norm.x < 0)
    fc_x = fc_back;
  float fc_y = fc_left;
  if(prel_norm.y < 0)
    fc_y = fc_right;
  float fc_z = fc_top;
  if(prel_norm.z < 0)
    fc_z = fc_bottom;
  float fc = exp(
      (weights.x * log(fc_x) + weights.y * log(fc_y) + weights.z * log(fc_z)) /
      w_total);
  d->flt.set_butterworth(fc, f_sample);
  // apply panning:
  uint32_t N(output.size());
  for(uint32_t k = 0; k < N; ++k) {
    float v = input[0][k];
    output[k] = d->flt.filter(v);
  }
  return false;
}

TASCAR::sourcemod_base_t::data_t*
srchead_t::create_state_data(double, uint32_t fragsize) const
{
  return new data_t(fragsize);
}

REGISTER_SOURCEMOD(srchead_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
