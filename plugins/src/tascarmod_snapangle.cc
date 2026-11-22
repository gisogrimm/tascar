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

#include "session.h"

class snapangle_t : public TASCAR::actor_module_t {
public:
  snapangle_t(const TASCAR::module_cfg_t& cfg);
  ~snapangle_t();
  void update(uint32_t frame, bool running);

private:
  std::string name = "snapangle";
  std::string srcobj;
  std::vector<std::string> candidates;
  std::vector<TASCAR::named_object_t> obj_source;
  std::vector<TASCAR::named_object_t> obj_candidates;
  bool bypass = false;
};

snapangle_t::snapangle_t(const TASCAR::module_cfg_t& cfg)
    : TASCAR::actor_module_t(cfg)
{
  GET_ATTRIBUTE(name, "", "Default name used in OSC variables");
  GET_ATTRIBUTE(srcobj, "", "Path of source object");
  GET_ATTRIBUTE(candidates, "", "Path of target candidates");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass algorithm");
  obj_source = session->find_objects(srcobj);
  if(obj_source.size() != 1)
    throw TASCAR::ErrMsg("Not exactly one source object (srcobj=\"" + srcobj +
                         "\", " + std::to_string(obj_source.size()) +
                         " matches).");
  obj_candidates = session->find_objects(candidates);
  if(!obj_candidates.size())
    throw TASCAR::ErrMsg("No target candidate objects found (candidates=\"" +
                         TASCAR::vecstr2str(candidates) + "\").");
  std::string path = std::string("/") + name;
  cfg.session->add_bool(path + "/bypass", &bypass);
}

snapangle_t::~snapangle_t() {}

void snapangle_t::update(uint32_t, bool)
{
  if(bypass)
    return;
  TASCAR::pos_t dir(1.0, 0.0, 0.0);
  TASCAR::pos_t srcpos;
  TASCAR::rotmat_t srcdir;
  obj_source[0].obj->get_6dof(srcpos, srcdir);
  dir *= srcdir;
  TASCAR::pos_t dirmin;
  float dmin = 2.0f;
  for(auto& obj : obj_candidates) {
    TASCAR::pos_t candpos = obj.obj->get_location();
    candpos -= srcpos;
    candpos.normalize();
    float d = distancef(candpos, dir);
    if(d < dmin) {
      dmin = d;
      dirmin = candpos;
    }
  }
  dirmin /= obj_source[0].obj->c6dof_nodelta.orientation;
  TASCAR::zyx_euler_t eul;
  eul.set_from_pos(dirmin);
  set_orientation(eul);
}

REGISTER_MODULE(snapangle_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
