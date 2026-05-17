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

#include "maskplugin.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <dlfcn.h>

using namespace TASCAR;

maskplugin_base_t::maskplugin_base_t(const maskplugin_cfg_t& cfg)
    : xml_element_t(cfg.xmlsrc), licensed_component_t(typeid(*this).name()),
      modname(cfg.modname)
{
  GET_ATTRIBUTE(
      drawradius, "m",
      "Draw mask plugin with this radius in TASCAR GUI, 0 for no drawing.");
}

maskplugin_base_t::~maskplugin_base_t() {}

TASCAR_RESOLVER(maskplugin_base_t, const maskplugin_cfg_t&)

TASCAR::maskplugin_t::maskplugin_t(const maskplugin_cfg_t& cfg)
    : maskplugin_base_t(cfg), lib(NULL), libdata(NULL)
{
  get_attribute("type", plugintype, "", "mask plugin type");
  std::string libname("tascar_mask_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
  libname += plugintype + TASCAR::dynamic_lib_extension();
  modname = plugintype;
  maskplugin_cfg_t lcfg(cfg);
  lcfg.modname = modname;
  lib = TASCAR::dlopen(libname.c_str(), RTLD_NOW);
  if(!lib)
    throw TASCAR::ErrMsg("Unable to open module \"" + plugintype +
                         "\": " + dlerror());
  try {
    maskplugin_base_t_resolver(&libdata, lcfg, lib, libname);
  }
  catch(...) {
    dlclose(lib);
    throw;
  }
}

float TASCAR::maskplugin_t::get_gain(const TASCAR::pos_t& pos)
{
  return libdata->get_gain(pos);
}

void TASCAR::maskplugin_t::get_diff_gain(float* gm)
{
  libdata->get_diff_gain(gm);
}

void TASCAR::maskplugin_t::add_variables(TASCAR::osc_server_t* srv)
{
  libdata->add_variables(srv);
}

void TASCAR::maskplugin_t::add_licenses(licensehandler_t* session)
{
  maskplugin_base_t::add_licenses(session);
  libdata->add_licenses(session);
}

TASCAR::maskplugin_t::~maskplugin_t()
{
  delete libdata;
  dlclose(lib);
}

void TASCAR::maskplugin_t::validate_attributes(std::string& msg) const
{
  libdata->validate_attributes(msg);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
