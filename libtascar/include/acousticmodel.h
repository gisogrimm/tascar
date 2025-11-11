/**
 * @file   acousticmodel.h
 * @author Giso Grimm
 *
 * @brief  The core of TASCAR acoustic modeling
 *
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
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
 *
 */

#ifndef ACOUSTICMODEL_H
#define ACOUSTICMODEL_H

#include "dynamicobjects.h"
#include "fdn.h"
#include "levelmeter.h"
#include "maskplugin.h"
#include "pluginprocessor.h"
#include "receivermod.h"
#include "sourcemod.h"

/*

  new delayline concept:

  source_base_t is base class of primary_source_t and
  image_source_t. primary_source_t owns a delayline and feeds into
  it. In "add_pointsource" audio will be read from delayline and
  filtered according to acoustic model.

  Delay Q
  Reflection filter R -> reflectors
  Air absorption A -> distance
  Radiation directivity D -> (receiver-source)/O_source orientation
  Receiver panning P -> (source-receiver)/O_receiver orientation

*/

#define FADE_START_NOW 0xFFFFFFFFFFFFFFFFu

namespace TASCAR {

  /**
   * @brief Falloff gain rules
   */
  enum gainmodel_t {
    /// 1/r rule
    GAIN_INVR,
    /// constant gain
    GAIN_UNITY
  };

  /** \brief Components relevant for the acoustic modelling
   */
  namespace Acousticmodel {

    /**
       \brief Diffraction model
    */
    class diffractor_t : public ngon_t {
    public:
      class state_t {
      public:
        double A1;
        double s1;
        double s2;
        state_t() : A1(0), s1(0), s2(0){};
      };
      diffractor_t() : b_inner(true), manual_aperture(0){};
      /**
         \brief Apply diffraction model

         \param p_src Source position
         \param p_rec Receiver position
         \param audio Audio chunk
         \param c Speed of sound
         \param fs Sampling rate
         \param state Diffraction filter states
         \param drywet Direct-to-diffracted ratio

         \return Effective source position

         \ingroup callgraph
      */
      pos_t process(pos_t p_src, const pos_t& p_rec, wave_t& audio, float c,
                    float fs, state_t& state, float drywet);
      /**
         \brief Flag, true if the diffraction is caused by the inner
         part of the object (limited surface), false if the
         diffraction is created by the outer part (infinite plane with
         polygon opening)
      */
      bool b_inner;
      float manual_aperture;
    };

    /**
       \brief Diffuse sound field
     */
    class diffuse_t : public shoebox_t,
                      public TASCAR::xml_element_t,
                      public audiostates_t,
                      public licensed_component_t {
    public:
      diffuse_t(tsccfg::node_t cfg, uint32_t chunksize,
                TASCAR::levelmeter_t& rmslevel_, const std::string& name,
                plugin_processor_t& plugins_);
      virtual ~diffuse_t(){};
      virtual void preprocess(const TASCAR::transport_t& tp);
      void configure();
      void post_prepare();
      void release();
      void add_licenses(licensehandler_t*);
      amb1rotator_t audio;
      float falloff;
      bool active;
      uint32_t layers;
      TASCAR::levelmeter_t& rmslevel;

    private:
      plugin_processor_t& plugins;
    };

    class boundingbox_t : public dynobject_t {
    public:
      boundingbox_t(tsccfg::node_t);
      pos_t size;
      float falloff;
      bool active;
    };

    class mask_t : public shoebox_t {
    public:
      mask_t();
      float gain(const pos_t& p);
      float inv_falloff;
      bool mask_inner;
      bool active;
    };

    /**
       \brief Primary sound source
     */
    class source_t : public sourcemod_t,
                     public c6dof_t,
                     public licensed_component_t {
    public:
      source_t(tsccfg::node_t xmlsrc, const std::string& name,
               const std::string& parentname);
      ~source_t();
      void configure();
      void post_prepare();
      void release();
      virtual void process_plugins(const TASCAR::transport_t& tp);
      void add_licenses(licensehandler_t*);
      uint32_t ismmin;
      uint32_t ismmax;
      uint32_t layers;
      float maxdist;
      float minlevel;
      float nearfieldlimit;
      uint32_t sincorder;
      gainmodel_t gainmodel;
      bool airabsorption;
      bool delayline;
      float size;
      // derived / internal / updated variables:
      std::vector<wave_t> inchannels;
      std::vector<wave_t*> inchannelsp;
      bool active;
      plugin_processor_t plugins;
    };

