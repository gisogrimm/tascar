/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

class gate_t : public TASCAR::audioplugin_base_t {
public:
  gate_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~gate_t();

private:
  double tautrack;
  double taurms;
  double threshold;
  double holdlen;
  double fadeinlen;
  double fadeoutlen;
  bool bypass;
  uint32_t ihold;
  uint32_t ifadein;
  uint32_t ifadeout;
  float* pfadein;
  float* pfadeout;
  std::vector<uint32_t> khold;
  std::vector<uint32_t> kfadein;
  std::vector<uint32_t> kfadeout;
  std::vector<double> lmin;
  std::vector<double> lmax;
  std::vector<double> l;
};

gate_t::gate_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), tautrack(30), taurms(0.005), threshold(0.125),
      holdlen(0.125), fadeinlen(0.01), fadeoutlen(0.125), bypass(true),
      ihold(0), ifadein(0), ifadeout(0), pfadein(NULL), pfadeout(NULL)
{
  GET_ATTRIBUTE(tautrack, "s", "Min/max tracking time constant");
  GET_ATTRIBUTE(taurms, "s", "RMS level estimation time constant");
  GET_ATTRIBUTE(threshold, "", "Threshold value between 0 and 1");
  GET_ATTRIBUTE(holdlen, "s",
                "Time to keep output after level decay below threshold");
  GET_ATTRIBUTE(fadeinlen, "s", "Duration of von-Hann fade in");
  GET_ATTRIBUTE(fadeoutlen, "s", "Duration of von-Hann fade out");
  GET_ATTRIBUTE_BOOL(bypass, "Start in bypass mode");
}

void gate_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double("/tautrack", &tautrack);
  srv->add_double("/taurms", &taurms);
  srv->add_double("/threshold", &threshold);
  srv->add_bool("/bypass", &bypass);
  srv->unset_variable_owner();
}

void gate_t::configure()
{
  audioplugin_base_t::configure();
  ifadein = (uint32_t)std::max(1.0, f_sample * fadeinlen);
  ifadeout = (uint32_t)std::max(1.0, f_sample * fadeoutlen);
  pfadein = new float[ifadein];
  pfadeout = new float[ifadeout];
  for(uint32_t k = 0; k < ifadein; ++k)
    pfadein[k] = 0.5f + 0.5f * cosf(TASCAR_PIf * (float)k / (float)ifadein);
  for(uint32_t k = 0; k < ifadeout; ++k)
    pfadeout[k] = 0.5f - 0.5f * cosf(TASCAR_PIf * (float)k / (float)ifadeout);
  ihold = (uint32_t)(f_sample * holdlen);
  lmin.resize(n_channels);
  lmax.resize(n_channels);
  l.resize(n_channels);
  kfadein.resize(n_channels);
  kfadeout.resize(n_channels);
  khold.resize(n_channels);
  for(uint32_t k = 0; k < n_channels; ++k) {
    kfadein[k] = 0;
    kfadeout[k] = 0;
    khold[k] = 0;
  }
}

void gate_t::release()
{
  audioplugin_base_t::release();
  delete[] pfadein;
  delete[] pfadeout;
}

gate_t::~gate_t() {}

void gate_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                        const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                        const TASCAR::transport_t&)
{
  double c1rms(exp(-1.0 / (taurms * f_sample)));
  double c2rms(1.0 - c1rms);
  double c1track(exp(-1.0 / (tautrack * f_sample)));
  double c2track(1.0 - c1track);
  double lthreshold(threshold * threshold);
  for(uint32_t ch = 0; ch < n_channels; ++ch) {
    float* paudio(chunk[ch].d);
    double& lminr(lmin[ch]);
    double& lmaxr(lmax[ch]);
    double& lr(l[ch]);
    uint32_t& kfadeinr(kfadein[ch]);
    uint32_t& kfadeoutr(kfadeout[ch]);
    uint32_t& kholdr(khold[ch]);
    uint32_t n(n_fragment);
    while(n--) {
      lr = lr * c1rms + (*paudio * *paudio) * c2rms;
      if(lr > lminr)
        lminr = lminr * c1track + lr * c2track;
      else
        lminr = lr;
      if(lr < lmaxr)
        lmaxr = lmaxr * c1track + lr * c2track;
      else
        lmaxr = lr;
      if((lr - lminr) > lthreshold * (lmaxr - lminr)) {
        if(!(kfadeoutr || kholdr))
          // fade in only at onset
          kfadeinr = ifadein;
        kfadeoutr = ifadeout;
        kholdr = ihold;
      }
      if(!bypass) {
        if(kfadeinr) {
          kfadeinr--;
          *paudio *= pfadein[kfadeinr];
        } else {
          if(kholdr)
            kholdr--;
          else {
            if(kfadeoutr) {
              kfadeoutr--;
              *paudio *= pfadeout[kfadeoutr];
            } else {
              *paudio = 0.0f;
            }
          }
        }
      }
      ++paudio;
    } // of while()
  }
  // DEBUG(lmin[0]);
  // DEBUG(lmax[0]);
  // DEBUG(l[0]);
}

REGISTER_AUDIOPLUGIN(gate_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
