/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "sourcemod.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <dlfcn.h>

using namespace TASCAR;

TASCAR_RESOLVER(sourcemod_base_t, tsccfg::node_t);

sourcemod_t::sourcemod_t(tsccfg::node_t cfg)
    : sourcemod_base_t(cfg), sourcetype("omni"), lib(NULL), libdata(NULL)
{
  get_attribute("type", sourcetype, "",
                "source directivity type, e.g., omni, cardioid");
  sourcetype = env_expand(sourcetype);
  std::string libname("tascarsource_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
  libname += sourcetype + TASCAR::dynamic_lib_extension();
  lib = TASCAR::dlopen(libname.c_str(), RTLD_NOW);
  if(!lib)
    throw TASCAR::ErrMsg("Unable to open source module \"" + sourcetype +
                         "\": " + dlerror());
  try {
    sourcemod_base_t_resolver(&libdata, cfg, lib, libname);
  }
  catch(...) {
    dlclose(lib);
    throw;
  }
}

bool sourcemod_t::read_source(pos_t& prel, const std::vector<wave_t>& input,
                              wave_t& output, sourcemod_base_t::data_t* data)
{
  return libdata->read_source(prel, input, output, data);
}

bool sourcemod_t::read_source_diffuse(pos_t& prel,
                                      const std::vector<wave_t>& input,
                                      wave_t& output,
                                      sourcemod_base_t::data_t* data)
{
  return libdata->read_source_diffuse(prel, input, output, data);
}

std::vector<std::string> sourcemod_t::get_connections() const
{
  return libdata->get_connections();
}

void sourcemod_t::configure()
{
  sourcemod_base_t::configure();
  libdata->prepare(cfg());
}

void sourcemod_t::post_prepare()
{
  sourcemod_base_t::post_prepare();
  libdata->post_prepare();
}

void sourcemod_t::release()
{
  sourcemod_base_t::release();
  libdata->release();
}

sourcemod_base_t::data_t*
sourcemod_t::create_state_data(double srate, uint32_t fragsize) const
{
  return libdata->create_state_data(srate, fragsize);
}

void sourcemod_base_t::configure()
{
  if(n_channels != 1)
    throw TASCAR::ErrMsg("This source module requires 1 input channel, current "
                         "configuration is " +
                         std::to_string(n_channels) + " channels.");
}

bool sourcemod_base_t::read_source_diffuse(
    TASCAR::pos_t&, const std::vector<TASCAR::wave_t>& input,
    TASCAR::wave_t& output, sourcemod_base_t::data_t*)
{
  if(n_channels != 1)
    throw TASCAR::ErrMsg("This source module requires 1 input channel.");
  output.copy(input[0]);
  return false;
}

bool sourcemod_base_t::read_source(TASCAR::pos_t&,
                                   const std::vector<TASCAR::wave_t>& input,
                                   TASCAR::wave_t& output,
                                   sourcemod_base_t::data_t*)
{
  output.copy(input[0]);
  return false;
}

sourcemod_t::~sourcemod_t()
{
  delete libdata;
  dlclose(lib);
}

sourcemod_base_t::sourcemod_base_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc)
{
}

sourcemod_base_t::~sourcemod_base_t() {}

void TASCAR::sourcemod_t::add_variables(TASCAR::osc_server_t* srv)
{
  return libdata->add_variables(srv);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
