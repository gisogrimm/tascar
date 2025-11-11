/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
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

#include "session.h"
#include <ltc.h>
#include <string.h>
#include <sys/time.h>

class ltcgen_t : public TASCAR::module_base_t, public jackc_transport_t {
public:
  ltcgen_t(const TASCAR::module_cfg_t& cfg);
  virtual ~ltcgen_t();
  virtual int process(jack_nframes_t, const std::vector<float*>&,
                      const std::vector<float*>&, uint32_t tp_frame,
                      bool tp_rolling);

private:
  double fpsnum = 25;
  double fpsden = 1;
  double volume = -18;
  double addtime = 0.0;
  bool usewallclock = false;
  std::vector<std::string> connect;
  LTCEncoder* encoder = NULL;
  ltcsnd_sample_t* enc_buf = NULL;
  //
  uint32_t encoded_data = 0;
  uint32_t byteCnt = 0;
  ltcsnd_sample_t* enc_buf_;
  uint32_t lastframe = -1;
#ifndef _WIN32
  struct timeval tv;
#endif
};

ltcgen_t::ltcgen_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), jackc_transport_t("ltc." + session->name),
      enc_buf_(enc_buf)
{
  GET_ATTRIBUTE(fpsnum, "", "Frames per second, numerator");
  GET_ATTRIBUTE(fpsden, "", "Frames per second, denominator");
  GET_ATTRIBUTE(volume, "dB re FS", "Signal volume");
  GET_ATTRIBUTE(connect, "", "Space-separated list of output port connections");
  GET_ATTRIBUTE(addtime, "s", "Add time, e.g., for time zone compensation");
  GET_ATTRIBUTE_BOOL(usewallclock,
                     "Use wallclock time instead of session time");
  LTC_TV_STANDARD ltc_tv_standard = LTC_TV_625_50;
  switch((int32_t)(fpsnum / fpsden + 0.5)) {
  case 30:
    ltc_tv_standard = LTC_TV_525_60;
    break;
  case 25:
    ltc_tv_standard = LTC_TV_625_50;
    break;
  case 24:
    ltc_tv_standard = LTC_TV_FILM_24;
    break;
  default:
    TASCAR::add_warning("Non-standard LTC frame rate: " +
                        TASCAR::to_string(fpsnum) + "/" +
                        TASCAR::to_string(fpsden) + " fps");
  }
  encoder =
      ltc_encoder_create(get_srate(), fpsnum / fpsden, ltc_tv_standard, 0);
  enc_buf = new ltcsnd_sample_t[ltc_encoder_get_buffersize(encoder)];
  memset(enc_buf, 0,
         ltc_encoder_get_buffersize(encoder) * sizeof(ltcsnd_sample_t));
  add_output_port("ltc");
  activate();
  for(std::vector<std::string>::const_iterator it = connect.begin();
      it != connect.end(); ++it)
    jackc_transport_t::connect(get_client_name() + ":ltc", *it, true);
}

ltcgen_t::~ltcgen_t()
{
  deactivate();
  ltc_encoder_free(encoder);
}

int ltcgen_t::process(jack_nframes_t n, const std::vector<float*>&,
                      const std::vector<float*>& sOut, uint32_t tp_frame,
                      bool tp_rolling)
{
  float smult = pow(10, volume / 20.0) / (90.0);
  if(tp_frame != lastframe + n) {
    double sec(tp_frame * t_sample);
    if(usewallclock) {
#ifndef _WIN32
      gettimeofday(&tv, NULL);
      struct tm* caltime = localtime(&(tv.tv_sec));
      double p_hour = caltime->tm_hour;
      double p_min = caltime->tm_min;
      double p_sec = caltime->tm_sec;
      p_sec += 0.000001 * tv.tv_usec;
#else
      SYSTEMTIME caltime;
      GetLocalTime(&caltime);
      double p_hour = caltime.wHour;
      double p_min = caltime.wMinute;
      double p_sec = caltime.wSecond;
      p_sec += 0.001 * caltime.wMilliseconds;
#endif
      sec = p_sec + 60 * p_min + 3600 * p_hour;
    }
    sec += addtime;
    SMPTETimecode st;
    memset(st.timezone, 0, 6);
    st.years = 0;
    st.months = 0;
    st.days = 0;
    st.hours = (int)floor(sec / 3600.0);
    st.mins = (int)floor((sec - 3600.0 * floor(sec / 3600.0)) / 60.0);
    st.secs = (int)floor(sec) % 60;
    st.frame = (int)floor((sec - floor(sec)) * (double)fpsnum / (double)fpsden);
    ltc_encoder_set_timecode(encoder, &st);
  }
  lastframe = tp_frame;
  if(sOut.size()) {
    for(uint32_t k = 0; k < n; ++k) {
      if(!encoded_data) {
        if(byteCnt >= 10) {
          byteCnt = 0;
          if(tp_rolling)
            ltc_encoder_inc_timecode(encoder);
        }
        ltc_encoder_encode_byte(encoder, byteCnt, 1.0);
        byteCnt++;
        //encoded_data = ltc_encoder_get_buffer(encoder, enc_buf);
        encoded_data = ltc_encoder_copy_buffer(encoder, enc_buf);
        enc_buf_ = enc_buf;
      }
      if(encoded_data) {
        sOut[0][k] = smult * ((float)(*enc_buf_) - 128.0f);
        ++enc_buf_;
        --encoded_data;
      }
    }
  }
  return 0;
}

REGISTER_MODULE(ltcgen_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
