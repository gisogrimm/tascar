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

class multibeam_t : public maskplugin_base_t {
public:
  multibeam_t(const maskplugin_cfg_t& cfg);
  float get_gain(const pos_t& pos);
  void get_diff_gain(float* gm);
  void add_variables(TASCAR::osc_server_t* srv);

private:
  void resize_val();
  uint32_t numbeams = 1u;
  float mingain = 0.0f;
  float maxgain = 1.0f;
  bool bypass = false;

public:
  std::vector<float> gain;
  std::vector<float> az;
  std::vector<float> el;
  std::vector<float> selectivity;
};

void resize_with_default(std::vector<float>& vec, float def, size_t n)
{
  for(size_t k = vec.size(); k < n; ++k)
    vec.push_back(def);
  vec.resize(n);
}

void multibeam_t::resize_val()
{
  resize_with_default(gain, 1.0f, numbeams);
  resize_with_default(az, 0.0f, numbeams);
  resize_with_default(el, 0.0f, numbeams);
  resize_with_default(selectivity, 1.0f, numbeams);
}

multibeam_t::multibeam_t(const maskplugin_cfg_t& cfg) : maskplugin_base_t(cfg)
{
  GET_ATTRIBUTE(numbeams, "", "Number of beams");
  resize_val();
  GET_ATTRIBUTE_DB(mingain, "Minimum gain");
  GET_ATTRIBUTE_DB(maxgain, "Maximum gain");
  GET_ATTRIBUTE_DB(gain, "On-axis gain");
  GET_ATTRIBUTE(az, "deg", "Azimuth of steering vectors");
  GET_ATTRIBUTE(el, "deg", "Elevation of steering vectors");
  GET_ATTRIBUTE(selectivity, "1/pi",
                "Selectivity, 0 = omni, 1 = cardioid (6 dB threshold)");
  resize_val();
}

void multibeam_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_vector_float_db("/gain", &gain);
  srv->add_vector_float("/selectivity", &selectivity);
  srv->add_vector_float("/az", &az);
  srv->add_vector_float("/el", &el);
  srv->add_float_db("/mingain", &mingain);
  srv->add_float_db("/maxgain", &maxgain);
  srv->add_bool("/bypass", &bypass);
  srv->unset_variable_owner();
}

float multibeam_t::get_gain(const pos_t& pos)
{
  if( bypass )
    return 1.0f;
  TASCAR::pos_t rp(pos.normal());
  float pgain = 0.0f;
  for(size_t k = 0; k < numbeams; ++k) {
    TASCAR::pos_t psteer;
    psteer.set_sphere(1.0, DEG2RAD * az[k], DEG2RAD * el[k]);
    float ang =
        std::min(selectivity[k] * acosf(dot_prodf(rp, psteer)), TASCAR_PIf);
    pgain += gain[k] * (0.5f + 0.5f * cosf(ang));
  }
  pgain = std::min(maxgain, mingain + (1.0f - mingain) * pgain);
  return pgain;
};

void multibeam_t::get_diff_gain(float* gm)
{
  if( bypass )
    return;
  memset(gm, 0, sizeof(float) * 16);
  float diag_gain = mingain;
  for(size_t k = 0; k < numbeams; ++k) {
    TASCAR::pos_t psteer;
    psteer.set_sphere(1.0, DEG2RAD * az[k], DEG2RAD * el[k]);
    float dgain = (1.0f - std::min(selectivity[k], 1.0f));
    diag_gain += gain[k] * dgain;
    // compensate for selectivity:
    float selgain = 1.0f - expf(-1.0f / (selectivity[k] * selectivity[k]));
    selgain *= (1.0f - dgain) * 0.5674f;
    float pgainw = gain[k] * selgain;
    float pgainy = (float)psteer.y * pgainw;
    float pgainz = (float)psteer.z * pgainw;
    float pgainx = (float)psteer.x * pgainw;
    float gains[4] = {1.0f, (float)(psteer.y), (float)(psteer.z),
                      (float)(psteer.x)};
    size_t kgain = 0;
    for(size_t r = 0; r < 16; r += 4) {
      float gain = gains[kgain];
      ++kgain;
      gm[r] += pgainw * gain;
      gm[r + 1] += pgainy * gain;
      gm[r + 2] += pgainz * gain;
      gm[r + 3] += pgainx * gain;
    }
  }
  for(size_t r = 0; r < 16; r += 5)
    gm[r] += diag_gain;
  for(size_t r = 0; r < 16; ++r)
    gm[r] = std::min(maxgain, gm[r]);
}

REGISTER_MASKPLUGIN(multibeam_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
