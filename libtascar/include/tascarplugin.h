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
#ifndef TASCARPLUGIN_H
#define TASCARPLUGIN_H

#include "tascarver.h"
#include <string.h>

#define TASCAR_PLUGIN(baseclass, cfgclass, pluginclass)                        \
  using namespace TASCAR;                                                      \
  extern "C" baseclass* baseclass##_factory(cfgclass cfg, std::string& emsg)   \
  {                                                                            \
    try {                                                                      \
      return new pluginclass(cfg);                                             \
    }                                                                          \
    catch(const std::exception& e) {                                           \
      emsg = e.what();                                                         \
      return NULL;                                                             \
    }                                                                          \
  };                                                                           \
  extern "C" const char* baseclass##_tascar_version() { return TASCARVER; };

#define TASCAR_RESOLVER(baseclass, cfgclass)                                   \
  void baseclass##_resolver(baseclass** instance, cfgclass cfg, void* lib,     \
                            const std::string& libname)                        \
  {                                                                            \
    typedef const char* (*baseclass##_tascar_version_t)();                     \
    baseclass##_tascar_version_t tscver((baseclass##_tascar_version_t)dlsym(   \
        lib, #baseclass "_tascar_version"));                                   \
    if(!tscver)                                                                \
      throw TASCAR::ErrMsg(                                                    \
          "Unable to resolve tascar version function\n(module: " + libname +   \
          ").");                                                               \
    std::string cl_tscver(TASCARVER);                                          \
    std::string pl_tscver(tscver());                                           \
    if(cl_tscver != pl_tscver)                                                 \
      throw TASCAR::ErrMsg("Invalid plugin version " + pl_tscver +             \
                           ".\n(module: " + libname + ", expected version " +  \
                           cl_tscver + ").");                                  \
    typedef baseclass* (*baseclass##factory_t)(cfgclass, std::string&);        \
    baseclass##factory_t factory(                                              \
        (baseclass##factory_t)dlsym(lib, #baseclass "_factory"));              \
    if(!factory)                                                               \
      throw TASCAR::ErrMsg("Unable to resolve factory of " +                   \
                           std::string(#baseclass) + "\n(module: " + libname + \
                           ").");                                              \
    std::string emsg;                                                          \
    *instance = factory(cfg, emsg);                                            \
    if(!(*instance))                                                           \
      throw TASCAR::ErrMsg("Error while loading \"" + libname +                \
                           "\": " + emsg);                                     \
  }

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
