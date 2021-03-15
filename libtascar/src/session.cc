#include "session.h"
#include <unistd.h>
#include <string.h>
#ifndef WIN32
#include <fnmatch.h>
#include <libgen.h>
#endif
#include "errorhandling.h"
#include <dlfcn.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <thread>
#include <chrono>

/**
   \defgroup moddev Module development

   The functionality of TASCAR can be extended using modules. Five
   types of modules can be used in TASCAR:
   

   \li general purpose modules (derived from TASCAR::module_base_t)
   \li actor modules (derived from TASCAR::actor_module_t)
   \li audio processing plugins (derived from TASCAR::audioplugin_base_t)
   \li source modules (derived from TASCAR::sourcemod_base_t)
   \li receiver modules (derived from TASCAR::receivermod_base_t)

   All of these module types consist of an initialization block which
   is called when the module is loaded, a prepare() and release()
   method pair, which is called when the audio properties such as
   sampling rate and block size are defined, and an update method,
   which is called once in each processing cycle.

   General purpose modules and actor modules are updated in
   TASCAR::session_t::process(), which is called first, before the
   geometry update and acoustic modelling of the scenes.

   Receiver modules, source modules and audio processing plugins are
   updated at their appropriate place during acoustic modelling, i.e,
   after the geometry update in TASCAR::render_core_t::process().

   To develop your own modules, copy the folder
   /usr/share/tascar/examples/plugins to a destination of your choice,
   e.g.:

   \verbatim
   cp -r /usr/share/tascar/examples/plugins ./
   \endverbatim

   Change to the new plugins directory:

   \verbatim
   cd plugins
   \endverbatim

   Now modify the Makefile and the appropriate source files in the "src"
   folder to your needs. For compilation, type:

   \verbatim
   make
   \endverbatim

   The plugins can be tested by starting tascar with:
   \verbatim
   LD_LIBRARY_PATH=./build tascar sessionfile.tsc
   \endverbatim

   Replace "sessionfile.tsc" by the name of your session file, e.g.,
   "example_rotate.tsc".

   To install the plugins, type:
   \verbatim
   sudo make install
   \endverbatim

   \note All external modules should use SI units for signals and variables.
   


   \section actormod Actor modules

   An example of an actor module is provided in \ref tascar_rotate.cc .

   Actor modules can interact with the position of objects. The
   objects can be selected with the \c actor attribute (see the <a
   href="file:///usr/share/doc/tascar/manual.pdf#section.6">user
   manual, section 6</a> for details).

   The core functionality is implemented in the update() method. Here
   you may access the list of actor objects,
   TASCAR::actor_module_t::obj, or use any of the following functions
   to control the delta-transformation of the objects:

   <ul>
   <li>TASCAR::actor_module_t::set_location()</li>
   <li>TASCAR::actor_module_t::set_orientation()</li>
   <li>TASCAR::actor_module_t::set_transformation()</li>
   <li>TASCAR::actor_module_t::add_location()</li>
   <li>TASCAR::actor_module_t::add_orientation()</li>
   <li>TASCAR::actor_module_t::add_transformation()</li>
   </ul>

   Variables can be registered for XML access (static) or OSC access
   (interactive) in the constructor. To register a variable for XML
   access, use the macro GET_ATTRIBUTE(). OSC variables can be
   registered with TASCAR::osc_server_t::add_double() and similar
   member functions of TASCAR::osc_server_t.

   The parameters of the audio signal backend, e.g., sampling rate and
   fragment size, can be accessed via the public attributes of
   chunk_cfg_t.

   \section audioplug Audio plugins

   An example of an audio plugin is provided in \ref
   tascar_ap_noise.cc .

   Audio plugins can be used in sounds or in receivers (see the <a
   href="file:///usr/share/doc/tascar/manual.pdf#section.7">user
   manual, section 7</a> for more details). In sounds,
   they are processed before application of the acoustic model, and in
   receivers, they are processed after rendering, in the receiver
   post_proc method.

   \section sourcemod Source modules

   An example of a source module is provided in \ref
   tascarsource_example.cc .

   Source directivity types are properties of sounds (see the <a
   href="file:///usr/share/doc/tascar/manual.pdf#subsection.5.3">user
   manual, subsection 5.3</a> for more details).

   \section receivermod Receiver modules

   An example of a receiver module is provided in \ref
   tascarreceiver_example.cc .

   For use documentation of receivers, see the <a
   href="file:///usr/share/doc/tascar/manual.pdf#subsection.5.5">user
   manual, subsection 5.5</a>.

   \example tascar_rotate.cc

   \example tascar_ap_noise.cc

   \example tascarsource_example.cc
   

   \example tascarreceiver_example.cc
   

*/