    /**
       \brief Receiver (or end point) of the acoustic model

       Provides output for the render method implementation

     */
    class receiver_t : public receivermod_t,
                       public c6dof_t,
                       public licensed_component_t {
    public:
      receiver_t(tsccfg::node_t xmlsrc, const std::string& name,
                 bool is_reverb_);
      ~receiver_t();
      void configure();
      void post_prepare();
      void release();
      void clear_output();
      void add_pointsource_with_scattering(const pos_t& prel, float width,
                                           float scattering,
                                           const wave_t& chunk,
                                           receivermod_base_t::data_t*);
      void add_diffuse_sound_field_rec(const amb1wave_t& chunk,
                                       receivermod_base_t::data_t*);
      /**
       * @brief Calculate parameters for panning, delayline and air absorption
       * @param psrc_physical Position of physical (primary) sound source
       * @param psrc_virtual Position of image source, or physical position if
       * primary source
       * @retval prel Position relative to receiver
       * @retval distance Distance for air absorption
       * @retval traveltime Travel time for delay lines, in meter
       * @retval gain Gain from distance law
       * @param b_img Flag to indicate if the source is an image source (true)
       * or a primary source (false)
       * @param gainmodel Type of gain model, e.g., 1/r
       */
      void update_refpoint(const pos_t& psrc_physical,
                           const pos_t& psrc_virtual, pos_t& prel,
                           float& distance, float& traveltime_in_m, float& gain,
                           bool b_img, gainmodel_t gainmodel,
                           float& nearfieldlimit);
      void set_next_gain(float gain);
      void set_fade(float targetgain, float duration, float start = -1);
      void apply_gain();
      virtual void postproc(std::vector<wave_t>& output);
      void post_proc(const TASCAR::transport_t& tp);
      // virtual void process_plugins(const TASCAR::transport_t& tp);
      virtual void add_variables(TASCAR::osc_server_t* srv);
      void validate_attributes(std::string& msg) const;
      void add_licenses(licensehandler_t*);
      // configuration/control variables:
      TASCAR::pos_t volumetric;
      bool volumetricgainwithdistance = false;
      float avgdist = 0.0f;
      bool render_point = true;
      bool render_diffuse = true;
      bool render_image = true;
      uint32_t ismmin = 0;
      uint32_t ismmax = 2147483647;
      uint32_t layers = 0xffffffff;
      bool use_global_mask = true;
      bool create_input_ports = false;
      float diffusegain = 1.0f;
      bool has_diffusegain = false;
      float falloff = -1.0f;
      float delaycomp = 0.0f;
      float recdelaycomp = 0.0f;
      float layerfadelen = 1.0f;
      bool muteonstop = false;
      // scatter filter parameters:
      uint32_t scatterreflections = 0;
      float scatterspread = TASCAR_PIf * 0.125;
      float scatterstructuresize = 1;
      float scatterdamping = 0.0;
      // proxy parameters:
      TASCAR::pos_t proxy_position;
      bool proxy_is_relative = false;
      bool proxy_delay = false;
      bool proxy_airabsorption = false;
      bool proxy_gain = false;
      bool proxy_direction = false;
      // derived / internal / updated variables:
      std::vector<wave_t> outchannels;
      std::vector<wave_t*> outchannelsp;
      TASCAR::amb1wave_t* scatterbuffer;
      receivermod_base_t::data_t* scatter_handle;
      bool active = true;
      TASCAR::Acousticmodel::boundingbox_t boundingbox;
      bool gain_zero = false;
      float external_gain = 1.0f;
      const bool is_reverb;

    private:
      // gain state:
      float x_gain;
      // target gain:
      float next_gain;
      // fade timer, is > 0 during fade:
      int32_t fade_timer;
      // time constant for fade:
      float fade_rate;
      // target gain at end of fade:
      float next_fade_gain;
      // current fade gain at time of fade update:
      float previous_fade_gain;
      // preliminary values to have atomic operations (correct?):
      float prelim_next_fade_gain;
      float prelim_previous_fade_gain;
      // current fade gain:
      float fade_gain;
      // start sample of next fade, or FADE_START_NOW
      uint64_t fade_startsample;
      // feed forward path with rotation and all-pass filters for scattering:
      TASCAR::fdn_t* scatterfilter = nullptr;
      std::vector<fdnpath_t> scatterfilterpath;
      std::vector<TASCAR::biquadf_t> scatterallpass_w;
      std::vector<TASCAR::biquadf_t> scatterallpass_x;
      std::vector<TASCAR::biquadf_t> scatterallpass_y;
      std::vector<TASCAR::biquadf_t> scatterallpass_z;

    protected:
      TASCAR::transport_t ltp;
      uint64_t starttime_samples;

    public:
      plugin_processor_t plugins;
      // optional mask plugin
      TASCAR::maskplugin_t* maskplug = nullptr;
    };

