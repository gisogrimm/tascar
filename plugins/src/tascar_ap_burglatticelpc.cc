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

// see also:
// Schnell, K. (2008). Time-varying Burg method for speech
// analysis. 2008 16th European Signal Processing Conference, 1–5.

// This code implements a whitening algorithm using a lattice filter
// and the Burg-method. This implementation assumes stationary
// signals, but is calculating the estimate function using sliding
// averages, and thus can adapt to changing signals implicitely.

class burg_lattice_section_t {
public:
  burg_lattice_section_t(){};
  inline void update_samples(float& x_f, float& x_b, float expectedvalcoeff)
  {
    // update inputs:
    o = x_f;
    u = x_b_prev;
    x_b_prev = x_b;
    // update coefficients:
    float expectedvalcoeff1 = 1.0f - expectedvalcoeff;
    E_ou *= expectedvalcoeff;
    E_ou += expectedvalcoeff1 * o * u;
    E_o2_u2 *= expectedvalcoeff;
    E_o2_u2 += expectedvalcoeff1 * (o * o + u * u);
    if(E_o2_u2 > 0.0f)
      r = -2.0f * E_ou / E_o2_u2;
    else
      r = -1.0f;
    // update outputs:
    x_f += r * u;
    x_b = u + r * o;
  }

  // private:
  float o = 0.0f;
  float u = 0.0f;
  float x_b_prev = 0.0f;
  float r = -1.0f;
  float E_ou = 0.0f;
  float E_o2_u2 = 0.0f;
};

class bllpc_t : public TASCAR::audioplugin_base_t {
public:
  bllpc_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~bllpc_t();

private:
  std::vector<std::vector<burg_lattice_section_t>> clattice;
  uint32_t n = 1;
  float coeff = 0.99;
  bool outerr = false;
};

bllpc_t::bllpc_t(const TASCAR::audioplugin_cfg_t& cfg) : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(n, "", "Number of lattice sections");
  GET_ATTRIBUTE(coeff, "", "Filter coefficient for expectation value filter");
  GET_ATTRIBUTE_BOOL(outerr, "Output error signal");
}

void bllpc_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/coeff", &coeff);
  srv->add_bool("/outerr", &outerr);
  srv->unset_variable_owner();
}

void bllpc_t::configure()
{
  audioplugin_base_t::configure();
  clattice.resize(n_channels);
  for(auto& lattice : clattice) {
    lattice.clear();
    lattice.resize(n);
  }
}

void bllpc_t::release()
{
  audioplugin_base_t::release();
}

bllpc_t::~bllpc_t() {}

void bllpc_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                         const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                         const TASCAR::transport_t&)
{
  size_t ch = 0;
  for(auto& lattice : clattice) {
    auto& w = chunk[ch];
    for(size_t k = 0; k < w.n; ++k) {
      float x_f = w.d[k];
      float x_b = w.d[k];
      for(auto& latsection : lattice)
        latsection.update_samples(x_f, x_b, coeff);
      if(outerr)
        w.d[k] = x_b;
      else
        w.d[k] = x_f;
    }
    ++ch;
  }
  // DEBUG(clattice[0][0].E_ou);
  for(auto& lattice : clattice[0])
    std::cout << lattice.r << " ";
  std::cout << ";..." << std::endl;
}

REGISTER_AUDIOPLUGIN(bllpc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
