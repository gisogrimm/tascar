#include "sourcemod.h"
#include "errorhandling.h"
#include <dlfcn.h>

using namespace TASCAR;

TASCAR_RESOLVER( sourcemod_base_t, xmlpp::Element* );

sourcemod_t::sourcemod_t( xmlpp::Element* cfg )
  : sourcemod_base_t( cfg ),
    sourcetype("omni"),
    lib( NULL ),
    libdata( NULL )
{
  get_attribute( "type", sourcetype );
  sourcetype = env_expand( sourcetype );
  std::string libname( "tascarsource_" );
  #ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
  #endif
  #if defined(__APPLE__)
    libname += sourcetype + ".dylib";
  #elif __linux__
    libname += sourcetype + ".so";
  #elif WIN32
    libname += sourcetype + ".dll";
  #else
    #error not supported
  #endif
  lib = dlopen(libname.c_str(), RTLD_NOW );
  if( !lib )
    throw TASCAR::ErrMsg("Unable to open source module \""+sourcetype+"\": "+dlerror());
  try{
    sourcemod_base_t_resolver( &libdata, cfg, lib, libname );
  }
  catch( ... ){
    dlclose(lib);
    throw;
  }
}

bool sourcemod_t::read_source(pos_t& prel, const std::vector<wave_t>& input, wave_t& output, sourcemod_base_t::data_t* data)
{
  return libdata->read_source( prel, input, output, data );
}

bool sourcemod_t::read_source_diffuse(pos_t& prel, const std::vector<wave_t>& input, wave_t& output, sourcemod_base_t::data_t* data)
{
  return libdata->read_source_diffuse( prel, input, output, data );
}

std::vector<std::string> sourcemod_t::get_connections() const
{
  return libdata->get_connections();
}

void sourcemod_t::configure()
{
  sourcemod_base_t::configure();
  libdata->prepare( cfg() );
}

void sourcemod_t::release()
{
  sourcemod_base_t::release();
  libdata->release();
}

sourcemod_base_t::data_t* sourcemod_t::create_data(double srate,uint32_t fragsize)
{
  return libdata->create_data(srate,fragsize);
}

void sourcemod_base_t::configure()
{
  if( n_channels != 1 )
    throw TASCAR::ErrMsg("This source module requires 1 input channel, current configuration is "+TASCAR::to_string(n_channels)+" channels.");
}

bool sourcemod_base_t::read_source_diffuse(TASCAR::pos_t& prel, const std::vector<TASCAR::wave_t>& input, TASCAR::wave_t& output, sourcemod_base_t::data_t*)
{
  if( n_channels != 1 )
    throw TASCAR::ErrMsg("This source module requires 1 input channel.");
  output.copy(input[0]);
  return false;
}

bool sourcemod_base_t::read_source(TASCAR::pos_t& prel, const std::vector<TASCAR::wave_t>& input, TASCAR::wave_t& output, sourcemod_base_t::data_t*)
{
  output.copy(input[0]);
  return false;
}

sourcemod_t::~sourcemod_t()
{
  delete libdata;
  dlclose(lib);
}

sourcemod_base_t::sourcemod_base_t(xmlpp::Element* xmlsrc)
  : xml_element_t(xmlsrc)
{
}

sourcemod_base_t::~sourcemod_base_t()
{
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

