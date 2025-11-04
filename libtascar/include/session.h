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
#ifndef SESSION_H
#define SESSION_H

#include "jackrender.h"
#include "session_reader.h"

namespace TASCAR {

  class session_t;

  class module_cfg_t {
  public:
    module_cfg_t(tsccfg::node_t xmlsrc, TASCAR::session_t* session_);
    TASCAR::session_t* session;
    tsccfg::node_t xmlsrc;
  };

  class module_base_t : public xml_element_t,
                        public audiostates_t,
                        public licensed_component_t {
  public:
    module_base_t(const module_cfg_t& cfg);
    virtual ~module_base_t();
    /**
       \brief Update geometry etc on each processing cycle in the session
       processing thread. \callgraph \callergraph

       This method will be called after scene geometry update and
       before acoustic model update and audio rendering.

       \param frame Transport time (in samples).
       \param running Transport running (true) or stopped (false).
     */
    virtual void update(uint32_t frame, bool running);

  protected:
    TASCAR::session_t* session;
  };

  class module_t : public module_base_t {
  public:
    module_t(const TASCAR::module_cfg_t& cfg);
    virtual ~module_t();
    void configure();
    void post_prepare();
    void release();
    void update(uint32_t frame, bool running);
    virtual void validate_attributes(std::string&) const;
    const std::string& modulename() const { return name; };

  private:
    std::string name;
    void* lib;

  public:
    TASCAR::module_base_t* libdata;
  };

  class connection_t : public TASCAR::xml_element_t {
  public:
    connection_t(tsccfg::node_t);
    std::string src;
    std::string dest;
    bool failonerror;
  };

  class range_t : public xml_element_t {
  public:
    range_t(tsccfg::node_t e);
    std::string name;
    double start;
    double end;
  };

  class named_object_t {
  public:
    named_object_t(TASCAR::Scene::object_t* o, const std::string& n,
                   TASCAR::Scene::scene_t* s)
        : obj(o), name(n), scene(s){};
    TASCAR::Scene::object_t* obj;         //< pointer to object
    std::string name;                     //< name of object
    TASCAR::Scene::scene_t* scene = NULL; //< scene containing this object;
  };

  class session_oscvars_t : public TASCAR::xml_element_t {
  public:
    session_oscvars_t(tsccfg::node_t src);
    std::string name;
    std::string srv_port;
    std::string srv_addr;
    std::string srv_proto;
    std::string starturl;
  };

  class session_core_t : public TASCAR::tsc_reader_t {
  public:
    session_core_t();
    virtual ~session_core_t();
    session_core_t(const std::string& filename_or_data, load_type_t t,
                   const std::string& path);
    // configuration variables:
    double duration = 60;
    bool loop = false;
    bool playonload = false;
    double locateonload = -1.0;
    double levelmeter_tc = 2.0;
    TASCAR::levelmeter::weight_t levelmeter_weight = TASCAR::levelmeter::Z;
    std::string levelmeter_mode;
    double levelmeter_min = 30.0;
    double levelmeter_range = 70.0;
    double requiresrate = 0.0;
    double warnsrate = 0.0;
    int32_t requirefragsize = 0;
    int32_t warnfragsize = 0;
    std::string initcmd;
    double initcmdsleep = 0.0;

  private:
    void start_initcmd();
    FILE* h_pipe_initcmd;
    pid_t pid_initcmd;
  };

  class session_t : public session_core_t,
                    public session_oscvars_t,
                    public jackc_transport_t,
                    public TASCAR::osc_server_t {
  public:
    session_t();
    session_t(const std::string& filename_or_data, load_type_t t,
              const std::string& path);

  private:
    session_t(const session_t& src);

  public:
    virtual ~session_t();
    void add_scene(tsccfg::node_t);
    void add_range(tsccfg::node_t);
    void add_connection(tsccfg::node_t);
    void add_module(tsccfg::node_t);
    void start();
    void stop();
    void run(bool& b_quit, bool use_stdin = true);
    double get_duration() const { return duration; };
    uint32_t get_active_pointsources() const;
    uint32_t get_total_pointsources() const;
    uint32_t get_active_diffuse_sound_fields() const;
    uint32_t get_total_diffuse_sound_fields() const;
    std::vector<TASCAR::named_object_t>
    find_objects(const std::string& pattern);
    std::vector<TASCAR::named_object_t>
    find_objects(const std::vector<std::string>& pattern);
    std::vector<TASCAR::Scene::audio_port_t*>
    find_audio_ports(const std::vector<std::string>& pattern);
    std::vector<TASCAR::Scene::audio_port_t*>
    find_route_ports(const std::vector<std::string>& pattern);
    std::vector<TASCAR::scene_render_rt_t*> scenes;
    std::vector<TASCAR::range_t*> ranges;
    std::vector<TASCAR::connection_t*> connections;
    std::vector<TASCAR::module_t*> modules;
    std::vector<std::string> get_render_output_ports() const;
    virtual int process(jack_nframes_t nframes,
                        const std::vector<float*>& inBuffer,
                        const std::vector<float*>& outBuffer, uint32_t tp_frame,
                        bool tp_rolling);
    void unload_modules();
    bool lock_vars();
    void unlock_vars();
    bool trylock_vars();
    bool is_running() { return started_; };
    virtual void validate_attributes(std::string&) const;
    TASCAR::scene_render_rt_t& scene_by_id(const std::string& id);
    TASCAR::Scene::sound_t& sound_by_id(const std::string& id);
    TASCAR::Scene::src_object_t& source_by_id(const std::string& id);
    TASCAR::Scene::receiver_obj_t& receiver_by_id(const std::string& id);
    void send_xml(const std::string& url, const std::string& path);
    void send_routes(const std::string& url, const std::string& path);

  protected:
    // derived variables:
    std::string session_path;

  private:
    void add_transport_methods();
    void read_xml();
    double period_time;
    bool started_;
    pthread_mutex_t mtx;
    std::set<std::string> namelist;
    std::map<std::string, TASCAR::scene_render_rt_t*> scenemap;
    std::map<std::string, TASCAR::Scene::sound_t*> soundmap;
    std::map<std::string, TASCAR::Scene::src_object_t*> sourcemap;
    std::map<std::string, TASCAR::Scene::receiver_obj_t*> receivermap;
    //
    TASCAR::tictoc_t tictoc;
    lo_message profilermsg;
    lo_arg** profilermsgargv;
    std::vector<std::string> initoscscript;
  };

  /// Control 'actors' in a scene
  class actor_module_t : public module_base_t {
  public:
    actor_module_t(const TASCAR::module_cfg_t& cfg, bool fail_on_empty = false);
    virtual ~actor_module_t();
    /**@brief Set delta location of all actors
       @param l new delta location
       @param b_local Apply in local coordinates (true) or in global coordinates
       (false)
     */
    void set_location(const TASCAR::pos_t& l, bool b_local = false);
    void set_orientation(const TASCAR::zyx_euler_t& o, bool b_local = false);
    void set_transformation(const TASCAR::c6dof_t& tf, bool b_local = false);
    void add_location(const TASCAR::pos_t& l, bool b_local);
    void add_orientation(const TASCAR::zyx_euler_t& o);
    void add_transformation(const TASCAR::c6dof_t& tf, bool b_local = false);

  protected:
    /**
       \brief Actor name pattern
     */
    std::vector<std::string> actor;
    /**
       \brief List of matching actor objects
     */
    std::vector<TASCAR::named_object_t> obj;
  };

} // namespace TASCAR

#define REGISTER_MODULE(x) TASCAR_PLUGIN(module_base_t, const module_cfg_t&, x)

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