    class filter_coeff_t {
    public:
      filter_coeff_t()
      {
        c[0] = 1.0;
        c[1] = 0.0;
      };
      double c[2];
    };

    class obstacle_t : public diffractor_t {
    public:
      obstacle_t();
      bool active;
      float transmission;
    };

    /**
       \brief Acoustic reflector

       Reflectors are a single polygon which reflect sounds arriving
       from the positive face normal side of the surface. Reflectors
       create an image source.
     */
    class reflector_t : public diffractor_t {
    public:
      reflector_t();
      void apply_reflectionfilter(TASCAR::wave_t& audio, float& lpstate) const;
      void read_xml(TASCAR::xml_element_t& e);
      bool active;
      float reflectivity;
      float damping;
      bool edgereflection;
      float scattering;
      std::string material;
      uint32_t layers;
    };

    /**
       \brief The path from one primary or image source to a receiver
     */
    class soundpath_t : public c6dof_t {
    public:
      soundpath_t(
          const source_t* src, const soundpath_t* parent_ = NULL,
          const reflector_t* generator_ =
              NULL); ///< constructor, for primary sources set parent_ to NULL
      void update_position(); ///< Update image source position from parent and
                              ///< reflector
      pos_t get_effective_position(
          const pos_t& receiverp,
          float& gain); ///< correct perceived position and caculate gain
      uint32_t getorder()
          const; ///< Return image source order of sound path, 0 is direct path
      void apply_reflectionfilter(
          TASCAR::wave_t& audio); ///< Apply reflection filter of all reflectors
      const soundpath_t* parent;  ///< Parent sound path (or this if primary)
      const source_t* primary;    ///< Primary source
      const reflector_t* reflector; ///< Reflector, which created new sound path
                                    ///< from parent, or NULL for primary
      std::vector<float>
          reflectionfilterstates; ///< Filter states for first-order reflection
                                  ///< filters
      bool visible;
      pos_t p_cut;
    };

    /** \brief A model for a sound wave propagating from a point source to a
     * receiver
     *
     * Processing includes delay, gain, air absorption, and optional
     * obstacles.
     */
    class acoustic_model_t : public soundpath_t {
    public:
      acoustic_model_t(float c, float fs, uint32_t chunksize, source_t* src,
                       receiver_t* receiver,
                       const std::vector<obstacle_t*>& obstacles =
                           std::vector<obstacle_t*>(0u, NULL),
                       const acoustic_model_t* parent = NULL,
                       const reflector_t* reflector = NULL);
      ~acoustic_model_t();
      /**
       * @brief Read audio from source, process and add to receiver.
       * @ingroup callgraph
       */
      uint32_t process(const TASCAR::transport_t& tp);
      float get_gain() const { return gain; };

    protected:
      float c_;
      float fs_;

    public:
      source_t* src_;
      receiver_t* receiver_;
      // pos_t effective_srcpos;
    protected:
      receivermod_base_t::data_t* receiver_data;
      sourcemod_base_t::data_t* source_data;
      std::vector<obstacle_t*> obstacles_;
      std::vector<diffractor_t::state_t> vstate;
      wave_t audio;
      uint32_t chunksize;
      float dt;
      float distance;
      float gain;
      float dscale;
      float air_absorption;
      varidelay_t delayline;
      float airabsorption_state;
      float layergain;
      float dlayergain;

    public:
      uint32_t ismorder;
    };

