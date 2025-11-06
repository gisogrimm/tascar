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

#include "session_reader.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 0x1000000
#endif

TASCAR::tsc_reader_t::tsc_reader_t()
    : xml_doc_t("<session/>", LOAD_STRING),
      licensed_component_t(typeid(*this).name()), file_name("")
{
  {
    char c_respath[PATH_MAX];
    orig_path = getcwd(c_respath, PATH_MAX);
  }
  // avoid problems with number format in xml file:
  setlocale(LC_ALL, "C");
  char* c_respath(getcwd(NULL, 0));
  session_path = c_respath;
  free(c_respath);
  if(root.get_element_name() != "session")
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got " +
                         root.get_element_name() + ".");
  TASCAR::reset_tuid_list();
}

void add_includes(tsccfg::node_t e, const std::string& parentdoc,
                  licensehandler_t* lh)
{
  for(auto& sne : tsccfg::node_get_children(e)) {
    if(tsccfg::node_get_name(sne) == "include") {
      std::string idocname(
          TASCAR::env_expand(tsccfg::node_get_attribute_value(sne, "name")));
      if((!idocname.empty()) && (idocname != parentdoc)) {
        TASCAR::xml_doc_t idoc(idocname, TASCAR::xml_doc_t::LOAD_FILE);
        if(idoc.root.get_element_name() != tsccfg::node_get_name(e)) {
          throw TASCAR::ErrMsg(
              "Invalid root node \"" + idoc.root.get_element_name() +
              "\" in include file \"" + idocname + "\".\nexpected \"" +
              tsccfg::node_get_name(e) + "\".");
        }
        std::string sublicense;
        std::string subattribution;
        get_license_info(idoc.root(), "", sublicense, subattribution);
        lh->add_license(sublicense, subattribution,
                        TASCAR::tscbasename(idocname));
        add_includes(idoc.root(), idocname, lh);
        for(auto& isne : idoc.root.get_children())
          tsccfg::node_import_node_before(e, isne, sne);
        tsccfg::node_remove_child(e, sne);
      }
    } else {
      add_includes(sne, parentdoc, lh);
    }
  }
}

const std::string& showstring(const std::string& s)
{
  DEBUG(s);
  return s;
}

TASCAR::tsc_reader_t::~tsc_reader_t()
{
  auto result = chdir(orig_path.c_str());
  if(result != 0)
    TASCAR::add_warning("Unable to change to directory \"" + orig_path + "\"." +
                        strerror(errno));
}

TASCAR::tsc_reader_t::tsc_reader_t(const std::string& filename_or_data,
                                   load_type_t t, const std::string& path)
    : xml_doc_t(filename_or_data, t),
      licensed_component_t(typeid(*this).name()), file_name("")
{
  {
    char c_respath[PATH_MAX];
    orig_path = getcwd(c_respath, PATH_MAX);
  }
  if(t == LOAD_FILE)
    file_name = filename_or_data;
  else
    file_name = "(loaded from string)";
  // avoid problems with number format in xml file:
  setlocale(LC_ALL, "C");
  if(path.size()) {
    char c_fname[path.size() + 1];
    char c_respath[PATH_MAX];
    memcpy(c_fname, path.c_str(), path.size() + 1);
    session_path = TASCAR::realpath(dirname(c_fname), c_respath);
    // Change current working directory of the complete process to the
    // directory containing the main configuration file in order to
    // resolve any relative paths present in the configuration.
    if(chdir(session_path.c_str()) != 0)
      add_warning("Unable to change directory.");
  } else {
    char c_respath[PATH_MAX];
    session_path = getcwd(c_respath, PATH_MAX);
  }
  if(root.get_element_name() != "session")
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got " +
                         root.get_element_name() + ".");
  TASCAR::reset_tuid_list();
  // add session-includes:
  add_includes(root(), "", this);
}

void TASCAR::tsc_reader_t::read_xml()
{
  root.GET_ATTRIBUTE(license, "", "license type");
  root.GET_ATTRIBUTE(attribution, "", "attribution of license, if applicable");
  add_license(license, attribution, "session file");
  root.GET_ATTRIBUTE(profilingpath, "",
                     "OSC path to dispatch module profiling information to");
  use_profiler = profilingpath.size() > 0;
  for(auto& sne : root.get_children()) {
    TASCAR::mainloopupdate();
    if(tsccfg::node_get_name(sne) == "scene")
      add_scene(sne);
    else if(tsccfg::node_get_name(sne) == "range")
      add_range(sne);
    else if(tsccfg::node_get_name(sne) == "connect")
      add_connection(sne);
    else if(tsccfg::node_get_name(sne) == "modules") {
      for(auto& lsne : tsccfg::node_get_children(sne)) {
        add_module(lsne);
      }
    } else if(tsccfg::node_get_name(sne) == "license") {
      TASCAR::xml_element_t lic(sne);
      std::string license;
      std::string attribution;
      std::string name;
      lic.GET_ATTRIBUTE(license, "", "license type");
      lic.GET_ATTRIBUTE(attribution, "",
                        "attribution of license, if applicable");
      lic.GET_ATTRIBUTE(name, "", "name of licensed component");
      add_license(license, attribution, name);
    } else if(tsccfg::node_get_name(sne) == "author") {
      TASCAR::xml_element_t lic(sne);
      std::string of;
      std::string name;
      lic.GET_ATTRIBUTE(name, "", "author name");
      lic.GET_ATTRIBUTE(of, "", "name of authored component");
      add_author(name, of);
    } else if(tsccfg::node_get_name(sne) == "bibitem") {
      add_bibitem(tsccfg::node_get_text(sne));

    } else if((tsccfg::node_get_name(sne) != "include") &&
              (tsccfg::node_get_name(sne) != "mainwindow") &&
              (tsccfg::node_get_name(sne) != "description"))
      add_warning("Invalid element: " + tsccfg::node_get_name(sne), sne);
    if(tsccfg::node_get_name(sne) == "module")
      add_module(sne);
  }
  if(localgetenv("TASCARGENDOC").size()) {
    generate_documentation = true;
    generate_plugin_documentation_tables(true);
  }
}

const std::string& TASCAR::tsc_reader_t::get_session_path() const
{
  return session_path;
}

const std::string& TASCAR::tsc_reader_t::get_file_name() const
{
  return file_name;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