using namespace TASCAR;

TASCAR_RESOLVER(module_base_t, const module_cfg_t&)

TASCAR::module_t::module_t(const TASCAR::module_cfg_t& cfg)
: module_base_t(cfg),
  lib(NULL),
  libdata(NULL)
{
  name = e->get_name();
  if(name == "module") {
    char cline[256];
    sprintf(cline, "%d", e->get_line());
    std::string msg("Deprecated session file, line ");
    msg += cline;
    msg += ": Use modules within <modules>...</modules> section.";
    TASCAR::add_warning(msg);
    get_attribute("name", name);
  }
  std::string libname("tascar_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
#if defined(__APPLE__)
  libname += name + ".dylib";
#elif __linux__
  libname += name + ".so";
#elif WIN32
  libname += name + ".dll";
#else
#error not supported
#endif
  lib = dlopen(libname.c_str(), RTLD_NOW);
  if(!lib)
    throw TASCAR::ErrMsg("Unable to open module \""+name+"\": "+dlerror());
  try {
    module_base_t_resolver(&libdata, cfg, lib, libname);
  }
  catch(...) {
    dlclose(lib);
    throw;
  }
}

void TASCAR::module_t::update(uint32_t frame, bool running)
{
  if(is_prepared())
    libdata->update(frame, running);
}

void TASCAR::module_t::configure()
{
  module_base_t::configure();
  try {
    libdata->prepare(cfg());
  }
  catch(...) {
    module_base_t::release();
    throw;
  }
}

void TASCAR::module_t::release()
{
  module_base_t::release();
  libdata->release();
}

void TASCAR::module_t::validate_attributes(std::string& msg) const
{
  libdata->validate_attributes(msg);
}

TASCAR::module_t::~module_t()
{
  delete libdata;
  dlclose(lib);
}

TASCAR::module_cfg_t::module_cfg_t(xmlpp::Element* xmlsrc_, TASCAR::session_t* session_ )
    : session(session_), xmlsrc(xmlsrc_)
{
}

xmlpp::Element* assert_element(xmlpp::Element* e)
{
  if(!e)
    throw TASCAR::ErrMsg("NULL pointer element");
  return e;
}

const std::string& debug_str(const std::string& s)
{
  return s;
}

TASCAR::session_oscvars_t::session_oscvars_t(xmlpp::Element* src)
  : xml_element_t(src),
    name("tascar"),
    srv_port("9877"),
    srv_proto("UDP")
{
  GET_ATTRIBUTE(srv_port);
  GET_ATTRIBUTE(srv_addr);
  GET_ATTRIBUTE(srv_proto);
  GET_ATTRIBUTE(name);
  GET_ATTRIBUTE(starturl);
}

TASCAR::session_core_t::session_core_t()
  : duration(60),
    loop(false),
    levelmeter_tc(2.0),
    levelmeter_weight(TASCAR::levelmeter::Z),
    levelmeter_min(30.0),
    levelmeter_range(70.0),
    requiresrate(0),
    warnsrate(0),
    requirefragsize(0),
    warnfragsize(0),
    h_pipe_initcmd(NULL),
    pid_initcmd(0)
{
  GET_ATTRIBUTE(duration);
  GET_ATTRIBUTE_BOOL(loop);
  GET_ATTRIBUTE(levelmeter_tc);
  GET_ATTRIBUTE(levelmeter_weight);
  GET_ATTRIBUTE(levelmeter_mode);
  GET_ATTRIBUTE(levelmeter_min);
  GET_ATTRIBUTE(levelmeter_range);
  GET_ATTRIBUTE(requiresrate);
  GET_ATTRIBUTE(requirefragsize);
  GET_ATTRIBUTE(warnsrate);
  GET_ATTRIBUTE(warnfragsize);
  GET_ATTRIBUTE(initcmd);
  start_initcmd();
}

void TASCAR::session_core_t::start_initcmd()
{
  if(!initcmd.empty()) {
    char ctmp[1024];
    memset(ctmp, 0, 1024);
    snprintf(ctmp,1024,"sh -c \"%s >/dev/null & echo \\$!;\"",initcmd.c_str());
    h_pipe_initcmd = popen(ctmp, "r");
    if(fgets(ctmp, 1024, h_pipe_initcmd) != NULL) {
      pid_initcmd = atoi(ctmp);
      if(pid_initcmd == 0) {
        std::cerr << "Warning: Invalid subprocess PID (while attempting to start init command \"" << initcmd << "\")." << std::endl;
      }
    }
    if(initcmdsleep > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000.0*initcmdsleep)));
  }
}

