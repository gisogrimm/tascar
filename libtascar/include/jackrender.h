/* License (GPL)
 *
 * Copyright (C) 2014,2015,2016,2017,2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef JACKRENDER_H
#define JACKRENDER_H

#include "osc_scene.h"
#include "render.h"

namespace TASCAR {

  class scene_render_rt_t : public TASCAR::render_core_t,
                            public TASCAR::Scene::osc_scene_t,
                            public jackc_transport_t {
  public:
    scene_render_rt_t(tsccfg::node_t xmlsrc, const std::string& parentname);
    virtual ~scene_render_rt_t();
    void run(bool& b_quit);
    void start();
    void stop();

  private:
    // jack callback:
    int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
                const std::vector<float*>& outBuffer, uint32_t tp_frame,
                bool tp_rolling);
  };

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
