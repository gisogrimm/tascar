/**
 * @file scene.h
 * @brief Scene definition without rendering functionality
 * @ingroup libtascar
 * @author Giso Grimm
 * @date 2013
 */
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
#ifndef SCENE_H
#define SCENE_H

#include "acousticmodel.h"

namespace TASCAR {

  /** \brief Components relevant for the spatial modelling
   */
  namespace Scene {

    class material_t : public xml_element_t {
    public:
      material_t(tsccfg::node_t);
      material_t();
      material_t(const std::string& name, const std::vector<float>& f,
                 const std::vector<float>& alpha);
      ~material_t();
      void update_coeff(float fs);
      void validate();
      std::string name = "plaster";
      std::vector<float> f = {125.0f,  250.0f,  500.0f,
                              1000.0f, 2000.0f, 4000.0f};
      std::vector<float> alpha = {0.013f, 0.015f, 0.02f, 0.03f, 0.04f, 0.05};
      float reflectivity = 1.0f;
      float damping = 0.0f;
    };

    class object_with_name_and_id_t {
    public:
      object_with_name_and_id_t(tsccfg::node_t);
      std::string get_name() const { return name; };
      std::string get_id() const { return id; };
      std::string default_name(const std::string& s)
      {
        if(name.empty())
          name = s;
        return name;
      }
      void set_name(const std::string& s) { name = s; };

    private:
      std::string name;
      std::string id;
    };

    /**
       \brief some magic super-class

       Activity control (mute/solo)

       Name and ID configuration

       Level metering for user display

       This class is inherited by all objects of a scene.
     */
    class route_t : public xml_element_t, public object_with_name_and_id_t {
    public:
      route_t(tsccfg::node_t);
      ~route_t();
      bool get_mute() const { return mute; };
      bool get_solo() const { return solo; };
      void set_mute(bool b) { mute = b; };
      void set_solo(bool b, uint32_t& anysolo);
      /**
         \brief Return combination of mute and solo.

         Internal solo state is used only if anysolo is true.

         \param anysolo Counter of objects which are soloed.
         \return True if active, false if either muted or not soloed but other
         tracks are soloed.
      */
      bool is_active(uint32_t anysolo);
      void addmeter(float fs);
      void configure_meter(float tc, TASCAR::levelmeter::weight_t w);
      void set_meterweight(TASCAR::levelmeter::weight_t w);
      uint32_t metercnt() const { return (uint32_t)rmsmeter.size(); };
      void reset_meters();
      const std::vector<float>& readmeter();
      float read_meter_max();
      float get_meterval(uint32_t k) const { return meterval[k]; };
      std::string get_type() const;
      /**
         \brief Return a rference to the level meter
         \param k Channel number
         \return Level meter
         \ingroup levels
      */
      const TASCAR::levelmeter_t& get_meter(uint32_t k) const
      {
        return *(rmsmeter[k]);
      };

    public:
      bool mute;

    private:
      bool solo;
      float meter_tc;
      TASCAR::levelmeter::weight_t meter_weight;

    public:
      float targetlevel;

    protected:
      /**
         \ingroup levels
      */
      std::vector<TASCAR::levelmeter_t*> rmsmeter;
      std::vector<float> meterval;
    };

    class rgb_color_t {
    public:
      rgb_color_t(double r_, double g_, double b_) : r(r_), g(g_), b(b_){};
      rgb_color_t(const std::string& webc);
      rgb_color_t() : r(0), g(0), b(0){};
      std::string str();
      double r, g, b;
    };

    class object_t : public dynobject_t, public route_t {
    public:
      object_t(tsccfg::node_t);
      virtual ~object_t(){};
      bool isactive(double time) const;
      bool is_active(uint32_t anysolo, double t);
      rgb_color_t color;
      double endtime;
      float scale = 1.0f;
    };

    class face_object_t : public object_t,
                          public TASCAR::Acousticmodel::reflector_t {
    public:
      face_object_t(tsccfg::node_t xmlsrc);
      virtual ~face_object_t();
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      double width;
      double height;
      std::vector<TASCAR::pos_t> vertices;
    };

    class face_group_t : public object_t,
                         public TASCAR::Acousticmodel::reflector_t {
    public:
      face_group_t(tsccfg::node_t xmlsrc);
      virtual ~face_group_t();
      void geometry_update(double t);
      void process_active(double t, uint32_t anysolo);
      std::vector<TASCAR::Acousticmodel::reflector_t*> reflectors;
      std::string importraw;
      TASCAR::pos_t shoebox;
      TASCAR::pos_t shoeboxwalls;
      TASCAR::pos_t shoeboxfloor;
      TASCAR::pos_t shoeboxceiling;
    };