TASCAR::session_core_t::session_core_t(const std::string& filename_or_data,load_type_t t,const std::string& path)
  : TASCAR::tsc_reader_t(filename_or_data,t,path),
  duration(60),
  loop(false),
  playonload(false),
  levelmeter_tc(2.0),
  levelmeter_weight(TASCAR::levelmeter::Z),
  levelmeter_min(30.0),
  levelmeter_range(70.0),
  requiresrate(0),
  warnsrate(0),
  requirefragsize(0),
  warnfragsize(0),
  initcmdsleep(0),
  h_pipe_initcmd(NULL),
  pid_initcmd(0)
{
  GET_ATTRIBUTE(duration);
  GET_ATTRIBUTE_BOOL(loop);
  GET_ATTRIBUTE_BOOL(playonload);
  GET_ATTRIBUTE(levelmeter_tc);
  GET_ATTRIBUTE(levelmeter_weight);
  GET_ATTRIBUTE(levelmeter_mode);
  GET_ATTRIBUTE(levelmeter_min);
  GET_ATTRIBUTE(levelmeter_range);
  GET_ATTRIBUTE(requiresrate);
  GET_ATTRIBUTE(requirefragsize);
  GET_ATTRIBUTE(warnsrate);
  GET_ATTRIBUTE(warnfragsize);
  GET_ATTRIBUTE(initcmd);
  GET_ATTRIBUTE(initcmdsleep);
  start_initcmd();
}

TASCAR::session_core_t::~session_core_t()
{
#ifndef WIN32
  if(pid_initcmd != 0)
    kill(pid_initcmd, SIGTERM);
#endif
  if(h_pipe_initcmd)
    fclose(h_pipe_initcmd);
}

void assert_jackpar( const std::string& what, double expected, double found, bool warn)
{
  if((expected > 0) && (expected != found)) {
    std::string msg("Invalid " + what + " (expected " +
                    TASCAR::to_string(expected) + ", jack has " +
                    TASCAR::to_string(found) + ")");
    if(warn)
      TASCAR::add_warning(msg);
    else
      throw TASCAR::ErrMsg(msg);
  }
}

