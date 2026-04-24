/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "maskplugin.h"
#include <mutex>

using namespace TASCAR;

class sampledgain_t : public maskplugin_base_t {
public:
  sampledgain_t(const maskplugin_cfg_t& cfg);
  float get_gain(const pos_t& pos);
  void get_diff_gain(float* gm);
  void add_variables(TASCAR::osc_server_t* srv);

private:
  void resize_val();
  uint32_t numsamples = 1u;
  std::vector<TASCAR::pos_t> samples;
  bool bypass = false;

public:
  std::vector<float> az;
  std::vector<float> el;
  std::vector<float> gain;
};

void resize_with_default(std::vector<float>& vec, float def, size_t n)
{
  for(size_t k = vec.size(); k < n; ++k)
    vec.push_back(def);
  vec.resize(n);
}

void sampledgain_t::resize_val()
{
  resize_with_default(gain, 1.0f, numsamples);
  resize_with_default(az, 0.0f, numsamples);
  resize_with_default(el, 0.0f, numsamples);
}

sampledgain_t::sampledgain_t(const maskplugin_cfg_t& cfg)
    : maskplugin_base_t(cfg)
{
  GET_ATTRIBUTE(az, "deg", "Azimuth of gain samples");
  GET_ATTRIBUTE(el, "deg", "Elevation of gain samples");
  numsamples = std::max(az.size(), el.size());
  resize_val();
  GET_ATTRIBUTE_DB(gain, "Gain at samples");
  resize_val();
  for(uint32_t k = 0; k < numsamples; ++k) {
    TASCAR::pos_t smp;
    smp.set_sphere(1.0, az[k]*DEG2RAD, el[k]*DEG2RAD);
    samples.push_back(smp);
  }
}

void sampledgain_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_vector_float_db("/gain", &gain);
  srv->add_vector_float("/lingain", &gain);
  srv->add_bool("/bypass", &bypass);
  srv->unset_variable_owner();
}

float sampledgain_t::get_gain(const pos_t& pos)
{
  if( bypass )
    return 1.0f;
  if(numsamples == 0)
    return 1.0f;
  TASCAR::pos_t rp(pos.normal());
  uint32_t kmin = 0u;
  double dmin = distance(rp, samples[0]);
  for(uint32_t k = 1u; k < numsamples; ++k) {
    double d = distance(rp, samples[k]);
    if(d < dmin) {
      dmin = d;
      kmin = k;
    }
  }
  return gain[kmin];
};

void sampledgain_t::get_diff_gain(float* gm)
{
  if( bypass )
    return;
  memset(gm, 0, sizeof(float) * 16);
  float diag_gain = 0.0f;
  for(auto g : gain)
    diag_gain += g;
  diag_gain /= numsamples;
  for(size_t r = 0; r < 16; r += 5)
    gm[r] += diag_gain;
}

REGISTER_MASKPLUGIN(sampledgain_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
