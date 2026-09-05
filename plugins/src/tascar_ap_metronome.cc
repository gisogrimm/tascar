/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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
#include "filterclass.h"

static int osc_set_message(const char*, const char*, lo_arg**, int,
                           lo_message msg, void* user_data)
{
  if(user_data) {
    TASCAR::msg_t* tmsg((TASCAR::msg_t*)user_data);
    lo_message_free(tmsg->msg);
    tmsg->msg = lo_message_clone(msg);
  }
  return 0;
}

class metronome_t : public TASCAR::audioplugin_base_t {
public:
  metronome_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~metronome_t();

private:
  void update_par();
  bool changeonone = false;
  double bpm = 120;
  std::vector<int32_t> bpb = {4}; //< beats per bar, for at least one bar
  double a1 = 0.002;
  double ao = 0.001;
  double fres1 = 1000;
  double freso = 600;
  double q1 = 0.997;
  double qo = 0.997;
  bool sync = false;
  bool bypass = false;
  bool bypass_ = false;
  int64_t t = 0;
  int64_t beat = 0;
  int64_t bar = 0;
  TASCAR::resonance_filter_t f1;
  TASCAR::resonance_filter_t fo;
  std::vector<int32_t> bpb_ = {4};
  double a1_ = 0.002;
  double ao_ = 0.001;
  int64_t period = 0; //< number of audio samples per beat
  uint32_t dispatchin = 0;
  TASCAR::osc_server_t* srv_ = NULL;
  TASCAR::msg_t msg;
};

metronome_t::metronome_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), msg(find_or_add_child("msg"))
{
  GET_ATTRIBUTE_BOOL(changeonone, "Apply OSC parameter changes on next bar");
  GET_ATTRIBUTE(bpm, "", "Beats per minute");
  GET_ATTRIBUTE(bpb, "", "Beats per bar");
  GET_ATTRIBUTE_DBSPL(a1, "Amplitude of first beat");
  GET_ATTRIBUTE_DBSPL(ao, "Amplitude of other beats");
  GET_ATTRIBUTE_BOOL(sync, "Use object time synchronization");
  GET_ATTRIBUTE(fres1, "Hz", "Resonance frequency of first beat");
  GET_ATTRIBUTE(freso, "Hz", "Resonance frequency of other beats");
  GET_ATTRIBUTE(q1, "", "Filter resonance of first beat");
  GET_ATTRIBUTE(qo, "", "Filter resonance of other beats");
  GET_ATTRIBUTE_BOOL(bypass, "Load in bypass mode");
  if(bpb.empty())
    throw TASCAR::ErrMsg("At least one bar needs to be specified (bpb).");
  for(auto b : bpb)
    if(b < 1)
      throw TASCAR::ErrMsg("A bar needs to contain at least one beat.");
}

void metronome_t::configure()
{
  audioplugin_base_t::configure();
  update_par();
}

void metronome_t::release()
{
  audioplugin_base_t::release();
}

void metronome_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv_ = srv;
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/changeonone", &changeonone,"If true, parameter changes (like BPM or volume updates) are applied only on the next \"one\" (the first beat of a bar). If false, changes are applied immediately.");
  srv->add_double("/bpm", &bpm,"","Beats per minute. Defaults to 120.");
  srv->add_vector_int("/bpb", &bpb,"","Beats per bar. This is a vector allowing for varying bar lengths (e.g., \"4 3\" for a 4/4 bar followed by a 3/4 bar).");
  srv->add_double_dbspl("/a1", &a1,"","Amplitude of the first beat (the \"downbeat\").");
  srv->add_double_dbspl("/ao", &ao,"","Amplitude of the other beats.");
  srv->add_bool("/sync", &sync,"Enables synchronization to the transport time line. If false (default), the metronome runs freely based on the audio sample rate.");
  srv->add_double("/filter/f1", &fres1,"","Resonance frequency of the first beat.");
  srv->add_double("/filter/fo", &freso,"","Resonance frequency of other beats.");
  srv->add_double("/filter/q1", &q1,"","Filter resonance (Q factor) of the first beat.");
  srv->add_double("/filter/qo", &qo,"","Filter resonance (Q factor) of other beats.");
  srv->add_bool("/bypass", &bypass,"If true, the metronome sound is muted.");
  srv->add_uint("/dispatchin", &dispatchin,"","A counter used to trigger an OSC message dispatch on a specific beat. (See detailed explanation below).");
  srv->add_method("/dispatchmsg", NULL, &osc_set_message, &msg,true,false,"","An OSC method that accepts a generic message. The data sent here is stored and used later by the dispatch mechanism.");
  srv->add_string("/dispatchpath", &(msg.path),"The OSC path (address) that the stored message will be sent to when the dispatch is triggered.");
  srv->unset_variable_owner();
}

metronome_t::~metronome_t() {}

void metronome_t::update_par()
{
  bypass_ = bypass;
  f1.set_fq(fres1 * t_sample, q1);
  fo.set_fq(freso * t_sample, qo);
  if(bpm < 6.9444e-04)
    bpm = 6.9444e-04;
  period = (60 * (int64_t)f_sample) / (int64_t)bpm;
  ao_ = ao;
  a1_ = a1;
  bpb_ = bpb;
}

void metronome_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                             const TASCAR::pos_t&, const TASCAR::zyx_euler_t&,
                             const TASCAR::transport_t& tp)
{
  // apply parameters:
  if(!changeonone)
    update_par();
  float v(0);
  size_t Nch(chunk.size());
  uint32_t Nsmp(0);
  if(Nch > 0)
    Nsmp = chunk[0].n;
  // TASCAR::wave_t& aud(chunk[0]);
  int64_t objecttime(tp.object_time_samples);
  for(uint32_t k = 0; k < Nsmp; ++k) {
    if(sync) {
      // synchronize to time line:
      t = objecttime;
      if(tp.rolling)
        ++objecttime;
      ldiv_t tmp(ldiv(t, period));
      t = tmp.rem;
      beat = tmp.quot;
      if(bpb_[bar] > 0) {
        tmp = ldiv(beat, bpb_[bar]);
        beat = tmp.rem;
      } else {
        beat = 0;
        ++bar;
        if(bar >= (int64_t)(bpb_.size()))
          bar = 0;
      }
    } else {
      // free-run mode, in
      if(t >= period) {
        t = 0;
        ++beat;
        if(beat >= bpb_[bar]) {
          beat = 0;
          ++bar;
          if(bar >= (int64_t)(bpb_.size()))
            bar = 0;
        }
      }
    }
    if(t || (sync && !tp.rolling)) {
      // this is not a beat sample:
      v = 0.0f;
    } else {
      // now we are exactly on a beat.
      // if beat != 0 then this is not a one:
      if(beat)
        v = ao_;
      else {
        if(dispatchin) {
          // count down "dispatchin". If zero, then dispatch message.
          --dispatchin;
          if(dispatchin > (1 << 16))
            dispatchin = 0u;
          else if((!dispatchin) && srv_)
            srv_->dispatch_data_message(msg.path.c_str(), msg.msg);
        }
        if(changeonone)
          update_par();
        v = a1_;
      }
    }
    if(!bypass_) {
      // beat-dependent filter properties:
      if(beat)
        v = fo.filter_unscaled(v);
      else
        v = f1.filter_unscaled(v);
      for(uint32_t c = 0; c < Nch; c++)
        chunk[c].d[k] += v;
    }
    ++t;
  }
}

REGISTER_AUDIOPLUGIN(metronome_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