TASCAR::session_t::session_t()
    : TASCAR::session_oscvars_t(tsc_reader_t::e),
      jackc_transport_t(jacknamer(name, "session.")),
    osc_server_t(srv_addr, srv_port, srv_proto, TASCAR::config("tascar.osc.list",0)),
  period_time(1.0/(double)srate),
  started_(false)
{
  assert_jackpar("sampling rate", requiresrate, srate, false);
  assert_jackpar("fragment size", requirefragsize, fragsize, false);
  assert_jackpar("sampling rate", warnsrate, srate, true);
  assert_jackpar("fragment size", warnfragsize, fragsize, true);
  pthread_mutex_init(&mtx, NULL);
  read_xml();
  try {
    add_output_port("sync_out");
    jackc_transport_t::activate();
    add_transport_methods();
    osc_server_t::activate();
    if(playonload)
      tp_start();
  }
  catch(...) {
    unload_modules();
    throw;
  }
}

TASCAR::session_t::session_t(const std::string& filename_or_data,load_type_t t,const std::string& path)
    : TASCAR::session_core_t(filename_or_data, t, path),
      session_oscvars_t(tsc_reader_t::e),
      jackc_transport_t(jacknamer(name, "session.")),
  osc_server_t(srv_addr, srv_port, srv_proto, TASCAR::config("tascar.osc.list",0)),
  period_time(1.0/(double)srate),
  started_(false)
{
  assert_jackpar("sampling rate", requiresrate, srate, false);
  assert_jackpar("fragment size", requirefragsize, fragsize, false);
  assert_jackpar("sampling rate", warnsrate, srate, true);
  assert_jackpar("fragment size", warnfragsize, fragsize, true);
  pthread_mutex_init(&mtx, NULL);
  // parse XML:
  read_xml();
  try {
    add_output_port("sync_out");
    jackc_transport_t::activate();
    add_transport_methods();
    osc_server_t::activate();
    if(playonload)
      tp_start();
  }
  catch(...) {
    unload_modules();
    throw;
  }
}

void TASCAR::session_t::read_xml()
{
  try {
    TASCAR::tsc_reader_t::read_xml();
  }
  catch(...) {
    if(lock_vars()) {
      std::vector<TASCAR::module_t*> lmodules(modules);
      modules.clear();
      for( std::vector<TASCAR::module_t*>::iterator it=lmodules.begin();it!=lmodules.end();++it)
        if((*it)->is_prepared())
          (*it)->release();
      for( std::vector<TASCAR::module_t*>::iterator it=lmodules.begin();it!=lmodules.end();++it)
        delete(*it);
      for( std::vector<TASCAR::scene_render_rt_t*>::iterator it=scenes.begin();it!=scenes.end();++it)
        delete(*it);
      for( std::vector<TASCAR::range_t*>::iterator it=ranges.begin();it!=ranges.end();++it)
        delete(*it);
      for( std::vector<TASCAR::connection_t*>::iterator it=connections.begin();it!=connections.end();++it)
        delete(*it);
      unlock_vars();
    }
    throw;
  }
}

void TASCAR::session_t::unload_modules()
{
  if(started_)
    stop();
  if(lock_vars()) {
    std::vector<TASCAR::module_t*> lmodules(modules);
    modules.clear();
    for(auto it = lmodules.begin(); it != lmodules.end(); ++it) {
      if((*it)->is_prepared())
        (*it)->release();
    }
    for(auto it = lmodules.begin(); it != lmodules.end(); ++it) {
      delete(*it);
    }
    for(auto it = scenes.begin(); it != scenes.end(); ++it)
      delete(*it);
    scenes.clear();
    for(auto it = ranges.begin(); it != ranges.end(); ++it)
      delete(*it);
    ranges.clear();
    for(auto it = connections.begin(); it != connections.end(); ++it)
      delete(*it);
    connections.clear();
    unlock_vars();
  }
}

bool TASCAR::session_t::lock_vars()
{
  return (pthread_mutex_lock(&mtx) == 0);
}

void TASCAR::session_t::unlock_vars()
{
  pthread_mutex_unlock(&mtx);
}

