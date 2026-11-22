/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "async_file.h"
#include "audioplugin.h"
#include "errorhandling.h"
#include "levelmeter.h"
#include <fstream>

class ap_sndfile_async_cfg_t : public TASCAR::audioplugin_base_t {
public:
  ap_sndfile_async_cfg_t(const TASCAR::audioplugin_cfg_t& cfg);

protected:
  std::string name;
  uint32_t channel;
  // double start;
  double position;
  // double length;
  double caliblevel;
  uint32_t loop;
  bool transport;
  bool mute;
  std::string license;
  std::string attribution;
};

ap_sndfile_async_cfg_t::ap_sndfile_async_cfg_t(
    const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), channel(0),
      // start(0),
      position(0),
      // length(0),
      caliblevel(1), loop(1), transport(true), mute(false)
{
  GET_ATTRIBUTE_DBSPL(caliblevel, "Calibration level");

  GET_ATTRIBUTE(name, "", "Sound file name");
  GET_ATTRIBUTE(channel, "", "First sound file channel to be used, zero-base");
  GET_ATTRIBUTE(position, "s", "Start position within the scene");
  GET_ATTRIBUTE(loop, "", "loop count or 0 for infinite looping");
  GET_ATTRIBUTE_BOOL(transport, "Use session time base");
  GET_ATTRIBUTE_BOOL(mute, "Load muted");
}

class ap_sndfile_async_t : public ap_sndfile_async_cfg_t {
public:
  ap_sndfile_async_t(const TASCAR::audioplugin_cfg_t& cfg);
  ~ap_sndfile_async_t();
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void add_licenses(licensehandler_t* session);
  void configure();
  void release();

private:
  TASCAR::transport_t ltp;
  TASCAR::async_sndfile_t* sndf;
};

ap_sndfile_async_t::ap_sndfile_async_t(const TASCAR::audioplugin_cfg_t& cfg)
    : ap_sndfile_async_cfg_t(cfg)
{
  get_license_info(e, name, license, attribution);
}

void ap_sndfile_async_t::configure()
{
  TASCAR::audioplugin_base_t::configure();
  if(n_channels < 1)
    throw TASCAR::ErrMsg("At least one channel required.");
  sndf = new TASCAR::async_sndfile_t(
      n_channels, TASCAR::config("tascar.sndfile.bufferlength", 1 << 18),
      n_fragment);
  sndf->open(name, channel, position * f_sample, caliblevel, loop);
  if(sndf->get_srate() != f_sample) {
    std::string msg("The sample rate of the sound file \"" + name +
                    "\" differs from the session sample rate:\n");
    char ctmp[1024];
    ctmp[1023] = 0;
    snprintf(ctmp, 1023, "  file has %d Hz, expected %g Hz", sndf->get_srate(),
             f_sample);
    msg += ctmp;
    TASCAR::add_warning(msg, e);
  }
  sndf->start_service();
}

void ap_sndfile_async_t::release()
{
  sndf->stop_service();
  TASCAR::audioplugin_base_t::release();
  delete sndf;
}

ap_sndfile_async_t::~ap_sndfile_async_t() {}

void ap_sndfile_async_t::add_licenses(licensehandler_t* session)
{
  audioplugin_base_t::add_licenses(session);
  session->add_license(license, attribution,
                       TASCAR::tscbasename(TASCAR::env_expand(name)));
}

void ap_sndfile_async_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/mute", &mute);
  srv->unset_variable_owner();
}

void ap_sndfile_async_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                    const TASCAR::pos_t&,
                                    const TASCAR::rotmat_t&,
                                    const TASCAR::transport_t& tp)
{
  if(transport)
    ltp = tp;
  else {
    ltp.object_time_samples += chunk[0].n;
    ltp.rolling = true;
  }
  float* dp[chunk.size()];
  for(uint32_t ch = 0; ch < chunk.size(); ch++)
    dp[ch] = chunk[ch].d;
  sndf->request_data(ltp.object_time_samples,
                     n_fragment * ((!mute) && (tp.rolling || (!transport))),
                     chunk.size(), dp);
}

REGISTER_AUDIOPLUGIN(ap_sndfile_async_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