    class obstacle_group_t : public object_t {
    public:
      obstacle_group_t(tsccfg::node_t xmlsrc);
      virtual ~obstacle_group_t();
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      std::vector<TASCAR::Acousticmodel::obstacle_t*> obstacles;
      float transmission;
      std::string importraw;
      bool ishole;
      float aperture;
    };

    class src_object_t;

    /**
       \brief Audio ports (anything which can receive or send audio,
       e.g., to jack) with gain controls and calibration
    */
    class audio_port_t : public TASCAR::xml_element_t {
    public:
      audio_port_t(tsccfg::node_t e, bool is_input_);
      virtual ~audio_port_t();
      virtual uint32_t get_num_input_ports() = 0;
      virtual uint32_t get_num_output_ports() = 0;
      void set_port_index(uint32_t port_index_);
      uint32_t get_port_index() const { return port_index; };
      void set_ctlname(const std::string& pn) { ctlname = pn; };
      std::string get_ctlname() const { return ctlname; };
      std::vector<std::string> get_connect() const { return connect; };
      bool get_isinput() const { return is_input; };
      float get_gain() const
      {
        if(is_input)
          return gain * caliblevel;
        else
          return gain / caliblevel;
      };
      float get_gain_db() const { return 20 * log10(fabsf(gain)); };
      void set_gain_db(float g);
      void set_gain_lin(float g);
      bool get_inv() const { return gain < 0.0f; };
      void set_inv(bool inv);

    private:
      std::string ctlname;
      std::vector<std::string> connect;
      uint32_t port_index;
      const bool is_input;

    public:
      float gain;
      float caliblevel;
      bool has_caliblevel;
    };

    /**
       \brief Diffuse sound field descriptor
    */
    class diff_snd_field_obj_t : public object_t,
                                 public audio_port_t,
                                 public licensed_component_t,
                                 public audiostates_t {
    public:
      diff_snd_field_obj_t(tsccfg::node_t e);
      virtual ~diff_snd_field_obj_t();
      void configure();
      void post_prepare();
      void release();
      uint32_t get_num_input_ports();
      uint32_t get_num_output_ports();
      void validate_attributes(std::string& msg) const;
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      pos_t size;
      float falloff;
      TASCAR::Acousticmodel::diffuse_t* get_source() { return source; };
      void add_licenses(licensehandler_t*);
      uint32_t layers;

    private:
      TASCAR::Acousticmodel::diffuse_t* source;

    public:
      plugin_processor_t plugins;
    };

    class sound_name_t : public object_with_name_and_id_t {
    public:
      sound_name_t(tsccfg::node_t e, src_object_t* parent_);
      std::string get_parent_name() const { return parentname; };
      std::string get_fullname() const { return parentname + "." + get_name(); };

    private:
      std::string parentname;
    };

    class sound_t : public sound_name_t,
                    public TASCAR::Acousticmodel::source_t,
                    public audio_port_t {
    public:
      sound_t(tsccfg::node_t e, src_object_t* parent_);
      virtual ~sound_t();
      rgb_color_t get_color() const;
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_plugins(const TASCAR::transport_t& tp);
      uint32_t get_num_input_ports();
      uint32_t get_num_output_ports();
      void apply_gain();
      // void prepare( chunk_cfg_t& );
      // void release();
      void add_meter(TASCAR::levelmeter_t*);
      float read_meter();
      void validate_attributes(std::string& msg) const;
      void set_mute(bool m) { b_mute = m; };
      bool get_mute() const { return b_mute; };

    public:
      src_object_t* parent;

      pos_t local_position;
      zyx_euler_t local_orientation;
      pos_t global_position;
      bool b_mute = false;

    private:
      double chaindist;
      float gain_;
      std::vector<TASCAR::levelmeter_t*> meter;
    };

    class src_object_t : public object_t,
                         public licensed_component_t,
                         public audiostates_t {
    public:
      src_object_t(tsccfg::node_t e);
      ~src_object_t();
      void configure();
      void post_prepare();
      void release();
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      void add_sound(tsccfg::node_t src);
      std::vector<sound_t*> sound;
      std::string next_sound_name() const;
      void validate_attributes(std::string& msg) const;
      void add_licenses(licensehandler_t*);
      sound_t& sound_by_id(const std::string& id);

    private:
      int32_t startframe;
      std::map<std::string, sound_t*> soundmap;
    };