bool TASCAR::session_t::trylock_vars()
{
  return (pthread_mutex_trylock(&mtx) == 0);
}

TASCAR::session_t::~session_t()
{
  osc_server_t::deactivate();
  jackc_transport_t::deactivate();
  unload_modules();
  pthread_mutex_trylock(&mtx);
  pthread_mutex_unlock(&mtx);
  pthread_mutex_destroy(&mtx);
}

std::vector<std::string> TASCAR::session_t::get_render_output_ports() const
{
  std::vector<std::string> ports;
  for( std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it){
    std::vector<std::string> pports((*it)->get_output_ports());
    ports.insert(ports.end(), pports.begin(), pports.end());
  }
  return ports;
}

void TASCAR::session_t::add_scene(xmlpp::Element* src)
{
  TASCAR::scene_render_rt_t* newscene(NULL);
  if(!src)
    src = tsc_reader_t::e->add_child("scene");
  try {
    newscene = new TASCAR::scene_render_rt_t(src);
    if(namelist.find(newscene->name) != namelist.end())
      throw TASCAR::ErrMsg("A scene of name \"" + newscene->name +
                           "\" already exists in the session.");
    namelist.insert(newscene->name);
    scenes.push_back(newscene);
    scenes.back()->configure_meter(levelmeter_tc, levelmeter_weight);
  }
  catch(...) {
    delete newscene;
    throw;
  }
}

void TASCAR::session_t::add_range(xmlpp::Element* src)
{
  if(!src)
    src = tsc_reader_t::e->add_child("range");
  ranges.push_back(new TASCAR::range_t(src));
}

void TASCAR::session_t::add_connection(xmlpp::Element* src)
{
  if(!src)
    src = tsc_reader_t::e->add_child("connect");
  connections.push_back(new TASCAR::connection_t(src));
}

void TASCAR::session_t::add_module(xmlpp::Element* src)
{
  if(!src)
    src = tsc_reader_t::e->add_child("module");
  modules.push_back(new TASCAR::module_t(TASCAR::module_cfg_t(src, this)));
}

void TASCAR::session_t::start()
{
  started_ = true;
  try {
    for(std::vector<TASCAR::scene_render_rt_t*>::iterator ipl=scenes.begin();ipl!=scenes.end();++ipl){
      (*ipl)->start();
      (*ipl)->add_child_methods(this);
    }
  }
  catch(...) {
    started_ = false;
    throw;
  }
  auto last_prepared(modules.begin());
  try {
    for(std::vector<TASCAR::module_t*>::iterator imod=modules.begin();imod!=modules.end();++imod){
      chunk_cfg_t cf(srate, fragsize);
      last_prepared = imod;
      (*imod)->prepare(cf);
    }
    last_prepared = modules.end();
  }
  catch(...) {
    for(auto it = modules.begin(); it != last_prepared; ++it)
      (*it)->release();
    started_ = false;
    throw;
  }
  for(std::vector<TASCAR::connection_t*>::iterator icon=connections.begin();icon!=connections.end();++icon){
    connect((*icon)->src, (*icon)->dest, !(*icon)->failonerror, true, true);
  }
  for(std::vector<TASCAR::scene_render_rt_t*>::iterator ipl=scenes.begin();ipl!=scenes.end();++ipl){
    try {
      connect(get_client_name()+":sync_out",(*ipl)->get_client_name()+":sync_in");
    }
    catch(const std::exception& e) {
      add_warning(e.what());
    }
    (*ipl)->add_licenses(this);
  }
}

int TASCAR::session_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame, bool tp_rolling)
{
  double t(period_time * (double)tp_frame);
  uint32_t next_tp_frame(tp_frame);
  if(tp_rolling)
    next_tp_frame += fragsize;
  if(started_)
    for(std::vector<TASCAR::module_t*>::iterator imod=modules.begin();imod!=modules.end();++imod)
      (*imod)->update(next_tp_frame, tp_rolling);
  if((duration > 0) && (t >= duration)) {
    if(loop)
      tp_locate(0u);
    else
      tp_stop();
  }
  return 0;
}

