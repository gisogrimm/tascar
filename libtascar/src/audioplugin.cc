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

#include "audioplugin.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <dlfcn.h>

using namespace TASCAR;

transport_t::transport_t()
    : session_time_samples(0), session_time_seconds(0), object_time_samples(0),
      object_time_seconds(0), rolling(false)
{
}

audioplugin_base_t::audioplugin_base_t(const audioplugin_cfg_t& cfg)
    : xml_element_t(cfg.xmlsrc), licensed_component_t(typeid(*this).name()),
      name(cfg.name), parentname(cfg.parentname), modname(cfg.modname)
{
}

audioplugin_base_t::~audioplugin_base_t() {}

TASCAR_RESOLVER(audioplugin_base_t, const audioplugin_cfg_t&)

TASCAR::audioplugin_t::audioplugin_t(const audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), lib(NULL), libdata(NULL)
{
  plugintype = tsccfg::node_get_name(e);
  if(plugintype == "plugin")
    get_attribute("type", plugintype, "", "plugin type");
  std::string libname("tascar_ap_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
  libname += plugintype + TASCAR::dynamic_lib_extension();
  modname = plugintype;
  audioplugin_cfg_t lcfg(cfg);
  lcfg.modname = modname;
  lib = TASCAR::dlopen(libname.c_str(), RTLD_NOW);
  if(!lib)
    throw TASCAR::ErrMsg("Unable to open module \"" + plugintype +
                         "\": " + dlerror());
  try {
    audioplugin_base_t_resolver(&libdata, lcfg, lib, libname);
  }
  catch(...) {
    dlclose(lib);
    throw;
  }
}

void TASCAR::audioplugin_t::ap_process(std::vector<wave_t>& chunk,
                                       const TASCAR::pos_t& pos,
                                       const TASCAR::zyx_euler_t& o,
                                       const TASCAR::transport_t& tp)
{
  libdata->ap_process(chunk, pos, o, tp);
}

void TASCAR::audioplugin_t::configure()
{
  audioplugin_base_t::configure();
  libdata->prepare(cfg());
}

void TASCAR::audioplugin_t::post_prepare()
{
  audioplugin_base_t::post_prepare();
  libdata->post_prepare();
}

void TASCAR::audioplugin_t::release()
{
  audioplugin_base_t::release();
  libdata->release();
}

void TASCAR::audioplugin_t::add_variables(TASCAR::osc_server_t* srv)
{
  libdata->add_variables(srv);
}

void TASCAR::audioplugin_t::add_licenses(licensehandler_t* session)
{
  audioplugin_base_t::add_licenses(session);
  libdata->add_licenses(session);
}

TASCAR::audioplugin_t::~audioplugin_t()
{
  delete libdata;
  dlclose(lib);
}

void TASCAR::audioplugin_t::validate_attributes(std::string& msg) const
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
