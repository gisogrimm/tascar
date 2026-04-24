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
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
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
  if(bypass)
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
}

/**
 * @brief Applies modal filtering in the first-order Ambisonics (FOA) domain to
 * suppress diffuse sound fields.
 *
 * This function computes a 4×4 spatial filter matrix (stored in `gm`) that
 * modifies the FOA signal (W, X, Y, Z) to achieve directional selectivity. It
 * is used when processing diffuse sound fields (e.g., from many uncorrelated
 * sources) and implements a form of approximate MVDR beamforming via modal
 * filtering.
 *
 * The filter is constructed by summing contributions from each beam, where each
 * beam:
 * - Suppresses directional modes based on its steering direction,
 * - Applies a nonlinear selectivity function to control beam width,
 * - Adds a base gain for omnidirectional components.
 *
 * @param gm Pointer to a 16-element float array where the 4×4 filter matrix
 * will be stored in row-major order.
 *
 */
void multibeam_t::get_diff_gain(float* gm)
{
  if(bypass)
    return;

  // Initialize the 4×4 filter matrix to zero
  memset(gm, 0, sizeof(float) * 16);

  // Base gain for omnidirectional (W) component
  float diag_gain = mingain;

  // Process each beam
  for(size_t k = 0; k < numbeams; ++k) {
    // Create steering vector in spherical coordinates (unit sphere)
    TASCAR::posf_t psteer;
    psteer.set_sphere(1.0, DEG2RAD * az[k], DEG2RAD * el[k]);

    // Compute "diffuse gain" — how much this beam suppresses diffuse field
    // If selectivity is 1.0, dgain = 0 -> full suppression; if 0.0, dgain = 1
    // -> no suppression
    float dgain = (1.0f - std::min(selectivity[k], 1.0f));
    diag_gain += gain[k] * dgain;

    // Compute selectivity gain: nonlinear function to control beam width
    // The function 1 - exp(-1 / sel^2) ensures sharp roll-off at high
    // selectivity
    float selgain = 1.0f;
    if(selectivity[k] > 0.0f)
      selgain = 1.0f - expf(-1.0f / (selectivity[k] * selectivity[k]));
    // Scale by (1 - dgain) and normalize with 0.5674
    selgain *= (1.0f - dgain) * 0.5674f;

    // Compute directional gains for each FOA mode (W, X, Y, Z)
    float pgainw = gain[k] * selgain; // Gain for W (omnidirectional)
    float pgainy = psteer.y * pgainw; // Gain for Y (Y-directional)
    float pgainz = psteer.z * pgainw; // Gain for Z (Z-directional)
    float pgainx = psteer.x * pgainw; // Gain for X (X-directional)

    // Define the steering vector in FOA mode order: [W, Y, Z, X]
    // This matches the FOA convention used in TASCAR
    float gains[4] = {1.0f, psteer.y, psteer.z, psteer.x};

    // Apply the rank-1 matrix contribution: G_k = selgain * a * a^T
    // where a = [1, y, z, x]^T is the FOA steering vector
    size_t kgain = 0;
    for(size_t r = 0; r < 16; r += 4) {
      float gain = gains[kgain];
      ++kgain;
      gm[r] += pgainw * gain;     // W component
      gm[r + 1] += pgainy * gain; // Y component
      gm[r + 2] += pgainz * gain; // Z component
      gm[r + 3] += pgainx * gain; // X component
    }
  }

  // Add the diagonal gain (omnidirectional component) to the W-channel of each
  // row This ensures minimum output power for diffuse fields
  for(size_t r = 0; r < 16; r += 5)
    gm[r] += diag_gain;

  // Clamp all filter coefficients to [-maxgain, maxgain] to prevent overflow
  for(size_t r = 0; r < 16; ++r)
    gm[r] = std::min(maxgain, std::max(-maxgain, gm[r]));
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