void TASCAR::session_t::stop()
{
  started_ = false;
  for(std::vector<TASCAR::scene_render_rt_t*>::iterator ipl=scenes.begin();ipl!=scenes.end();++ipl)
    (*ipl)->stop();
}

void del_whitespace(xmlpp::Node* node)
{
  xmlpp::TextNode* nodeText = dynamic_cast<xmlpp::TextNode*>(node);
  if(nodeText && nodeText->is_white_space()) {
    nodeText->get_parent()->remove_child(node);
  } else {
    xmlpp::Element* nodeElement = dynamic_cast<xmlpp::Element*>(node);
    if(nodeElement) {
      xmlpp::Node::NodeList children = nodeElement->get_children();
      for(xmlpp::Node::NodeList::iterator nita=children.begin();nita!=children.end();++nita){
        del_whitespace(*nita);
      }
    }
  }
}

void TASCAR::xml_doc_t::save(const std::string& filename)
{
  if(doc) {
    del_whitespace(doc->get_root_node());
    doc->write_to_file_formatted(filename);
  }
}

void TASCAR::session_t::run(bool& b_quit, bool use_stdin)
{
  start();
  while(!b_quit) {
    usleep(50000);
    if(use_stdin) {
      getchar();
      if(feof(stdin))
        b_quit = true;
    }
  }
  stop();
}

uint32_t TASCAR::session_t::get_active_pointsources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it)
    rv += (*it)->active_pointsources;
  return rv;
}

uint32_t TASCAR::session_t::get_total_pointsources() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it)
    rv += (*it)->total_pointsources;
  return rv;
}

uint32_t TASCAR::session_t::get_active_diffuse_sound_fields() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it)
    rv += (*it)->active_diffuse_sound_fields;
  return rv;
}

uint32_t TASCAR::session_t::get_total_diffuse_sound_fields() const
{
  uint32_t rv(0);
  for( std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it)
    rv += (*it)->total_diffuse_sound_fields;
  return rv;
}

TASCAR::range_t::range_t(xmlpp::Element* xmlsrc)
  : xml_element_t(xmlsrc),
    name(""),
    start(0),
    end(0)
{
  GET_ATTRIBUTE(name);
  GET_ATTRIBUTE(start);
  GET_ATTRIBUTE(end);
}

TASCAR::connection_t::connection_t(xmlpp::Element* xmlsrc)
  : xml_element_t(xmlsrc),
    failonerror(false)
{
  get_attribute("src", src);
  get_attribute("dest", dest);
  GET_ATTRIBUTE_BOOL(failonerror);
}

TASCAR::module_base_t::module_base_t(const TASCAR::module_cfg_t& cfg)
  : xml_element_t(cfg.xmlsrc),
    licensed_component_t(typeid(*this).name()),
      session(cfg.session)
{
}

void TASCAR::module_base_t::update(uint32_t frame,bool running)
{
}

TASCAR::module_base_t::~module_base_t()
{
}

std::vector<TASCAR::named_object_t> TASCAR::session_t::find_objects(const std::string& pattern)
{
  std::vector<TASCAR::named_object_t> retv;
  for(std::vector<TASCAR::scene_render_rt_t*>::iterator sit=scenes.begin();sit!=scenes.end();++sit){
    std::vector<TASCAR::Scene::object_t*> objs((*sit)->get_objects());
    std::string base("/" + (*sit)->name + "/");
    for(std::vector<TASCAR::Scene::object_t*>::iterator it=objs.begin();it!=objs.end();++it){
      std::string name(base + (*it)->get_name());
#ifdef WIN32
      if(fnmatch_win32(pattern.c_str(), name.c_str()))
        retv.push_back(TASCAR::named_object_t(*it, name));
#else
      if(fnmatch(pattern.c_str(), name.c_str(), FNM_PATHNAME) == 0)
        retv.push_back(TASCAR::named_object_t(*it, name));
#endif
    }
  }
  return retv;
}

