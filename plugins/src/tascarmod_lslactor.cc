/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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
#include <lsl_cpp.h>
#include <thread>

class lslactor_t : public TASCAR::actor_module_t {
public:
  lslactor_t(const TASCAR::module_cfg_t& cfg);
  ~lslactor_t();
  void update(uint32_t frame, bool running);
  void service();

private:
  std::string predicate;
  std::vector<int32_t> channels;
  std::vector<double> influence;
  lsl::stream_inlet* inlet;
  std::thread srv;
  bool run_service;
  bool local;
  bool incremental;
  TASCAR::c6dof_t transform;
  TASCAR::pos_t pos;
  TASCAR::zyx_euler_t eul;
  std::vector<double> sync_sample;
  std::mutex mtx;
};

lslactor_t::lslactor_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg), inlet(NULL), run_service(true), local(false),
      incremental(false)
{
  GET_ATTRIBUTE_(predicate);
  GET_ATTRIBUTE_(channels);
  GET_ATTRIBUTE_(influence);
  GET_ATTRIBUTE_BOOL_(local);
  GET_ATTRIBUTE_BOOL_(incremental);
  if(channels.empty())
    throw TASCAR::ErrMsg("No channels selected.");
  if(channels.size() > 6)
    throw TASCAR::ErrMsg("More than 6 channels selected.");
  for(uint32_t k = channels.size(); k < 6; ++k)
    channels.push_back(-1);
  influence.resize(channels.size());
  bool valid(false);
  for(uint32_t k = 0; k < channels.size(); ++k)
    if((channels[k] > -1) && (influence[k] != 0))
      valid = true;
  if(!valid)
    throw TASCAR::ErrMsg("No channel has a non-zero influence.");
  srv = std::thread(&lslactor_t::service, this);
}

void lslactor_t::service()
{
  inlet = NULL;
  while((!inlet) && run_service) {
    std::vector<lsl::stream_info> results(lsl::resolve_stream(predicate, 1, 1));
    if(results.empty())
      TASCAR::add_warning("No matching LSL stream found (" + predicate + ").");
    else {
      lsl::channel_format_t cf(results[0].channel_format());
      if(!((cf == lsl::cf_float32) || (cf == lsl::cf_double64)))
        TASCAR::add_warning("The LSL stream \"" + predicate +
                            "\" has no floating point data.");
      else {
        int32_t streamchannels(results[0].channel_count());
        for(uint32_t k = 0; k < channels.size(); ++k)
          if((channels[k] > -1) && (channels[k] >= streamchannels)) {
            char ctmp[1024];
            ctmp[1023] = 0;
            snprintf(ctmp, 1023,
                     "The %dth entry of channel vector requires channel %d, "
                     "but the LSL stream \"%s\" has only %d channels.",
                     k, channels[k], predicate.c_str(), streamchannels);
            TASCAR::add_warning(ctmp);
          } else {
            inlet = new lsl::stream_inlet(results[0]);
          }
      }
    }
  }
  std::vector<double> sample;
  while(run_service && inlet) {
    double t(inlet->pull_sample(sample, 0.1));
    if(t != 0) {
      std::lock_guard<std::mutex> lock(mtx);
      sync_sample = sample;
    }
  }
}

lslactor_t::~lslactor_t()
{
  run_service = false;
  srv.join();
  if(inlet)
    delete inlet;
}

void lslactor_t::update(uint32_t, bool)
{
  if(mtx.try_lock()) {
    if((channels[0] > -1) && (sync_sample.size() > 0))
      pos.x = sync_sample[channels[0]] * influence[0];
    if((channels[1] > -1) && (sync_sample.size() > 1))
      pos.y = sync_sample[channels[1]] * influence[1];
    if((channels[2] > -1) && (sync_sample.size() > 2))
      pos.z = sync_sample[channels[2]] * influence[2];
    if((channels[3] > -1) && (sync_sample.size() > 3))
      eul.z = sync_sample[channels[3]] * influence[3];
    if((channels[4] > -1) && (sync_sample.size() > 4))
      eul.y = sync_sample[channels[4]] * influence[4];
    if((channels[5] > -1) && (sync_sample.size() > 5))
      eul.x = sync_sample[channels[5]] * influence[5];
    transform.position = pos;
    transform.orientation = eul;
    // if(channels[0] > -1)
    //   transform.position.x = sample[channels[0]] * influence[0];
    // if(channels[1] > -1)
    //   transform.position.y = sample[channels[1]] * influence[1];
    // if(channels[2] > -1)
    //   transform.position.z = sample[channels[2]] * influence[2];
    // if(channels[3] > -1)
    //   transform.orientation.z = sample[channels[3]] * influence[3];
    // if(channels[4] > -1)
    //   transform.orientation.y = sample[channels[4]] * influence[4];
    // if(channels[5] > -1)
    //   transform.orientation.x = sample[channels[5]] * influence[5];
    mtx.unlock();
  }
  if(incremental)
    add_transformation(transform, local);
  else
    set_transformation(transform, local);
}

REGISTER_MODULE(lslactor_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
