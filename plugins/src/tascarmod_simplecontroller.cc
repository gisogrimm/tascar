/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "errorhandling.h"
#include "session.h"
#include "simplecontroller_glade.h"
#include <gdkmm/event.h>
#include <gtkmm.h>
#include <gtkmm/builder.h>
#include <gtkmm/window.h>
#include <iostream>

#define GET_WIDGET(x)                                                          \
  refBuilder->get_widget(#x, x);                                               \
  if(!x)                                                                       \
  throw TASCAR::ErrMsg(std::string("No widget \"") + #x +                      \
                       std::string("\" in builder."))

class simplecontroller_t : public TASCAR::actor_module_t {
public:
  simplecontroller_t(const TASCAR::module_cfg_t& cfg);
  virtual ~simplecontroller_t();
  void configure();
  void update(uint32_t frame, bool running);

private:
  void cb_button_left_pressed() { velocity.y = vy; };
  void cb_button_left_released() { velocity.y = 0; };
  void cb_button_right_pressed() { velocity.y = -vy; };
  void cb_button_right_released() { velocity.y = 0; };
  void cb_button_forward_pressed() { velocity.x = vx; };
  void cb_button_forward_released() { velocity.x = 0; };
  void cb_button_backward_pressed() { velocity.x = -vx; };
  void cb_button_backward_released() { velocity.x = 0; };
  void cb_button_rotate_left_pressed() { rotvel.z = vr; };
  void cb_button_rotate_left_released() { rotvel.z = 0; };
  void cb_button_rotate_right_pressed() { rotvel.z = -vr; };
  void cb_button_rotate_right_released() { rotvel.z = 0; };
  void cb_button_rotate_up_pressed() { rotvel.x = vr; };
  void cb_button_rotate_up_released() { rotvel.x = 0; };
  void cb_button_rotate_down_pressed() { rotvel.x = -vr; };
  void cb_button_rotate_down_released() { rotvel.x = 0; };
  void cb_key_pressed(GdkEventKey* key)
  {
    switch(key->keyval) {
    case 65360:
      cb_button_rotate_down_pressed();
      break;
    case 65367:
      cb_button_rotate_up_pressed();
      break;
    case 65365:
      cb_button_rotate_left_pressed();
      break;
    case 65366:
      cb_button_rotate_right_pressed();
      break;
    case 65362:
      cb_button_forward_pressed();
      break;
    case 65364:
      cb_button_backward_pressed();
      break;
    case 65361:
      cb_button_left_pressed();
      break;
    case 65363:
      cb_button_right_pressed();
      break;
    default:
      std::cerr << "released: " << key->keyval << std::endl;
    }
    // return true;
  };
  void cb_key_released(GdkEventKey* key)
  {
    switch(key->keyval) {
    case 65360:
      cb_button_rotate_down_released();
      break;
    case 65367:
      cb_button_rotate_up_released();
      break;
    case 65365:
      cb_button_rotate_left_released();
      break;
    case 65366:
      cb_button_rotate_right_released();
      break;
    case 65362:
      cb_button_forward_released();
      break;
    case 65364:
      cb_button_backward_released();
      break;
    case 65361:
      cb_button_left_released();
      break;
    case 65363:
      cb_button_right_released();
      break;
    default:
      std::cerr << "released: " << key->keyval << std::endl;
    }
    // return true;
  };
  TASCAR::pos_t velocity;
  TASCAR::zyx_euler_t rotvel;
  double vscale;
  double maxnorm;
  double vx;
  double vy;
  double vz;
  double vr;
  Glib::RefPtr<Gtk::Builder> refBuilder;
  Gtk::Window* win;
  Gtk::Button* button_left;
  Gtk::Button* button_right;
  Gtk::Button* button_forward;
  Gtk::Button* button_backward;
  Gtk::Button* button_rotate_left;
  Gtk::Button* button_rotate_right;
  Gtk::Button* button_rotate_up;
  Gtk::Button* button_rotate_down;
};

simplecontroller_t::simplecontroller_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg, true), vscale(1), maxnorm(0), vx(1), vy(1), vz(1),
      vr(TASCAR_PI2)
{
  GET_ATTRIBUTE(
      maxnorm, "m",
      "Maximum distance of object from origin, or zero for no limit.");
  GET_ATTRIBUTE(vx, "m/s", "Velocity in x direction");
  GET_ATTRIBUTE(vy, "m/s", "Velocity in y direction");
  GET_ATTRIBUTE(vz, "m/s", "Velocity in y direction");
  GET_ATTRIBUTE_DEGU(vr, "Angular velocity", "deg/s");
  refBuilder = Gtk::Builder::create_from_string(ui_simplecontroller);
  GET_WIDGET(win);
  GET_WIDGET(button_left);
  GET_WIDGET(button_right);
  GET_WIDGET(button_forward);
  GET_WIDGET(button_backward);
  GET_WIDGET(button_rotate_left);
  GET_WIDGET(button_rotate_right);
  GET_WIDGET(button_rotate_up);
  GET_WIDGET(button_rotate_down);
  win->add_events(Gdk::KEY_PRESS_MASK | Gdk::BUTTON_PRESS_MASK);
  button_left->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_left_pressed));
  button_left->signal_released().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_left_released));
  button_right->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_right_pressed));
  button_right->signal_released().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_right_released));
  button_forward->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_forward_pressed));
  button_forward->signal_released().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_forward_released));
  button_backward->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_backward_pressed));
  button_backward->signal_released().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_backward_released));
  button_rotate_left->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_rotate_left_pressed));
  button_rotate_left->signal_released().connect(sigc::mem_fun(
      *this, &simplecontroller_t::cb_button_rotate_left_released));
  button_rotate_right->signal_pressed().connect(sigc::mem_fun(
      *this, &simplecontroller_t::cb_button_rotate_right_pressed));
  button_rotate_right->signal_released().connect(sigc::mem_fun(
      *this, &simplecontroller_t::cb_button_rotate_right_released));
  button_rotate_up->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_rotate_up_pressed));
  button_rotate_up->signal_released().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_rotate_up_released));
  button_rotate_down->signal_pressed().connect(
      sigc::mem_fun(*this, &simplecontroller_t::cb_button_rotate_down_pressed));
  button_rotate_down->signal_released().connect(sigc::mem_fun(
      *this, &simplecontroller_t::cb_button_rotate_down_released));
  win->signal_key_press_event().connect_notify(
      sigc::mem_fun(*this, &simplecontroller_t::cb_key_pressed));
  win->signal_key_release_event().connect_notify(
      sigc::mem_fun(*this, &simplecontroller_t::cb_key_released));
  win->show();
}

void simplecontroller_t::configure()
{
  actor_module_t::configure();
  vscale = n_fragment / f_sample;
}

void simplecontroller_t::update(uint32_t, bool)
{
  TASCAR::zyx_euler_t r(rotvel);
  r *= vscale;
  add_orientation(r);
  for(auto& obj_ : obj) {
    TASCAR::pos_t v(velocity);
    v *= vscale;
    auto eulz = obj_.obj->dorientation.to_euler();
    eulz.x = 0.0;
    eulz.y = 0.0;
    v *= eulz;
    obj_.obj->dlocation += v;
    if(maxnorm > 0) {
      double dnorm(obj_.obj->dlocation.norm());
      if(dnorm > maxnorm)
        obj_.obj->dlocation *= maxnorm / dnorm;
    }
  }
}

simplecontroller_t::~simplecontroller_t()
{
  // deactivate();
  delete win;
  delete button_left;
}

REGISTER_MODULE(simplecontroller_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * compile-command: "make -C .."
 * End:
 */
