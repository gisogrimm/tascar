/**
 * @file   gui_elements.h
 * @author Giso Grimm
 * @ingroup GUI
 * @brief  Components for GUI applications
 */
/**
   @defgroup GUI Graphical user interface
*/
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
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
#ifndef GUI_ELEMENTS_H
#define GUI_ELEMENTS_H

#include "session.h"
#include "viewport.h"
#include <cairomm/context.h>
#include <gtkmm.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

/// Name space for graphical user interface components
namespace TSCGUI {

  class playertimeline_t : public Gtk::DrawingArea {
  public:
    playertimeline_t();
  };

  class dameter_t : public Gtk::DrawingArea {
  public:
    dameter_t();
    void invalidate_win();
    enum mode_t { rmspeak, rms, peak, percentile };
    mode_t mode;
    bool narrow;
    bool narrowleg;
    float v_rms;
    float v_peak;
    float q30;
    float q50;
    float q65;
    float q95;
    float q99;
    float vmin;
    float range;
    float targetlevel;
    TASCAR::levelmeter::weight_t weight;
    bool active;

  private:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  };

  /**
     \brief Display of levels
     \ingroup levels
  */
  class splmeter_t : public Gtk::Frame {
  public:
    splmeter_t();
    void update_levelmeter(const TASCAR::levelmeter_t& lm, float targetlevel);
    void set_mode(dameter_t::mode_t mode);
    void set_active(bool);
    void set_weight(TASCAR::levelmeter::weight_t w);
    void set_narrow(bool narrow, bool leg);
    void set_min_and_range(float vmin, float range);
    void invalidate_win();

  private:
    dameter_t dameter;
  };

  class GainScale_t : public Gtk::Scale {
  public:
    GainScale_t();
    float update(bool& inv);
    void set_src(TASCAR::Scene::audio_port_t* ap);
    void on_value_changed();
    void set_inv(bool inv);

  private:
    TASCAR::Scene::audio_port_t* ap_;
    double vmin;
    double vmax;
  };

  class gainctl_t : public Gtk::Frame {
  public:
    gainctl_t();
    void update();
    void set_src(TASCAR::Scene::audio_port_t* ap);
    void set_mute(bool* m = NULL);
    void on_scale_changed();
    void on_text_changed();
    void on_inv_changed();
    void on_mute_changed();

  private:
    Gtk::VBox box;
    // Gtk::CheckButton polarity;
    Gtk::ToggleButton mute;
    Gtk::ToggleButton polarity;
    Gtk::Entry val;
    GainScale_t scale;
    bool* pbmute = NULL;
  };

  class source_ctl_t : public Gtk::VBox {
  public:
    source_ctl_t(lo_address client_addr, TASCAR::Scene::scene_t* s,
                 TASCAR::Scene::route_t* r);
    source_ctl_t(TASCAR::Scene::scene_t* s, TASCAR::Scene::route_t* r);

  private:
    void setup();
    void on_mute();
    void on_solo();

  public:
    void update();
    void set_levelmeter_mode(dameter_t::mode_t mode);
    void set_levelmeter_weight(TASCAR::levelmeter::weight_t w);
    void set_levelmeter_range(float vmin, float range);
    void invalidate_win();
    ~source_ctl_t();

  private:
    Gtk::Frame frame; //< frame for background
    Gtk::EventBox ebox;
    Gtk::VBox box;     //< control elements container
    Gtk::Label tlabel; //< type label
    Gtk::Label label;  //< route label
    Gtk::ToggleButton mute;
    Gtk::ToggleButton solo;
    Gtk::HBox msbox;
    Gtk::HBox meterbox;
    std::vector<splmeter_t*> meters;
    std::vector<gainctl_t*> gainctl;
    playertimeline_t playertimeline;
    lo_address client_addr_;
    std::string name_;
    TASCAR::Scene::scene_t* scene_;

  public:
    TASCAR::Scene::route_t* route_;