std::vector<TASCAR::Scene::audio_port_t*> TASCAR::session_t::find_audio_ports(const std::vector<std::string>& pattern)
{
  std::vector<TASCAR::Scene::audio_port_t*> all_ports;
  // first get all audio ports from scenes:
  for(std::vector<TASCAR::scene_render_rt_t*>::iterator sit=scenes.begin();sit!=scenes.end();++sit){
    std::vector<TASCAR::Scene::object_t*> objs((*sit)->get_objects());
    // std::string base("/"+(*sit)->name+"/");
    for(std::vector<TASCAR::Scene::object_t*>::iterator it=objs.begin();it!=objs.end();++it){
      // check if this object is derived from audio_port_t:
      TASCAR::Scene::audio_port_t* p_ap(dynamic_cast<TASCAR::Scene::audio_port_t*>(*it));
      if(p_ap)
        all_ports.push_back(p_ap);
      // If this is a source, then check sound vertices:
      TASCAR::Scene::src_object_t* p_src(dynamic_cast<TASCAR::Scene::src_object_t*>(*it));
      if(p_src) {
        for( std::vector<TASCAR::Scene::sound_t*>::iterator it=p_src->sound.begin(); it!=p_src->sound.end();++it ){
          TASCAR::Scene::audio_port_t* p_ap(dynamic_cast<TASCAR::Scene::audio_port_t*>(*it));
          if(p_ap)
            all_ports.push_back(p_ap);
        }
      }
    }
  }
  // now test for all modules which implement audio_port_t:
  for(std::vector<TASCAR::module_t*>::iterator it=modules.begin();it!= modules.end();++it){
    TASCAR::Scene::audio_port_t* p_ap(dynamic_cast<TASCAR::Scene::audio_port_t*>((*it)->libdata));
    if(p_ap)
      all_ports.push_back(p_ap);
  }
  std::vector<TASCAR::Scene::audio_port_t*> retv;
  // first, iterate over all pattern elements:
  for( auto i_pattern=pattern.begin();i_pattern!=pattern.end();++i_pattern){
    for(auto p_ap = all_ports.begin(); p_ap != all_ports.end(); ++p_ap) {
      // check if name is matching:
      std::string name((*p_ap)->get_ctlname());
#ifdef WIN32
      if((fnmatch_win32(i_pattern->c_str(), name.c_str())) ||
         (*i_pattern == "*"))
        retv.push_back(*p_ap);
#else
      if((fnmatch(i_pattern->c_str(), name.c_str(), FNM_PATHNAME) == 0) ||
         (*i_pattern == "*"))
        retv.push_back(*p_ap);
#endif
    }
  }
  return retv;
}

TASCAR::actor_module_t::actor_module_t( const TASCAR::module_cfg_t& cfg, bool fail_on_empty)
    : module_base_t(cfg)
{
  GET_ATTRIBUTE(actor);
  for(auto act : actor) {
    std::vector<TASCAR::named_object_t> lobj = session->find_objects(act);
    for(auto o : lobj)
      obj.push_back(o);
  }
  if(fail_on_empty && obj.empty())
    throw TASCAR::ErrMsg("No object matches actor pattern \""+vecstr2str(actor)+"\".");
}

TASCAR::actor_module_t::~actor_module_t()
{
}

void TASCAR::actor_module_t::set_location(const TASCAR::pos_t& l, bool b_local)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it){
    if(b_local) {
      TASCAR::zyx_euler_t o(it->obj->get_orientation());
      TASCAR::pos_t p(l);
      p *= o;
      it->obj->dlocation = p;
    } else {
      it->obj->dlocation = l;
    }
  }
}

void TASCAR::actor_module_t::set_orientation(const TASCAR::zyx_euler_t& o)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dorientation = o;
}

