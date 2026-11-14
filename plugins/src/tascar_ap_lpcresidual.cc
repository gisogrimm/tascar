/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 */

#include "acousticmodel.h"
#include "audioplugin.h"
#include "errorhandling.h"
#include <algorithm>
#include <cmath>
#include <vector>

class lpc_residual_t : public TASCAR::audioplugin_base_t {
public:
  lpc_residual_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~lpc_residual_t();

private:
  int lpcorder = 12; // Default number of coefficients

  TASCAR::lpc_t lpc;
  TASCAR::wave_t sigcpy;
  std::vector<TASCAR::wave_t> prev_x;
};

lpc_residual_t::lpc_residual_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), lpc(1), sigcpy(1)
{
  // Make the number of coefficients configurable via attribute "lpcorder"
  GET_ATTRIBUTE(lpcorder, "", "LPC filter order");
}

void lpc_residual_t::add_variables(TASCAR::osc_server_t*)
{
  // OSC control removed as per request
}

lpc_residual_t::~lpc_residual_t() {}

void lpc_residual_t::configure()
{
  audioplugin_base_t::configure();
  lpc.update_p(lpcorder);
  sigcpy.resize(n_fragment);
  prev_x.resize(n_channels);
  for(auto& px : prev_x)
    px.resize(n_fragment);
}

void lpc_residual_t::release()
{
  audioplugin_base_t::release();
}

void lpc_residual_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                const TASCAR::pos_t&,
                                const TASCAR::zyx_euler_t&,
                                const TASCAR::transport_t&)
{
  if(chunk.empty())
    return;

  uint32_t nch = chunk.size();
  uint32_t N = chunk[0].n;
  if(N == 0)
    return;

  sigcpy.copy(chunk[0]);

  lpc.calc(sigcpy, chunk[0], prev_x[0]);
  for(uint32_t ch = 1; ch < nch; ++ch) {
    sigcpy.copy(chunk[ch]);
    lpc.analyse(sigcpy, chunk[ch], prev_x[ch]);
  }
}

REGISTER_AUDIOPLUGIN(lpc_residual_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
