/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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
#include <lsl_cpp.h>

class pos2lsl_t : public TASCAR::module_base_t {
public:
  pos2lsl_t(const TASCAR::module_cfg_t& cfg);
  ~pos2lsl_t();
  virtual void configure();
  void update(uint32_t frame, bool running);

private:
  std::string pattern;
  bool transport;
  std::vector<TASCAR::named_object_t> obj;
  std::vector<lsl::stream_outlet*> voutlet;
  std::vector<float> data;
};

pos2lsl_t::pos2lsl_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), transport(true)
{
  data.resize(6);
  GET_ATTRIBUTE_(pattern);
  GET_ATTRIBUTE_BOOL_(transport);
  if(pattern.empty())
    pattern = "/*/*";
  obj = session->find_objects(pattern);
  if(!obj.size())
    throw TASCAR::ErrMsg("No target objects found (target pattern: \"" +
                         pattern + "\").");
}

void pos2lsl_t::configure()
{
  module_base_t::configure();
  for(auto it = voutlet.begin(); it != voutlet.end(); ++it)
    delete *it;
  voutlet.clear();
  for(auto it = obj.begin(); it != obj.end(); ++it)
    voutlet.push_back(new lsl::stream_outlet(
        lsl::stream_info(it->obj->get_name(), "position", 6, f_fragment)));
}

pos2lsl_t::~pos2lsl_t()
{
  for(uint32_t k = 0; k < voutlet.size(); ++k)
    delete voutlet[k];
}

void pos2lsl_t::update(uint32_t, bool tp_rolling)
{
  if(tp_rolling || (!transport)) {
    uint32_t kobj(0);
    for(auto it = obj.begin(); it != obj.end(); ++it) {
      const TASCAR::pos_t& p(it->obj->c6dof.position);
      auto o = it->obj->c6dof.orientation.to_euler();
      // it->obj->get_6dof(p,o);
      data[0] = p.x;
      data[1] = p.y;
      data[2] = p.z;
      data[3] = RAD2DEG * o.z;
      data[4] = RAD2DEG * o.y;
      data[5] = RAD2DEG * o.x;
      voutlet[kobj]->push_sample(data);
      ++kobj;
    }
  }
}

REGISTER_MODULE(pos2lsl_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
