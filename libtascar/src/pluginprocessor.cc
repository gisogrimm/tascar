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

#include "pluginprocessor.h"

using namespace TASCAR;

plugin_processor_t::plugin_processor_t(tsccfg::node_t xmlsrc,
                                       const std::string& name,
                                       const std::string& parentname)
    : xml_element_t(xmlsrc), licensed_component_t(typeid(*this).name()),
      eplug(find_or_add_child("plugins"))
{
  eplug.GET_ATTRIBUTE(profilingpath, "",
                      "OSC path to dispatch profiling information to");
  use_profiler = profilingpath.size() > 0;
  msg = lo_message_new();
  for(auto& sne : eplug.get_children()) {
    plugins.push_back(
        new TASCAR::audioplugin_t(audioplugin_cfg_t(sne, name, parentname)));
    lo_message_add_double(msg, 0.0);
  }
  oscmsgargv = lo_message_get_argv(msg);
  if(use_profiler) {
    std::cout << "<osc path=\"" << profilingpath << "\" size=\""
              << plugins.size() << "\"/>" << std::endl;
    std::cout << "csPlugins = { ";
    for(auto mod : plugins)
      std::cout << "'" << mod->get_modname() << "' ";
    std::cout << "};" << std::endl;
  }
}

void plugin_processor_t::configure()
{
  try {
    for(auto p : plugins)
      p->prepare(cfg());
  }
  catch(...) {
    for(auto p : plugins)
      if(p->is_prepared())
        p->release();
    throw;
  }
}

void plugin_processor_t::post_prepare()
{
  for(auto& p : plugins)
    p->post_prepare();
}

void plugin_processor_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  for(auto p : plugins)
    p->add_licenses(lh);
}

void plugin_processor_t::release()
{
  audiostates_t::release();
  for(auto p : plugins)
    if(p->is_prepared())
      p->release();
}

plugin_processor_t::~plugin_processor_t()
{
  for(auto p : plugins)
    delete p;
  lo_message_free(msg);
}

void plugin_processor_t::validate_attributes(std::string& msg) const
{
  eplug.validate_attributes(msg);
  for(auto p : plugins)
    p->validate_attributes(msg);
}

void plugin_processor_t::process_plugins(std::vector<wave_t>& s,
                                         const pos_t& pos, const rotmat_t& o,
                                         const transport_t& tp)
{
  double t_prev = 0.0;
  size_t k = 0;
  if(use_profiler)
    tictoc.tic();
  for(auto p : plugins) {
    p->ap_process(s, pos, o, tp);
    if(use_profiler) {
      auto t = tictoc.toc();
      oscmsgargv[k]->d = t - t_prev;
      t_prev = t;
    }
    ++k;
  }
  if(use_profiler && oscsrv)
    oscsrv->dispatch_data_message(profilingpath.c_str(), msg);
}

void plugin_processor_t::add_variables(TASCAR::osc_server_t* srv)
{
  oscsrv = srv;
  std::string oldpref(srv->get_prefix());
  uint32_t k = 0;
  for(auto p : plugins) {
    char ctmp[1024];
    ctmp[1023] = 0;
    snprintf(ctmp, 1023, "ap%u", k);
    srv->set_prefix(oldpref + "/" + ctmp + "/" + p->get_modname());
    p->add_variables(srv);
    ++k;
  }
  srv->set_prefix(oldpref);
}

// Local Variables:
// compile-command: "make -C .."
// End:
