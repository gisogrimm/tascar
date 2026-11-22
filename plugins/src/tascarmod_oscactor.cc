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
#include <thread>

class oscactor_t : public TASCAR::actor_module_t {
public:
  oscactor_t(const TASCAR::module_cfg_t& cfg);
  ~oscactor_t();
  void update(uint32_t frame, bool running);

private:
  std::string prefix = "";
  std::string path;
  uint32_t inputchannels = 6;
  std::vector<int32_t> channels;
  std::vector<float> influence;
  std::vector<float> data;
  bool local = false;
  bool incremental = false;
  TASCAR::c6dof_t transform;
  TASCAR::pos_t pos;
  TASCAR::zyx_euler_t eul;
};

oscactor_t::oscactor_t(const TASCAR::module_cfg_t& cfg) : actor_module_t(cfg)
{
  GET_ATTRIBUTE(prefix, "", "OSC prefix for influence variable");
  GET_ATTRIBUTE(path, "", "OSC path");
  GET_ATTRIBUTE(inputchannels, "", "Number of OSC channels");
  GET_ATTRIBUTE(channels, "", "Which channels to use");
  GET_ATTRIBUTE(influence, "",
                "Influence of OSC values on the selected movement channels");
  GET_ATTRIBUTE_BOOL(local, "Use transformations in local coordinates");
  GET_ATTRIBUTE_BOOL(incremental,
                     "Add transformation to current delta transformation, "
                     "e.g., when used together with other motion controllers");
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
  data = std::vector<float>(inputchannels, 0.0f);
  session->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  session->add_vector_float(path.c_str(), &data, "", "OSC data variable");
  std::string ipath = prefix + path;
  ipath += "/influence";
  session->add_vector_float(
      ipath.c_str(), &influence, "",
      "Influence of OSC values on the selected movement channels");
  session->unset_variable_owner();
}

oscactor_t::~oscactor_t() {}

void oscactor_t::update(uint32_t, bool)
{
  if(channels[0] > -1)
    pos.x = data[channels[0]] * influence[0];
  if(channels[1] > -1)
    pos.y = data[channels[1]] * influence[1];
  if(channels[2] > -1)
    pos.z = data[channels[2]] * influence[2];
  if(channels[3] > -1)
    eul.z = data[channels[3]] * influence[3];
  if(channels[4] > -1)
    eul.y = data[channels[4]] * influence[4];
  if(channels[5] > -1)
    eul.x = data[channels[5]] * influence[5];
  transform.position = pos;
  transform.orientation = eul;
  if(incremental)
    add_transformation(transform, local);
  else
    set_transformation(transform, local);
}

REGISTER_MODULE(oscactor_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