    /** \brief A model for a sound wave propagating from a point source to a
     * receiver
     *
     * Processing includes delay, gain, air absorption, and optional
     * obstacles.
     */
    class diffuse_acoustic_model_t {
    public:
      diffuse_acoustic_model_t(float fs, uint32_t chunksize, diffuse_t* src,
                               receiver_t* receiver);
      ~diffuse_acoustic_model_t();
      /** \brief Read audio from source, process and add to receiver.
       */
      uint32_t process(const TASCAR::transport_t& tp);

    protected:
      diffuse_t* src_;
      receiver_t* receiver_;
      receivermod_base_t::data_t* receiver_data;
      amb1rotator_t audio;
      uint32_t chunksize;
      float dt;
      float gain = 1.0f;
      float gainmat[16];
    };

    /**
       \brief Subset of a scene as seen by a single receiver
     */
    class receiver_graph_t {
    public:
      /** \brief Create a graph for one receiver
       */
      receiver_graph_t(float c, float fs, uint32_t chunksize,
                       const std::vector<source_t*>& sources,
                       const std::vector<diffuse_t*>& diffuse_sound_fields,
                       const std::vector<reflector_t*>& reflectors,
                       const std::vector<obstacle_t*>& obstacles,
                       receiver_t* receiver, uint32_t ismorder);
      ~receiver_graph_t();
      void process(const TASCAR::transport_t& tp);
      void process_diffuse(const TASCAR::transport_t& tp);
      uint32_t get_active_pointsource() const { return active_pointsource; };
      uint32_t get_active_diffuse_sound_field() const
      {
        return active_diffuse_sound_field;
      };
      uint32_t get_total_pointsource() const
      {
        return (uint32_t)(acoustic_model.size());
      };
      uint32_t get_total_diffuse_sound_field() const
      {
        return (uint32_t)(diffuse_acoustic_model.size());
      };
      std::vector<acoustic_model_t*> acoustic_model;
      std::vector<diffuse_acoustic_model_t*> diffuse_acoustic_model;
      uint32_t active_pointsource;
      uint32_t active_diffuse_sound_field;
    };

    /** \brief The render model of an acoustic scenario.
     *
     * A world creates a set of acoustic models, one for each
     * combination of a sound source (primary or mirrored) and a receiver.
     */
    class world_t {
    public:
      /** \brief Create a world of acoustic models.
       *
       * \param c Speed of sound in m/s
       * \param fs Sampling rate in Hz
       * \param chunksize Chunk size in Samples
       * \param sources Pointers to primary sound sources
       * \param reflectors Pointers to reflector objects
       * \param receivers Pointers to render receivers
       * \param diffuse_sound_fields List of diffuse sound fields
       * \param obstacles List of obstacles
       * \param masks List of masks
       * \param ismorder Maximum image source model order
       *
       * A mirror model is created from the reflectors and primary sources.
       * An instance of this class is created in
       * TASCAR::render_core_t::prepare().
       */
      world_t(float c, float fs, uint32_t chunksize,
              const std::vector<source_t*>& sources,
              const std::vector<diffuse_t*>& diffuse_sound_fields,
              const std::vector<reflector_t*>& reflectors,
              const std::vector<obstacle_t*>& obstacles,
              const std::vector<receiver_t*>& receivers,
              const std::vector<mask_t*>& masks, uint32_t ismorder);
      ~world_t();
      /** \brief Process the mirror model and all acoustic models.
          \ingroup callgraph
      */
      void process(const TASCAR::transport_t& tp);
      /// Return number of active point sources, including image sources
      uint32_t get_active_pointsource() const { return active_pointsource; };
      /// Return number of active diffuse sound fields
      uint32_t get_active_diffuse_sound_field() const
      {
        return active_diffuse_sound_field;
      };
      /// Return total number of point sources, including image sources
      uint32_t get_total_pointsource() const { return total_pointsource; };
      /// Return total number of diffuse sound fields
      uint32_t get_total_diffuse_sound_field() const
      {
        return total_diffuse_sound_field;
      };
      std::vector<receiver_graph_t*> receivergraphs;
      std::vector<receiver_t*> receivers_;
      std::vector<mask_t*> masks_;
      uint32_t active_pointsource;
      uint32_t active_diffuse_sound_field;
      uint32_t total_pointsource;
      uint32_t total_diffuse_sound_field;
    };

  } // namespace Acousticmodel

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