void TASCAR::actor_module_t::set_transformation( const TASCAR::c6dof_t& tf, bool b_local )
{
  set_orientation(tf.orientation);
  set_location(tf.position, b_local);
}

void TASCAR::actor_module_t::add_location(const TASCAR::pos_t& l, bool b_local)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it){
    if(b_local) {
      TASCAR::zyx_euler_t o(it->obj->get_orientation());
      TASCAR::pos_t p(l);
      p *= o;
      it->obj->dlocation += p;
    } else {
      it->obj->dlocation += l;
    }
  }
}

void TASCAR::actor_module_t::add_orientation(const TASCAR::zyx_euler_t& o)
{
  for(std::vector<TASCAR::named_object_t>::iterator it=obj.begin();it!=obj.end();++it)
    it->obj->dorientation += o;
}

void TASCAR::actor_module_t::add_transformation( const TASCAR::c6dof_t& tf, bool b_local )
{
  add_orientation(tf.orientation);
  add_location(tf.position, b_local);
}

namespace OSCSession {

  int _addtime(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 1) && (types[0] == 'f')) {
      double cur_time(std::max(0.0,std::min(((TASCAR::session_t*)user_data)->duration,((TASCAR::session_t*)user_data)->tp_get_time())));
      ((TASCAR::session_t*)user_data)->tp_locate(cur_time + argv[0]->f);
      return 0;
    }
    return 1;
  }


  int _locate(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 1) && (types[0] == 'f')) {
      ((TASCAR::session_t*)user_data)->tp_locate(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _locatei(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 1) && (types[0] == 'i')) {
      ((TASCAR::session_t*)user_data)->tp_locate((uint32_t)(argv[0]->i));
      return 0;
    }
    return 1;
  }

  int _stop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 0)) {
      ((TASCAR::session_t*)user_data)->tp_stop();
      return 0;
    }
    return 1;
  }

  int _start(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 0)) {
      ((TASCAR::session_t*)user_data)->tp_start();
      return 0;
    }
    return 1;
  }

  int _playrange(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
      ((TASCAR::session_t*)user_data)->tp_playrange(argv[0]->f, argv[1]->f);
      return 0;
    }
    return 1;
  }

  int _unload_modules(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if((argc == 0)) {
      ((TASCAR::session_t*)user_data)->unload_modules();
      return 0;
    }
    return 0;
  }

}

void TASCAR::session_t::add_transport_methods()
{
  osc_server_t::add_method("/transport/locate", "f", OSCSession::_locate, this);
  osc_server_t::add_method("/transport/locatei","i",OSCSession::_locatei,this);
  osc_server_t::add_method("/transport/addtime","f",OSCSession::_addtime,this);
  osc_server_t::add_method("/transport/start", "", OSCSession::_start, this);
  osc_server_t::add_method("/transport/playrange","ff",OSCSession::_playrange,this);
  osc_server_t::add_method("/transport/stop", "", OSCSession::_stop, this);
  osc_server_t::add_method("/transport/unload","",OSCSession::_unload_modules,this);
}

void TASCAR::session_t::validate_attributes(std::string& msg) const
{
  TASCAR::tsc_reader_t::validate_attributes(msg);
  for(std::vector<TASCAR::scene_render_rt_t*>::const_iterator it=scenes.begin();it!=scenes.end();++it)
    (*it)->validate_attributes(msg);
  for(std::vector<TASCAR::range_t*>::const_iterator it=ranges.begin();it!=ranges.end();++it)
    (*it)->validate_attributes(msg);
  for(std::vector<TASCAR::connection_t*>::const_iterator it=connections.begin();
      it != connections.end(); ++it)
    (*it)->validate_attributes(msg);
  for(std::vector<TASCAR::module_t*>::const_iterator it=modules.begin();it!=modules.end();++it)
    (*it)->validate_attributes(msg);
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
