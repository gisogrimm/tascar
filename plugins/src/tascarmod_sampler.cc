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

#include "sampler.h"
#include "session.h"

class sound_var_t : public TASCAR::xml_element_t {
public:
  sound_var_t(tsccfg::node_t xmlsrc);
  std::string name;
  double gain;
};

sound_var_t::sound_var_t(tsccfg::node_t xmlsrc) : xml_element_t(xmlsrc), gain(0)
{
  GET_ATTRIBUTE(name, "", "File name of sound file");
  GET_ATTRIBUTE(gain, "dB", "Gain to be applied");
}

class sampler_var_t : public TASCAR::module_base_t {
public:
  sampler_var_t(const TASCAR::module_cfg_t& cfg);
  std::string multicast;
  std::string port;
  std::vector<sound_var_t> sounds;
};

sampler_var_t::sampler_var_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), port("9999")
{
  GET_ATTRIBUTE(multicast, "", "Multicast address");
  GET_ATTRIBUTE(port, "", "OSC port number");
  if(port.empty()) {
    std::cerr << "Warning: Empty port number; using default port 9999.\n";
    port = "9999";
  }
  for(auto sne : tsccfg::node_get_children(e, "sound"))
    sounds.push_back(sound_var_t(sne));
}

class sampler_mod_t : public sampler_var_t, public TASCAR::sampler_t {
public:
  sampler_mod_t(const TASCAR::module_cfg_t& cfg);
  ~sampler_mod_t();
};

sampler_mod_t::sampler_mod_t(const TASCAR::module_cfg_t& cfg)
    : sampler_var_t(cfg), TASCAR::sampler_t(jacknamer(session->name, "sampler"),
                                            multicast, port)
{
  for(auto snd : sampler_var_t::sounds)
    add_sound(snd.name, snd.gain);
  start();
}

sampler_mod_t::~sampler_mod_t()
{
  stop();
}

REGISTER_MODULE(sampler_mod_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