  private:
    bool use_osc;
  };

  class source_panel_t : public Gtk::ScrolledWindow {
  public:
    source_panel_t(lo_address client_addr);
    source_panel_t(BaseObjectType* cobject,
                   const Glib::RefPtr<Gtk::Builder>& refGlade);
    void set_scene(TASCAR::Scene::scene_t* scene, TASCAR::session_t* session);
    void set_levelmeter_mode(const std::string& mode);
    void set_levelmeter_range(float vmin, float range);
    void set_levelmeter_weight(TASCAR::levelmeter::weight_t w);
    void update();
    void invalidate_win();
    std::vector<source_ctl_t*> vbuttons;
    Gtk::HBox box;
    lo_address client_addr_;
    bool use_osc;
    dameter_t::mode_t lmode;
  };

  class scene_draw_t {
  public:
    enum viewt_t { xy, xz, yz, xyz, p };
    scene_draw_t();
    virtual ~scene_draw_t();
    void set_scene(TASCAR::render_core_t* scene);
    void select_object(TASCAR::Scene::object_t* o);
    void set_viewport(const scene_draw_t::viewt_t& v);
    virtual void draw(Cairo::RefPtr<Cairo::Context> cr);
    void set_markersize(double msize);
    void set_blink(bool blink);
    void set_time(double t);
    void set_print_labels(bool print_labels);
    void set_show_acoustic_model(bool acmodel);
    double get_time() const { return time; };
    bool draw_edge(Cairo::RefPtr<Cairo::Context> cr, pos_t p1, pos_t p2);

  protected:
    void draw_object(TASCAR::Scene::object_t* obj,
                     Cairo::RefPtr<Cairo::Context> cr);
    virtual void ngon_draw_normal(TASCAR::ngon_t* f,
                                  Cairo::RefPtr<Cairo::Context> cr,
                                  double normalsize, double msize);
    virtual void ngon_draw(TASCAR::ngon_t* f, Cairo::RefPtr<Cairo::Context> cr,
                           bool fill = false, bool area = false);
    virtual void draw_cube(TASCAR::pos_t pos, TASCAR::rotmat_t orient,
                           TASCAR::pos_t size,
                           Cairo::RefPtr<Cairo::Context> cr);
    // object draw functions:
    virtual void draw_track(TASCAR::Scene::object_t* obj,
                            Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_src(TASCAR::Scene::src_object_t* obj,
                          Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_receiver_object(TASCAR::Scene::receiver_obj_t* obj,
                                      Cairo::RefPtr<Cairo::Context> cr,
                                      double msize);
    //  virtual void draw_door_src(TASCAR::Scene::src_door_t*
    //  obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_room_src(TASCAR::Scene::diff_snd_field_obj_t* obj,
                               Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_face(TASCAR::Scene::face_object_t* obj,
                           Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_facegroup(TASCAR::Scene::face_group_t* obj,
                                Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_obstaclegroup(TASCAR::Scene::obstacle_group_t* obj,
                                    Cairo::RefPtr<Cairo::Context> cr,
                                    double msize);
    virtual void draw_mask(TASCAR::Scene::mask_object_t* obj,
                           Cairo::RefPtr<Cairo::Context> cr, double msize);
    virtual void draw_acousticmodel(Cairo::RefPtr<Cairo::Context> cr);
    TASCAR::render_core_t* scene_;

  public:
    viewport_t view;

  protected:
    double time;
    TASCAR::Scene::object_t* selection;
    double markersize;
    bool blink;
    bool b_print_labels;
    bool b_acoustic_model;

  private:
    std::mutex mtx;
    // void draw_source_trace(Cairo::RefPtr<Cairo::Context> cr,TASCAR::pos_t
    // rpos,TASCAR::Acousticmodel::source_t*
    // src,TASCAR::Acousticmodel::acoustic_model_t* am);
  };

} // namespace TSCGUI

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