    /**
       \brief Combine acoustic receiver functionality with dynamic geometry and
       audio port control
    */
    class receiver_obj_t : public object_t,
                           public audio_port_t,
                           public TASCAR::Acousticmodel::receiver_t {
    public:
      receiver_obj_t(tsccfg::node_t e, bool is_reverb_);
      void configure();
      void release();
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      /**
         \callgraph
         \callergraph
      */
      virtual void postproc(std::vector<wave_t>& output);
      void validate_attributes(std::string& msg) const;
      uint32_t get_num_input_ports();
      uint32_t get_num_output_ports();
    };

    class mask_object_t : public object_t,
                          public TASCAR::Acousticmodel::mask_t {
    public:
      mask_object_t(tsccfg::node_t e);
      /**
         \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \callgraph
         \callergraph
      */
      void process_active(double t, uint32_t anysolo);
      pos_t xmlsize;
      double xmlfalloff;
    };

    class diffuse_reverb_defaults_t {
    public:
      diffuse_reverb_defaults_t(tsccfg::node_t e);
    };

    /**
       \brief Object for diffuse reverb, consists of room microphone, diffuse
       sound field source
     */
    class diffuse_reverb_t : public diffuse_reverb_defaults_t,
                             public receiver_obj_t {
    public:
      diffuse_reverb_t(tsccfg::node_t e);
      ~diffuse_reverb_t();
      void configure();
      void post_prepare();
      void release();
      void geometry_update(double t);
      void process_active(double t, uint32_t anysolo);
      TASCAR::Acousticmodel::diffuse_t* get_source() { return source; };
      void add_licenses(licensehandler_t*);
      void validate_attributes(std::string& msg) const;
      uint32_t get_num_output_ports() { return 0u; };

    private:
      uint32_t outputlayers;
      TASCAR::Acousticmodel::diffuse_t* source;

    public:
      plugin_processor_t plugins;
    };

    /**
       \brief The class scene_t implements the definition of a virtual acoustic
       environment.

       Here, objects (object_t, member scene_t::all_objects) are
       defined, which can move along trajectories. This class is
       mainly responsible for updating the object positions, see
       scene_t::geometry_update(). Rendering methods are defined in
       the derived class TASCAR::render_core_t. The actual acoustic
       representation of the virtual acoustic environment is
       implemented in the class TASCAR::Acousticmodel::world_t,
       allocated in TASCAR::render_core_t::world.
    */
    class scene_t : public xml_element_t,
                    public audiostates_t,
                    public licensed_component_t {
    public:
      scene_t(tsccfg::node_t e);
      ~scene_t();
      src_object_t* add_source();
      void configure();
      void post_prepare();
      void release();
      std::string description;
      std::string name;
      std::string id;
      double c;
      /**
         \brief Update geometry of all objects within a scene based on current
         transport time \param t Transport time \ingroup callgraph \callgraph
         \callergraph
      */
      void geometry_update(double t);
      /**
         \brief Update activity flags based on current transport time
         \param t Transport time
         \ingroup callgraph
         \callgraph
         \callergraph
      */
      void process_active(double t);
      std::map<std::string, TASCAR::Scene::material_t> materials;
      std::vector<sound_t*> sounds;
      std::map<std::string, sound_t*> soundmap;
      sound_t& sound_by_id(const std::string& id);
      std::vector<src_object_t*> source_objects;
      std::vector<diff_snd_field_obj_t*> diff_snd_field_objects;
      std::vector<face_object_t*> face_objects;
      std::vector<face_group_t*> facegroups;
      std::vector<obstacle_group_t*> obstaclegroups;
      std::vector<receiver_obj_t*> receivermod_objects;
      std::vector<mask_object_t*> mask_objects;
      std::vector<diffuse_reverb_t*> diffuse_reverbs;
      std::vector<object_t*> all_objects;
      std::vector<object_t*> find_object(const std::string& pattern);
      uint32_t ismorder;
      double guiscale;
      pos_t guicenter;
      TASCAR::Scene::object_t* guitrackobject;
      uint32_t anysolo;
      std::vector<object_t*> get_objects();
      std::string scene_path;
      void configure_meter(float tc, TASCAR::levelmeter::weight_t w);
      void add_licenses(licensehandler_t* session);
      void validate_attributes(std::string& msg) const;
      bool active;
      std::mutex mtx_geometry;

    private:
      void clean_children();
      scene_t(const scene_t&);
      std::set<std::string> namelist;
    };

  } // namespace Scene

} // namespace TASCAR

std::string jacknamer(const std::string& prefix, const std::string& name);

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
