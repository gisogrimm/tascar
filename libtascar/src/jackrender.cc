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

#include "jackrender.h"
#include <string.h>
#include <unistd.h>

std::string strrep(std::string s, const std::string& pat,
                   const std::string& rep)
{
  std::string out_string("");
  std::string::size_type len = pat.size();
  std::string::size_type pos;
  while((pos = s.find(pat)) < s.size()) {
    out_string += s.substr(0, pos);
    out_string += rep;
    s.erase(0, pos + len);
  }
  s = out_string + s;
  return s;
}

TASCAR::scene_render_rt_t::scene_render_rt_t(tsccfg::node_t xmlsrc,
                                             const std::string& parentname)
    : render_core_t(xmlsrc), osc_scene_t(xmlsrc, this),
      jackc_transport_t(jacknamer(parentname, name))
{
}

TASCAR::scene_render_rt_t::~scene_render_rt_t()
{
  if(jackc_t::active)
    jackc_t::deactivate();
}

/**
   \ingroup callgraph
 */
int TASCAR::scene_render_rt_t::process(jack_nframes_t nframes,
                                       const std::vector<float*>& inBuffer,
                                       const std::vector<float*>& outBuffer,
                                       uint32_t tp_frame, bool tp_rolling)
{
  TASCAR::transport_t tp;
  tp.rolling = tp_rolling;
  tp.session_time_samples = tp_frame;
  tp.session_time_seconds = (double)tp_frame / (double)srate;
  tp.object_time_samples = tp_frame;
  tp.object_time_seconds = (double)tp_frame / (double)srate;
  render_core_t::process(nframes, tp, inBuffer, outBuffer);
  return 0;
}

void TASCAR::scene_render_rt_t::start()
{
  chunk_cfg_t cf(get_srate(), get_fragsize());
  // first prepare all nodes for audio processing:
  prepare(cf);
  post_prepare();
  try {
    // create all ports:
    for(std::vector<std::string>::iterator iip = input_ports.begin();
        iip != input_ports.end(); ++iip)
      add_input_port(*iip);
    for(std::vector<std::string>::iterator iop = output_ports.begin();
        iop != output_ports.end(); ++iop)
      add_output_port(*iop);
    add_input_port("sync_in");
  }
  catch(...) {
    release();
    throw;
  }
  try {
    jackc_t::activate();
    // osc_server_t::activate();
    //  connect jack ports of point sources:
    for(unsigned int k = 0; k < sounds.size(); k++) {
      std::vector<std::string> cn(sounds[k]->get_connect());
      for(auto it = cn.begin(); it != cn.end(); ++it)
        *it = strrep(*it, "@",
                     "player." + name + ":" + sounds[k]->get_parent_name());
      if(cn.size()) {
        std::vector<std::string> cno(get_port_names_regexp(cn));
        if(!cno.size())
          throw TASCAR::ErrMsg("Port list \"" + TASCAR::vecstr2str(cn) +
                               "\" did not match any port.");
        for(auto it = cno.begin(); it != cno.end(); ++it)
          if(it->size())
            connect_in(sounds[k]->get_port_index(), *it, true);
      }
      // connect diffuse ports:
      for(std::vector<TASCAR::Scene::diff_snd_field_obj_t*>::iterator idiff =
              diff_snd_field_objects.begin();
          idiff != diff_snd_field_objects.end(); ++idiff) {
        TASCAR::Scene::diff_snd_field_obj_t* pdiff(*idiff);
        std::vector<std::string> cn(pdiff->get_connect());
        for(auto it = cn.begin(); it != cn.end(); ++it)
          *it = strrep(*it, "@", "player." + name + ":" + pdiff->get_name());
        // cn = get_port_names_regexp( cn );
        uint32_t pi(pdiff->get_port_index());
        for(auto it = cn.begin(); it != cn.end(); ++it)
          if(it->size()) {
            for(uint32_t k = 0; k < 4; ++k) {
              char ctmp[1024];
              ctmp[1023] = 0;
              snprintf(ctmp, 1023, "%s.%u", it->c_str(), k);
              connect_in(pi + k, ctmp, true);
            }
          }
      }
    }
    // connect receiver ports:
    for(unsigned int k = 0; k < receivermod_objects.size(); k++) {
      std::vector<std::string> cn(receivermod_objects[k]->get_connect());
      for(auto it = cn.begin(); it != cn.end(); ++it)
        if(it->size()) {
          for(uint32_t ch = 0; ch < receivermod_objects[k]->n_channels; ch++)
            connect_out(receivermod_objects[k]->get_port_index() + ch,
                        *it + receivermod_objects[k]->labels[ch], true);
        }
      std::vector<std::string> cns(receivermod_objects[k]->get_connections());
      for(uint32_t kc = 0; kc < std::min((uint32_t)(cns.size()),
                                         receivermod_objects[k]->n_channels);
          kc++) {
        if(cns[kc].size())
          connect_out(receivermod_objects[k]->get_port_index() + kc, cns[kc],
                      true);
      }
    }
  }
  catch(...) {
    jackc_t::deactivate();
    release();
    throw;
  }
}

void TASCAR::scene_render_rt_t::stop()
{
  jackc_t::deactivate();
  release();
}

void TASCAR::scene_render_rt_t::run(bool& b_quit)
{
  start();
  while(!b_quit) {
    usleep(50000);
    getchar();
    if(feof(stdin))
      b_quit = true;
  }
  stop();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
