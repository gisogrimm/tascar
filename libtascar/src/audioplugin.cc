#include "audioplugin.h"
#include "errorhandling.h"
#include <dlfcn.h>

using namespace TASCAR;

transport_t::transport_t()
  : session_time_samples(0), session_time_seconds(0), 
    object_time_samples(0), object_time_seconds(0), rolling(false)
{
}

audioplugin_base_t::audioplugin_base_t( const audioplugin_cfg_t& cfg )
  : xml_element_t(cfg.xmlsrc),
    licensed_component_t(typeid(*this).name()),
    name(cfg.name),
    parentname(cfg.parentname),
    modname(cfg.modname)
{
}

audioplugin_base_t::~audioplugin_base_t()
{
}

TASCAR_RESOLVER( audioplugin_base_t, const audioplugin_cfg_t& )


TASCAR::audioplugin_t::audioplugin_t( const audioplugin_cfg_t& cfg )
  : audioplugin_base_t( cfg ),
    lib(NULL),
    libdata(NULL)
{
  plugintype = e->get_name();
  if( plugintype == "plugin" )
    get_attribute("type",plugintype);
  std::string libname("tascar_ap_");
  #ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
  #endif
  #if defined(__APPLE__)
    libname += plugintype + ".dylib";
  #elif __linux__
    libname += plugintype + ".so";
  #elif WIN32
    libname += plugintype + ".dll";
  #else
    #error not supported
  #endif
  modname = plugintype;
  audioplugin_cfg_t lcfg(cfg);
  lcfg.modname = modname;
  lib = dlopen(libname.c_str(), RTLD_NOW );
  if( !lib )
    throw TASCAR::ErrMsg("Unable to open module \""+plugintype+"\": "+dlerror());
  try{
    audioplugin_base_t_resolver( &libdata, lcfg, lib, libname );
  }
  catch( ... ){
    dlclose(lib);
    throw;
  }
}

void TASCAR::audioplugin_t::ap_process( std::vector<wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::zyx_euler_t& o, const TASCAR::transport_t& tp )
{
  libdata->ap_process( chunk, pos, o, tp );
}

void TASCAR::audioplugin_t::configure()
{
  audioplugin_base_t::configure();
  libdata->prepare( cfg() );
}

void TASCAR::audioplugin_t::release()
{
  audioplugin_base_t::release();
  libdata->release();
}

void TASCAR::audioplugin_t::add_variables(TASCAR::osc_server_t* srv)
{
  libdata->add_variables( srv );
}

void TASCAR::audioplugin_t::add_licenses( licensehandler_t* session )
{
  audioplugin_base_t::add_licenses( session );
  libdata->add_licenses( session );
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
