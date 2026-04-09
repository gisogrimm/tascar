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
#ifndef OSC_SCENE_H
#define OSC_SCENE_H

#include "render.h"

namespace TASCAR {

  class route_solo_p_t {
  public:
    TASCAR::Scene::route_t* route;
    uint32_t* anysolo;
  };

  namespace Scene {

    class osc_scene_t {
    public:
      osc_scene_t(tsccfg::node_t xmlsrc, TASCAR::render_core_t* scene_);
      ~osc_scene_t();
      void add_child_methods(TASCAR::osc_server_t*);

    private:
      void add_object_methods(TASCAR::osc_server_t*,
                              TASCAR::Scene::object_t* o);
      void add_route_methods(TASCAR::osc_server_t*, TASCAR::Scene::route_t* r);
      void add_sound_methods(TASCAR::osc_server_t*, TASCAR::Scene::sound_t* s);
      void add_diffuse_methods(TASCAR::osc_server_t*,
                               TASCAR::Scene::diff_snd_field_obj_t* s);
      void add_receiver_methods(TASCAR::osc_server_t*,
                                TASCAR::Scene::receiver_obj_t* s);
      void add_face_object_methods(TASCAR::osc_server_t*,
                                   TASCAR::Scene::face_object_t* s);
      void add_face_group_methods(TASCAR::osc_server_t*,
                                  TASCAR::Scene::face_group_t* s);
      TASCAR::render_core_t* scene;
      std::vector<route_solo_p_t*> vprs;
    };

  } // namespace Scene

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
