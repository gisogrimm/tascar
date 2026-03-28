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

#include "tascar_mainwindow.h"
#include "logo.xpm"
#include "pdfexport.h"
#include "tascar.res.c"
#include "tascar_os.h"
#include <fstream>

#undef WEBKIT2GTK30
#undef WEBKIT2GTK40

#define GET_WIDGET(x)                                                          \
  m_refBuilder->get_widget(#x, x);                                             \
  if(!x)                                                                       \
  throw TASCAR::ErrMsg(std::string("No widget \"") + #x +                      \
                       std::string("\" in builder."))

void error_message(const std::string& msg)
{
  std::cerr << "Error: " << msg << std::endl;
  Gtk::MessageDialog dialog("Error", false, Gtk::MESSAGE_ERROR);
  dialog.set_secondary_text(msg);
  dialog.run();
}

tascar_window_t::tascar_window_t(BaseObjectType* cobject,
                                 const Glib::RefPtr<Gtk::Builder>& refGlade)
    : Gtk::Window(cobject), m_refBuilder(refGlade), scene_map(NULL),
      statusbar_main(NULL), timeline(NULL), scene_selector(NULL),
      selected_scene(0), scene_active(NULL), active_selector(NULL),
      active_object(NULL), active_label_sourceline(NULL), active_mixer(NULL),
      active_source_ctl(NULL), blink(false), sessionloaded(false),
      sessionquit(false), splash_timeout(0)
{
  Gsv::init();
  language_manager = Gsv::LanguageManager::create();
  pthread_mutex_init(&mtx_draw, NULL);
  m_refBuilder->get_widget("SceneDraw", scene_map);
  if(!scene_map)
    throw TASCAR::ErrMsg("No drawing area widget");
  con_draw = scene_map->signal_draw().connect(
      sigc::mem_fun(*this, &tascar_window_t::draw_scene), false);
  con_timeout = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_timeout), 100);
  con_timeout_blink = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_timeout_blink), 600);
  con_timeout_statusbar = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_timeout_statusbar), 100);
  scene_map->add_events(Gdk::SCROLL_MASK);
  scene_map->add_events(Gdk::BUTTON_PRESS_MASK);
  scene_map->signal_scroll_event().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_map_scroll));
  scene_map->signal_button_press_event().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_map_clicked));
  m_refBuilder->get_widget("StatusbarMain", statusbar_main);
  if(!statusbar_main)
    throw TASCAR::ErrMsg("No main status bar");
  m_refBuilder->get_widget("Timeline", timeline);
  if(!timeline)
    throw TASCAR::ErrMsg("No time line");
  timeline->signal_value_changed().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_time_changed));
  m_refBuilder->get_widget_derived("RouteButtons", source_panel);
  if(!source_panel)
    throw TASCAR::ErrMsg("No source panel");
  Gtk::AboutDialog* aboutdialog(NULL);
  m_refBuilder->get_widget("aboutdialog", aboutdialog);
  if(aboutdialog)
    aboutdialog->set_logo(Gdk::Pixbuf::create_from_xpm_data(logo));
  GET_WIDGET(scene_selector);
  scene_selector->signal_changed().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_scene_selector_changed));
  GET_WIDGET(notebook);
  GET_WIDGET(scene_active);
  GET_WIDGET(active_selector);
  GET_WIDGET(active_type_label);
  GET_WIDGET(active_track);
  GET_WIDGET(active_label_sourceline);
  GET_WIDGET(active_source_display);
  GET_WIDGET(active_mixer);
  GET_WIDGET(session_splash);
  GET_WIDGET(lab_authors);
  GET_WIDGET(lab_sessionname);
  GET_WIDGET(but_warnings);
  GET_WIDGET(but_scripts);
  active_selector->signal_changed().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_active_selector_changed));
  active_track->signal_toggled().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_active_track_changed));
  scene_active->signal_toggled().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_scene_active_changed));
  but_warnings->signal_clicked().connect(
      sigc::mem_fun(*this, &tascar_window_t::on_show_warnings_clicked));
  Gtk::Image* image_splashlogo(NULL);
  m_refBuilder->get_widget("image_splashlogo", image_splashlogo);
  if(image_splashlogo)
    image_splashlogo->set(Gdk::Pixbuf::create_from_xpm_data(logo));
  GET_WIDGET(text_warnings);
  GET_WIDGET(scrolled_window_source);
  scrolled_window_source->add(source_view);
  source_view.set_show_line_numbers();
  source_view.set_editable(false);
  source_view.show();
  source_buffer = source_view.get_source_buffer();
  source_buffer->set_language(language_manager->get_language("xml"));
  source_buffer->set_highlight_syntax();
  GET_WIDGET(legal_view);
  GET_WIDGET(osc_vars);
  GET_WIDGET(text_srv_url);
  // Create actions for menus and toolbars:
  Glib::RefPtr<Gio::SimpleActionGroup> refActionGroupMain =
      Gio::SimpleActionGroup::create();
  refActionGroupMain->add_action(
      "scene_new", sigc::mem_fun(*this, &tascar_window_t::on_menu_file_new));
  refActionGroupMain->add_action(
      "scene_open", sigc::mem_fun(*this, &tascar_window_t::on_menu_file_open));
  refActionGroupMain->add_action(
      "scene_open_example",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_open_example));
  refActionGroupMain->add_action(
      "scene_reload",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_reload));
  refActionGroupMain->add_action(
      "file_runscript",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_runscript));
  refActionGroupMain->add_action(
      "export_csv",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_exportcsv));
  refActionGroupMain->add_action(
      "export_csvsounds",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_exportcsvsounds));
  refActionGroupMain->add_action(
      "export_svg",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_exportsvg));
  refActionGroupMain->add_action(
      "export_pdf",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_exportpdf));
  refActionGroupMain->add_action(
      "export_acmodel",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_exportacmodel));
  refActionGroupMain->add_action(
      "scene_close",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_file_close));
  refActionGroupMain->add_action(
      "quit", sigc::mem_fun(*this, &tascar_window_t::on_menu_file_quit));
  refActionGroupMain->add_action(
      "help_bugreport",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_help_bugreport));
  refActionGroupMain->add_action(
      "help_manual",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_help_manual));
  refActionGroupMain->add_action(
      "help_about", sigc::mem_fun(*this, &tascar_window_t::on_menu_help_about));
  insert_action_group("main", refActionGroupMain);
  Glib::RefPtr<Gio::SimpleActionGroup> refActionGroupView =
      Gio::SimpleActionGroup::create();
  refActionGroupView->add_action(
      "zoom_in", sigc::mem_fun(*this, &tascar_window_t::on_menu_view_zoom_in));
  refActionGroupView->add_action(
      "zoom_out",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_zoom_out));
  refActionGroupView->add_action(
      "viewport_xy",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_xy));
  refActionGroupView->add_action(
      "viewport_xz",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_xz));
  refActionGroupView->add_action(
      "viewport_yz",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_yz));
  refActionGroupView->add_action(
      "viewport_xyz",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_xyz));
  refActionGroupView->add_action(
      "viewport_rotz",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_rotz));
  refActionGroupView->add_action(
      "viewport_rotzcw",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_rotzcw));
  refActionGroupView->add_action(
      "viewport_setref",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_viewport_setref));
  refActionGroupView->add_action(
      "meter_rmspeak",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meter_rmspeak));
  refActionGroupView->add_action(
      "meter_rms",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meter_rms));
  refActionGroupView->add_action(
      "meter_peak",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meter_peak));
  refActionGroupView->add_action(
      "meter_percentile",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meter_percentile));
  refActionGroupView->add_action(
      "meterw_z",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meterw_z));
  refActionGroupView->add_action(
      "meterw_c",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meterw_c));
  refActionGroupView->add_action(
      "meterw_a",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meterw_a));
  refActionGroupView->add_action(
      "meterw_bandpass",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_view_meterw_bandpass));
  insert_action_group("view", refActionGroupView);
  Glib::RefPtr<Gio::SimpleActionGroup> refActionGroupTransport =
      Gio::SimpleActionGroup::create();
  refActionGroupTransport->add_action(
      "play", sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_play));
  refActionGroupTransport->add_action(
      "stop", sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_stop));
  refActionGroupTransport->add_action(
      "rewind",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_rewind));
  refActionGroupTransport->add_action(
      "forward",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_forward));
  refActionGroupTransport->add_action(
      "previous",
      sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_previous));
  refActionGroupTransport->add_action(
      "next", sigc::mem_fun(*this, &tascar_window_t::on_menu_transport_next));
  insert_action_group("transport", refActionGroupTransport);
#if defined(WEBKIT2GTK30) || defined(WEBKIT2GTK40)
  news_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
  news_viewpp = Glib::wrap(GTK_WIDGET(news_view));
  GET_WIDGET(news_box);
  news_box->pack_end(*news_viewpp);
  std::string url(std::string("http://news.tascar.org/?version=" +
                              std::string(TASCARVERSION)));
  bool load_news(TASCAR::config("tascar.gui.newspage", true));
  if(load_news)
    webkit_web_view_load_uri(news_view, url.c_str());
#else
  notebook->remove_page(-1);
#endif
  notebook->show_all();
  auto css = Gtk::CssProvider::create();
  std::string s_css =
      ".toolbar {font-size: 70%;} button:checked "
      "{background-image:image(yellow);} .smallbutton "
      "{padding:0px;margin:0px;font-size:70%;min-height: 14px;}";
  s_css = TASCAR::config("tascar.gui.css", s_css);
  try {
    if(css->load_from_data(s_css)) {
      auto screen = Gdk::Screen::get_default();
      auto ctx = get_style_context();
      ctx->add_provider_for_screen(screen, css,
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  }
  catch(const Gtk::CssProviderError& e) {
    TASCAR::add_warning("css error: " + e.what());
  }
  // optionally test for running jack server and start qjackctl:
  bool checkforjack = TASCAR::config("tascar.gui.checkforjack", 0);
  if(checkforjack) {
    bool jack_is_running = test_for_jack_server();
    if(!jack_is_running) {
      jackpid = TASCAR::system("qjackctl", false);
    }
  }
}

bool tascar_window_t::on_timeout()
{
  if(session_mutex.try_lock()) {
    if(splash_timeout) {
      splash_timeout--;
      if(!splash_timeout)
        session_splash->hide();
    }
    if(session) {
      if(pthread_mutex_trylock(&mtx_draw) == 0) {
        if(session->scenes.size() > selected_scene)
          if(session->scenes[selected_scene]->scene_t::active !=
             scene_active->get_active())
            scene_active->set_active(
                session->scenes[selected_scene]->scene_t::active);
        Glib::RefPtr<Gdk::Window> win = scene_map->get_window();
        if(win) {
          Gdk::Rectangle r(0, 0, scene_map->get_allocation().get_width(),
                           scene_map->get_allocation().get_height());
          win->invalidate_rect(r, true);
        }
        if(source_panel)
          source_panel->invalidate_win();
        if(active_source_ctl)
          active_source_ctl->invalidate_win();
        if(session)
          draw.set_time(session->tp_get_time());
        timeline->set_value(draw.get_time());
        pthread_mutex_unlock(&mtx_draw);
      }
    }
    if(sessionquit)
      hide();
    // script button:
    Gdk::RGBA col;
    if( session && (session->scriptrunning != 0) ){
      but_scripts->set_label(TASCAR::to_string(session->scriptrunning)+" scripts");
      col.set_rgba(1, 0.4, 0.4, 1);
    }else{
      but_scripts->set_label("0 scripts");
      col.set_rgba(0.92, 0.92, 0.92, 1);
    }
    but_scripts->override_background_color(col);
    session_mutex.unlock();
  }
  return true;
}

bool tascar_window_t::on_timeout_statusbar()
{
  if(session_mutex.try_lock()) {
    if(pthread_mutex_trylock(&mtx_draw) == 0) {
      char cmp[1024];
      cmp[1023] = 0;
      if(session && session->is_running()) {
        if((session->scenes.size() > selected_scene) &&
           (session->scenes[selected_scene]->scene_t::active)) {
          TASCAR::scene_render_rt_t* scene(session->scenes[selected_scene]);
          TASCAR::render_profiler_t prof(scene->loadaverage);
          prof.normalize(prof.t_postproc);
          snprintf(cmp, 1023,
                   "scenes: %zu  point sources: %d/%d  diffuse sound fields: "
                   "%d/%d | jack: %1.1f%% (scene \"%s\" load: %1.1f%% init: "
                   "%1.1f%%  geo: %1.1f%%  preproc: %1.1f%%  acoustic: "
                   "%1.1f%%  postproc: %1.1f%%)",
                   session->scenes.size(), session->get_active_pointsources(),
                   session->get_total_pointsources(),
                   session->get_active_diffuse_sound_fields(),
                   session->get_total_diffuse_sound_fields(),
                   scene->get_cpu_load(), scene->name.c_str(),
                   100.0 * scene->loadaverage.t_postproc, 100.0 * prof.t_init,
                   100.0 * (prof.t_geo - prof.t_init),
                   100.0 * (prof.t_preproc - prof.t_geo),
                   100.0 * (prof.t_acoustics - prof.t_preproc),
                   100.0 * (prof.t_postproc - prof.t_acoustics));
        } else {
          snprintf(cmp, 1023,
                   "scenes: %ld  point sources: %d/%d  diffuse sound fields: "
                   "%d/%d",
                   (long int)(session->scenes.size()),
                   session->get_active_pointsources(),
                   session->get_total_pointsources(),
                   session->get_active_diffuse_sound_fields(),
                   session->get_total_diffuse_sound_fields());
        }
        sessionloaded = true;
      } else {
        snprintf(cmp, 1023, "No session loaded.");
        if(sessionloaded)
          reset_gui();
        sessionloaded = false;
      }
      statusbar_main->remove_all_messages();
      statusbar_main->push(cmp);
      if(session && session->is_running()) {
        if(source_panel)
          source_panel->update();
        if(active_source_ctl)
          active_source_ctl->update();
      }
      pthread_mutex_unlock(&mtx_draw);
    }
    session_mutex.unlock();
  }
  return true;
}

bool tascar_window_t::on_timeout_blink()
{
  if(pthread_mutex_trylock(&mtx_draw) == 0) {
    if(blink)
      blink = false;
    else
      blink = true;
    draw.set_blink(blink);
    if(TASCAR::get_warnings().size() > numlastwarnings) {
      numlastwarnings = TASCAR::get_warnings().size();
      on_menu_view_show_warnings();
    }
    bool has_warnings(text_warnings->get_buffer()->size());
    Gdk::RGBA col;
    if(has_warnings)
      but_warnings->set_label("!");
    else
      but_warnings->set_label("");
    if(has_warnings && blink)
      col.set_rgba(1, 0.5, 0, 1);
    else
      col.set_rgba(0.92, 0.92, 0.92, 1);
    but_warnings->override_background_color(col);
    pthread_mutex_unlock(&mtx_draw);
  }
  return true;
}

void tascar_window_t::on_show_warnings_clicked()
{
  notebook->set_current_page(5);
}

tascar_window_t::~tascar_window_t()
{
#ifdef WEBKIT2GTK40
  webkit_web_view_try_close(news_view);
#endif
  con_draw.disconnect();
  con_timeout.disconnect();
  con_timeout_blink.disconnect();
  con_timeout_statusbar.disconnect();
  active_mixer->remove();
  if(active_source_ctl)
    delete active_source_ctl;
  pthread_mutex_trylock(&mtx_draw);
  pthread_mutex_unlock(&mtx_draw);
  pthread_mutex_destroy(&mtx_draw);
  if(jackpid > 0) {
    terminate_process(jackpid);
  }
}

bool tascar_window_t::draw_scene(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if(session) {
    if(session->lock_vars()) {
      if(session->is_running()) {
        if(pthread_mutex_trylock(&mtx_draw) == 0) {
          Gtk::Allocation allocation = scene_map->get_allocation();
          const int width = allocation.get_width();
          const int height = allocation.get_height();
          cr->rectangle(0, 0, width, height);
          cr->clip();
          cr->translate(0.5 * width, 0.5 * height);
          double wscale(0.5 * std::max(height, width));
          double markersize(5.0 / wscale);
          cr->scale(wscale, wscale);
          cr->set_line_width(0.3 * markersize);
          cr->set_font_size(3 * markersize);
          cr->save();
          cr->set_source_rgb(1, 1, 1);
          cr->paint();
          cr->restore();
          draw.set_markersize(markersize);
          draw.draw(cr);
          // calculate left bottom corner in TASCAR coordinate system:
          TASCAR::pos_t p_o(-0.4 * width, -0.4 * height, 0);
          p_o /= wscale;
          p_o = draw.view.inverse(p_o);
          float scale(std::min(
              pow(10.0, ceil(log10(0.1 * draw.view.scale))),
              std::min(2.0 * pow(10.0, ceil(log10(0.05 * draw.view.scale))),
                       5.0 * pow(10.0, ceil(log10(0.02 * draw.view.scale))))));
          TASCAR::pos_t p_x(scale, 0, 0);
          TASCAR::pos_t p_y(0, scale, 0);
          TASCAR::pos_t p_z(0, 0, scale);
          p_x += p_o;
          p_y += p_o;
          p_z += p_o;
          p_o = draw.view(p_o);
          p_x = draw.view(p_x);
          p_y = draw.view(p_y);
          p_z = draw.view(p_z);
          cr->move_to(p_o.x, -p_o.y);
          char ctmp[1024];
          if(scale >= 1000)
            snprintf(ctmp, 1023, "%2.5g km (%2.5g)", 0.001 * scale,
                     2 * draw.view.scale);
          else
            snprintf(ctmp, 1023, "%2.5g m (%2.5g)", scale, 2 * draw.view.scale);
          cr->set_source_rgb(0, 0, 0);
          cr->show_text(ctmp);
          cr->set_source_rgb(1, 0, 0);
          draw.draw_edge(cr, p_o, p_x);
          cr->stroke();
          cr->set_source_rgb(0, 1, 0);
          draw.draw_edge(cr, p_o, p_y);
          cr->stroke();
          cr->set_source_rgb(0, 0, 1);
          draw.draw_edge(cr, p_o, p_z);
          cr->stroke();
          p_x = TASCAR::pos_t(10.0 * draw.view.scale, 0, 0);
          p_y = TASCAR::pos_t(0, 10.0 * draw.view.scale, 0);
          p_z = TASCAR::pos_t(0, 0, 10.0 * draw.view.scale);
          TASCAR::pos_t p_x1(p_x);
          TASCAR::pos_t p_y1(p_y);
          TASCAR::pos_t p_z1(p_z);
          p_x1 *= -1.0;
          p_y1 *= -1.0;
          p_z1 *= -1.0;
          p_x = draw.view(p_x);
          p_y = draw.view(p_y);
          p_z = draw.view(p_z);
          p_x1 = draw.view(p_x1);
          p_y1 = draw.view(p_y1);
          p_z1 = draw.view(p_z1);
          cr->set_source_rgba(1, 0, 0, 0.17);
          draw.draw_edge(cr, p_x1, p_x);
          cr->stroke();
          cr->set_source_rgba(0, 1, 0, 0.17);
          draw.draw_edge(cr, p_y1, p_y);
          cr->stroke();
          cr->set_source_rgba(0, 0, 1, 0.17);
          draw.draw_edge(cr, p_z1, p_z);
          cr->stroke();
          pthread_mutex_unlock(&mtx_draw);
        }
      }
      session->unlock_vars();
    }
  }
  return true;
}

void tascar_window_t::load(const std::string& fname)
{
  get_warnings().clear();
  numlastwarnings = 0;
  statusbar_main->remove_all_messages();
  statusbar_main->push("loading session...");
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
  scene_load(fname);
  tascar_filename = fname;
  sessionquit = false;
  if(session) {
    session->add_bool_true("/tascargui/quit", &sessionquit);
    session->add_bool_true("/tascar/quit", &sessionquit);
  }
  reset_gui();
}

void tascar_window_t::on_active_track_changed()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene)) {
      if(active_track->get_active()) {
        if(active_object)
          session->scenes[selected_scene]->guitrackobject = active_object;
      } else {
        session->scenes[selected_scene]->guitrackobject = NULL;
      }
    }
    session_mutex.unlock();
  }
}

void tascar_window_t::on_scene_active_changed()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene))
      session->scenes[selected_scene]->scene_t::active =
          scene_active->get_active();
    session_mutex.unlock();
  }
}

void tascar_window_t::on_active_selector_changed()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene)) {
      // do something.
      std::string sc(active_selector->get_active_text());
      if(sc == "(none)")
        active_object = NULL;
      else {
        std::vector<TASCAR::Scene::object_t*> match(
            session->scenes[selected_scene]->find_object(sc));
        if(match.size())
          active_object = match[0];
        else
          active_object = NULL;
      }
      update_selection_info();
    }
    draw.select_object(active_object);
    session_mutex.unlock();
  }
}

void tascar_window_t::on_scene_selector_changed()
{
  if(session_mutex.try_lock()) {
    if(session) {
      std::string sc(scene_selector->get_active_text());
      for(unsigned int k = 0; k < session->scenes.size(); k++)
        if(session->scenes[k]->name == sc)
          selected_scene = k;
    } else {
      selected_scene = 0;
    }
    if(session && (session->scenes.size() > selected_scene)) {
      draw.set_scene(session->scenes[selected_scene]);
      source_panel->set_scene(session->scenes[selected_scene], session);
      draw.view.set_scale(session->scenes[selected_scene]->guiscale);
      // fill object list:
      update_levelmeter_settings();
      update_object_list();
    } else {
      draw.set_scene(NULL);
      source_panel->set_scene(NULL, NULL);
      draw.view.set_scale(20);
    }
    session_mutex.unlock();
  }
}

void tascar_window_t::update_selection_info()
{
  active_mixer->remove();
  if(active_source_ctl)
    delete active_source_ctl;
  active_source_ctl = NULL;
  if(session && active_object) {
    active_type_label->set_text(active_object->get_type());
    active_track->set_active(session->scenes[selected_scene]->guitrackobject ==
                             active_object);
    tsccfg::node_t e(active_object->TASCAR::dynobject_t::e);
    active_label_sourceline->set_text(tsccfg::node_get_path(e));
    TASCAR::xml_doc_t doc(e);
    std::string disp(doc.save_to_string());
    // remove first line:
    size_t p(disp.find(10));
    if(p < disp.npos)
      ++p;
    disp.erase(disp.begin(), disp.begin() + p);
    active_source_display->get_buffer()->set_text(disp);
    active_source_ctl = new TSCGUI::source_ctl_t(
        session->scenes[selected_scene], active_object);
    active_mixer->add(*active_source_ctl);
    active_mixer->show_all();
    active_source_ctl->set_levelmeter_mode(source_panel->lmode);
    active_source_ctl->set_levelmeter_range(session->levelmeter_min,
                                            session->levelmeter_range);
    active_source_ctl->set_levelmeter_weight(session->levelmeter_weight);
  } else {
    active_type_label->set_text("");
    active_track->set_active(false);
    active_label_sourceline->set_text("");
    active_source_display->get_buffer()->set_text("");
  }
}

void tascar_window_t::update_object_list()
{
  active_selector->remove_all();
  active_selector->append("(none)");
  active_selector->set_active(0);
  active_object = NULL;
  draw.select_object(NULL);
  if(session && (session->scenes.size() > selected_scene)) {
    for(std::vector<TASCAR::Scene::object_t*>::const_iterator it =
            session->scenes[selected_scene]->all_objects.begin();
        it != session->scenes[selected_scene]->all_objects.end(); ++it)
      active_selector->append((*it)->get_name());
  }
  update_selection_info();
}

void tascar_window_t::update_levelmeter_settings()
{
  if(session && source_panel) {
    source_panel->set_levelmeter_mode(session->levelmeter_mode);
    source_panel->set_levelmeter_range(session->levelmeter_min,
                                       session->levelmeter_range);
    source_panel->set_levelmeter_weight(session->levelmeter_weight);
    if(active_source_ctl) {
      active_source_ctl->set_levelmeter_mode(source_panel->lmode);
      active_source_ctl->set_levelmeter_range(session->levelmeter_min,
                                              session->levelmeter_range);
      active_source_ctl->set_levelmeter_weight(session->levelmeter_weight);
    }
  }
}

void tascar_window_t::reset_gui()
{
  timeline->clear_marks();
  scene_selector->remove_all();
  scene_selector->set_active(0);
  selected_range = -1;
  if(session) {
    if(session->has_authors()) {
      lab_authors->set_text(session->get_authors());
      lab_sessionname->set_text(session->name);
      splash_timeout =
          10.0 * TASCAR::config("tascar.gui.sessionsplashtimeout", 5.0);
      session_splash->show();
    }
    int32_t mainwin_width(1600);
    int32_t mainwin_height(900);
    int32_t mainwin_x(0);
    int32_t mainwin_y(0);
    get_position(mainwin_x, mainwin_y);
    TASCAR::xml_element_t mainwin(
        session->root.find_or_add_child("mainwindow"));
    mainwin.get_attribute("w", mainwin_width, "px", "main window width");
    mainwin.get_attribute("h", mainwin_height, "px", "main window height");
    mainwin.get_attribute("x", mainwin_x, "px", "main window x position");
    mainwin.get_attribute("y", mainwin_y, "px", "main window y position");
    bool minimized = false;
    mainwin.GET_ATTRIBUTE_BOOL(minimized, "Start with minimized main window");
    resize(mainwin_width, mainwin_height);
    move(mainwin_x, mainwin_y);
    resize(mainwin_width, mainwin_height);
    move(mainwin_x, mainwin_y);
    if(minimized)
      Gtk::Window::iconify();
    timeline->set_range(0, session->duration);
    for(unsigned int k = 0; k < session->ranges.size(); k++) {
      timeline->add_mark(session->ranges[k]->start, Gtk::POS_BOTTOM, "");
      timeline->add_mark(session->ranges[k]->end, Gtk::POS_BOTTOM, "");
    }
    for(std::vector<TASCAR::scene_render_rt_t*>::iterator it =
            session->scenes.begin();
        it != session->scenes.end(); ++it)
      scene_selector->append((*it)->name);
    selected_scene = 0;
    scene_selector->set_active(0);
    if(session->scenes.size() > selected_scene)
      set_scale(session->scenes[selected_scene]->guiscale);
    if(session->scenes.size() > selected_scene)
      if(session->scenes[selected_scene]->scene_t::active !=
         scene_active->get_active())
        scene_active->set_active(
            session->scenes[selected_scene]->scene_t::active);
  }
  if(session) {
    set_title("tascar - " + session->name + " [" +
              Glib::filename_display_basename(tascar_filename) + "]");
  } else {
    set_title("tascar");
    // resize(200, 60);
  }
  update_object_list();
  if(session && (session->scenes.size() > selected_scene)) {
    draw.set_scene(session->scenes[selected_scene]);
    source_panel->set_scene(session->scenes[selected_scene], session);
    draw.view.set_scale(session->scenes[selected_scene]->guiscale);
    update_levelmeter_settings();
  }
  if(session) {
    source_buffer->set_text(session->save_to_string());
    osc_vars->get_buffer()->set_text(session->list_variables());
    text_srv_url->set_text(session->get_srv_url());
    legal_view->get_buffer()->set_text(session->legal_stuff());
  } else {
    source_view.get_source_buffer()->set_text("");
    osc_vars->get_buffer()->set_text("");
    text_srv_url->set_text("");
    legal_view->get_buffer()->set_text("");
  }
  on_menu_view_show_warnings();
#if defined(WEBKIT2GTK30) || defined(WEBKIT2GTK40)
  if(session && (!session->starturl.empty())) {
    webkit_web_view_load_uri(news_view,
                             TASCAR::env_expand(session->starturl).c_str());
    notebook->set_current_page(6);
  }
  if(session)
    webkit_web_view_reload_bypass_cache(news_view);
#endif
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
}

void tascar_window_t::on_menu_file_quit()
{
  hide(); // Closes the main window to stop the app->run().
}

void tascar_window_t::on_menu_file_close()
{
  try {
    scene_destroy();
    resize(200, 60);
    // get_warnings().clear();
#if defined(WEBKIT2GTK30) || defined(WEBKIT2GTK40)
    webkit_web_view_try_close(news_view);
#endif
  }
  catch(const std::exception& e) {
    error_message(e.what());
  }
  reset_gui();
}

void tascar_window_t::on_menu_file_new()
{
  try {
    scene_new();
  }
  catch(const std::exception& e) {
    error_message(e.what());
  }
  reset_gui();
}

void tascar_window_t::on_menu_file_exportcsv()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    // Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    // Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
    filter_tascar->set_name("CSV files");
    filter_tascar->add_pattern("*.csv");
    dialog.add_filter(filter_tascar);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    // Show the dialog and wait for a user response:
    int result = dialog.run();
    // Handle the response:
    if(result == Gtk::RESPONSE_OK) {
      // Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if(filename.find(".csv") == std::string::npos)
        filename += ".csv";
      if(filename.size() > 0) {
        try {
          std::ofstream ofs(filename.c_str());
          ofs << "\"Name\",\"PosX\",\"PosY\",\"PosZ\",\"EulerZ\",\"EulerY\","
                 "\"EulerX\"\n";
          for(uint32_t kscene = 0; kscene < session->scenes.size(); kscene++) {
            ofs << "\"" << session->scenes[kscene]->name << "\", , , , , , \n";
            std::vector<TASCAR::Scene::object_t*> obj(
                session->scenes[kscene]->get_objects());
            for(uint32_t k = 0; k < obj.size(); k++) {
              TASCAR::pos_t p;
              TASCAR::zyx_euler_t o;
              obj[k]->get_6dof(p, o);
              ofs << "\"" << obj[k]->get_name() << "\"," << p.x << "," << p.y
                  << "," << p.z << "," << o.z * RAD2DEG << "," << o.y * RAD2DEG
                  << "," << o.x * RAD2DEG << "\n";
            }
          }
        }
        catch(const std::exception& e) {
          error_message(e.what());
        }
      }
    }
  }
}

void tascar_window_t::on_menu_file_exportcsvsounds()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    // Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    // Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
    filter_tascar->set_name("CSV files");
    filter_tascar->add_pattern("*.csv");
    dialog.add_filter(filter_tascar);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    // Show the dialog and wait for a user response:
    int result = dialog.run();
    // Handle the response:
    if(result == Gtk::RESPONSE_OK) {
      // Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if(filename.find(".csv") == std::string::npos)
        filename += ".csv";
      if(filename.size() > 0) {
        try {
          std::ofstream ofs(filename.c_str());
          ofs << "\"Name\",\"PosX\",\"PosY\",\"PosZ\"\n";
          for(uint32_t kscene = 0; kscene < session->scenes.size(); kscene++) {
            ofs << "\"" << session->scenes[kscene]->name << "\", , , \n";
            for(auto am : session->scenes[kscene]->sounds) {
              TASCAR::pos_t p(am->position);
              ofs << "\"" << am->get_fullname() << "\"," << p.x << "," << p.y
                  << "," << p.z << "\n";
            }
          }
        }
        catch(const std::exception& e) {
          error_message(e.what());
        }
      }
    }
  }
}

void tascar_window_t::on_menu_file_exportsvg()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    // Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    // Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
    filter_tascar->set_name("SVG files");
    filter_tascar->add_pattern("*.svg");
    dialog.add_filter(filter_tascar);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    // Show the dialog and wait for a user response:
    int result = dialog.run();
    // Handle the response:
    if(result == Gtk::RESPONSE_OK) {
      // Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if(filename.find(".svg") == std::string::npos)
        filename += ".svg";
      if(filename.size() > 0) {
        try {
          std::ofstream ofstr(filename);
          ofstr << TASCAR::export_svg(session, 400, 400);
        }
        catch(const std::exception& e) {
          error_message(e.what());
        }
      }
    }
  }
}

void tascar_window_t::on_menu_file_exportpdf()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    // Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    // Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
    filter_tascar->set_name("PDF files");
    filter_tascar->add_pattern("*.pdf");
    dialog.add_filter(filter_tascar);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    // Show the dialog and wait for a user response:
    int result = dialog.run();
    // Handle the response:
    if(result == Gtk::RESPONSE_OK) {
      // Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if(filename.find(".pdf") == std::string::npos)
        filename += ".pdf";
      if(filename.size() > 0) {
        try {
          TASCAR::pdfexport_t pdfexport(session, filename, false);
        }
        catch(const std::exception& e) {
          error_message(e.what());
        }
      }
    }
  }
}

void tascar_window_t::on_menu_file_exportacmodel()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    // Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    // Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
    filter_tascar->set_name("PDF files");
    filter_tascar->add_pattern("*.pdf");
    dialog.add_filter(filter_tascar);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    // Show the dialog and wait for a user response:
    int result = dialog.run();
    // Handle the response:
    if(result == Gtk::RESPONSE_OK) {
      // Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if(filename.find(".pdf") == std::string::npos)
        filename += ".pdf";
      if(filename.size() > 0) {
        try {
          TASCAR::pdfexport_t pdfexport(session, filename, true);
        }
        catch(const std::exception& e) {
          error_message(e.what());
        }
      }
    }
  }
}

void tascar_window_t::on_menu_file_reload()
{
  try {
    get_warnings().clear();
    numlastwarnings = 0;
    scene_destroy();
    reset_gui();
    // wait?
    statusbar_main->remove_all_messages();
    statusbar_main->push("loading session...");
    while(Gtk::Main::events_pending())
      Gtk::Main::iteration(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    scene_load(tascar_filename);
    sessionquit = false;
    if(session) {
      session->add_bool_true("/tascargui/quit", &sessionquit);
      session->add_bool_true("/tascar/quit", &sessionquit);
    }
    reset_gui();
  }
  catch(const std::exception& e) {
    error_message(e.what());
  }
}

void tascar_window_t::on_menu_file_runscript()
{
  Gtk::FileChooserDialog dialog("Please choose a file",
                                Gtk::FILE_CHOOSER_ACTION_OPEN);
  dialog.set_transient_for(*this);
  // Add response buttons the the dialog:
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
  // Add filters, so that only certain file types can be selected:
  Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
  filter_tascar->set_name("tascar OSC script files");
  filter_tascar->add_pattern("*.tosc");
  dialog.add_filter(filter_tascar);
  Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
  filter_any->set_name("Any files");
  filter_any->add_pattern("*");
  dialog.add_filter(filter_any);
  // Show the dialog and wait for a user response:
  int result = dialog.run();
  // Handle the response:
  if(result == Gtk::RESPONSE_OK) {
    // Notice that this is a std::string, not a Glib::ustring.
    std::string filename = dialog.get_filename();
    try {
      if(session) {
        session->read_script_async({filename});
      }
    }
    catch(const std::exception& e) {
      error_message(e.what());
    }
  }
}

void tascar_window_t::on_menu_file_open()
{
  Gtk::FileChooserDialog dialog("Please choose a file",
                                Gtk::FILE_CHOOSER_ACTION_OPEN);
  dialog.set_transient_for(*this);
  // Add response buttons the the dialog:
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
  // Add filters, so that only certain file types can be selected:
  Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
  filter_tascar->set_name("tascar files");
  filter_tascar->add_pattern("*.tsc");
  dialog.add_filter(filter_tascar);
  Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
  filter_any->set_name("Any files");
  filter_any->add_pattern("*");
  dialog.add_filter(filter_any);
  // Show the dialog and wait for a user response:
  int result = dialog.run();
  // Handle the response:
  if(result == Gtk::RESPONSE_OK) {
    // Notice that this is a std::string, not a Glib::ustring.
    std::string filename = dialog.get_filename();
    try {
      get_warnings().clear();
      numlastwarnings = 0;
      statusbar_main->remove_all_messages();
      statusbar_main->push("loading session...");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      scene_load(filename);
      tascar_filename = filename;
      sessionquit = false;
      if(session) {
        session->add_bool_true("/tascargui/quit", &sessionquit);
        session->add_bool_true("/tascar/quit", &sessionquit);
      }
      reset_gui();
    }
    catch(const std::exception& e) {
      error_message(e.what());
    }
  }
}

void tascar_window_t::on_menu_file_open_example()
{
  Gtk::FileChooserDialog dialog("Please choose a file",
                                Gtk::FILE_CHOOSER_ACTION_OPEN);
  std::string examplepath = "/usr/share/tascar/examples/";
  auto homebrewprefix = localgetenv("HOMEBREW_PREFIX");
  if(homebrewprefix.size())
    examplepath = homebrewprefix + "/share/tascar/examples/";
#ifdef _WIN32
  DEBUG(examplepath);
  char buffer[MAX_PATH];
  auto res = GetModuleFileName(NULL, buffer, MAX_PATH);
  DEBUG(res);
  DEBUG(buffer);
  if(res > 0) {
    std::string binpath(buffer);
    examplepath = TASCAR::strrep(binpath, "tascar.exe", "examples");
  }
  DEBUG(examplepath);
#endif
  dialog.set_current_folder(examplepath);
  dialog.set_transient_for(*this);
  // Add response buttons the the dialog:
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
  // Add filters, so that only certain file types can be selected:
  Glib::RefPtr<Gtk::FileFilter> filter_tascar = Gtk::FileFilter::create();
  filter_tascar->set_name("tascar files");
  filter_tascar->add_pattern("*.tsc");
  dialog.add_filter(filter_tascar);
  Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
  filter_any->set_name("Any files");
  filter_any->add_pattern("*");
  dialog.add_filter(filter_any);
  // Show the dialog and wait for a user response:
  int result = dialog.run();
  // Handle the response:
  if(result == Gtk::RESPONSE_OK) {
    // Notice that this is a std::string, not a Glib::ustring.
    std::string filename = dialog.get_filename();
    try {
      get_warnings().clear();
      numlastwarnings = 0;
      statusbar_main->remove_all_messages();
      statusbar_main->push("loading session...");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      scene_load(filename);
      tascar_filename = filename;
      sessionquit = false;
      if(session) {
        session->add_bool_true("/tascargui/quit", &sessionquit);
        session->add_bool_true("/tascar/quit", &sessionquit);
      }
      reset_gui();
    }
    catch(const std::exception& e) {
      error_message(e.what());
    }
  }
}

void tascar_window_t::on_menu_view_meterw_z()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_weight = TASCAR::levelmeter::Z;
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meterw_a()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_weight = TASCAR::levelmeter::A;
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meterw_c()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_weight = TASCAR::levelmeter::C;
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meterw_bandpass()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_weight = TASCAR::levelmeter::bandpass;
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meter_rmspeak()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_mode = "rmspeak";
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meter_rms()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_mode = "rms";
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meter_peak()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_mode = "peak";
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_meter_percentile()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session)
    session->levelmeter_mode = "percentile";
  update_levelmeter_settings();
}

void tascar_window_t::on_menu_view_viewport_xy()
{
  draw.set_viewport(TSCGUI::scene_draw_t::xy);
}

void tascar_window_t::on_menu_view_viewport_rotz()
{
  draw.view.euler.z += TASCAR_PIf / 24;
}

void tascar_window_t::on_menu_view_viewport_rotzcw()
{
  draw.view.euler.z -= TASCAR_PIf / 24;
}

void tascar_window_t::on_menu_view_viewport_setref()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene))
      session->scenes[selected_scene]->guitrackobject = NULL;
    draw.select_object(NULL);
    draw.view.ref = TASCAR::pos_t();
    active_track->set_active(false);
    session_mutex.unlock();
  }
}

void tascar_window_t::on_menu_view_viewport_xyz()
{
  draw.set_viewport(TSCGUI::scene_draw_t::xyz);
}

void tascar_window_t::on_menu_view_viewport_xz()
{
  draw.set_viewport(TSCGUI::scene_draw_t::xz);
}

void tascar_window_t::on_menu_view_viewport_yz()
{
  draw.set_viewport(TSCGUI::scene_draw_t::yz);
}

void tascar_window_t::on_menu_view_show_warnings()
{
  if(session_mutex.try_lock()) {
    bool has_warning = false;
    auto ptextbuffer = text_warnings->get_buffer();
    ptextbuffer->erase(ptextbuffer->begin(), ptextbuffer->end());
    for(auto warn : get_warnings()) {
      has_warning = true;
      ptextbuffer->insert_markup(
          ptextbuffer->end(), "<span font_weight=\"bold\">Warning:</span>\n  ");
      ptextbuffer->insert(ptextbuffer->end(), warn + "\n");
    }
    if(has_warning)
      ptextbuffer->insert(ptextbuffer->end(), "\n");
    if(session) {
      std::string v = session->show_unknown();
      if(v.size()) {
        ptextbuffer->insert_markup(ptextbuffer->end(),
                                   "<span font_weight=\"bold\">Missing license "
                                   "information:</span>\n  ");
        ptextbuffer->insert(ptextbuffer->end(), v + "\n\n");
      }
      v.clear();
      session->validate_attributes(v);
      if(v.size()) {
        ptextbuffer->insert_markup(
            ptextbuffer->end(),
            "<span font_weight=\"bold\">Malformed session file:</span>\n  ");
        ptextbuffer->insert(ptextbuffer->end(), v + "\n");
        has_warning = true;
      }
    }
    if(has_warning)
      notebook->set_current_page(5);
    session_mutex.unlock();
  }
}

void tascar_window_t::on_menu_view_zoom_in()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene)) {
      draw.view.set_scale(session->scenes[selected_scene]->guiscale /=
                          1.1892071150027210269);
    }
    session_mutex.unlock();
  }
}

void tascar_window_t::on_menu_view_zoom_out()
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene)) {
      draw.view.set_scale(session->scenes[selected_scene]->guiscale *=
                          1.1892071150027210269);
    }
    session_mutex.unlock();
  }
}

bool tascar_window_t::on_map_clicked(GdkEventButton* e)
{
  if(session_mutex.try_lock()) {
    if(session && (session->scenes.size() > selected_scene)) {
      if(e->type == GDK_BUTTON_PRESS) {
        Gtk::Allocation allocation = scene_map->get_allocation();
        const int width = allocation.get_width();
        const int height = allocation.get_height();
        TASCAR::pos_t mousepos(e->x - 0.5 * width, -(e->y - 0.5 * height), 0);
        double wscale(0.5 * std::max(height, width));
        mousepos *= 1.0 / wscale;
        if(pthread_mutex_trylock(&mtx_draw) == 0) {
          double dmin(DBL_MAX);
          TASCAR::Scene::object_t* obj_nearest(NULL);
          for(std::vector<TASCAR::Scene::object_t*>::iterator it =
                  session->scenes[selected_scene]->all_objects.begin();
              it != session->scenes[selected_scene]->all_objects.end(); ++it) {
            TASCAR::pos_t opos(draw.view((*it)->c6dof.position));
            opos.z = 0;
            double d(distance(mousepos, opos));
            if((d < dmin) && (d * wscale < 30)) {
              dmin = d;
              obj_nearest = *it;
            }
          }
          if(obj_nearest) {
            // active selection
            active_object = obj_nearest;
            active_selector->set_active_text(active_object->get_name());
            update_selection_info();
            draw.select_object(active_object);
          }
          pthread_mutex_unlock(&mtx_draw);
        }
      }
    }
    session_mutex.unlock();
  }
  return false;
}

bool tascar_window_t::on_map_scroll(GdkEventScroll* e)
{
  if(((e->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0) && session) {
    switch(e->direction) {
    case GDK_SCROLL_UP:
      on_menu_view_zoom_in();
      break;
    case GDK_SCROLL_DOWN:
      on_menu_view_zoom_out();
      break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_SMOOTH:
      break;
    }
  }
  return true;
}

void tascar_window_t::on_time_changed()
{
  if(session_mutex.try_lock()) {
    double ltime(timeline->get_value());
    if(session) {
      if(ltime != draw.get_time()) {
        session->tp_locate(ltime);
      }
    }
    session_mutex.unlock();
  }
}

void tascar_window_t::on_menu_help_manual()
{
  // open tascar manual
  // this is deprecated in Gtk 3.3:
  gtk_show_uri(NULL, "file:///usr/share/doc/tascar/manual.pdf",
               GDK_CURRENT_TIME, NULL);
  // Gtk::gtk_show_uri_on_window( this,
  // "file:///usr/share/doc/tascar/manual.pdf", GDK_CURRENT_TIME, NULL );
}

void tascar_window_t::on_menu_help_bugreport()
{
  // open tascar manual
  // this is deprecated in Gtk 3.3:

  std::string uri("https://github.com/gisogrimm/tascar/issues/"
                  "new?body=TASCARpro%20version%20");
  uri += TASCARVER;
  gtk_show_uri(NULL, uri.c_str(), GDK_CURRENT_TIME, NULL);
  // Gtk::gtk_show_uri_on_window( this,
  // "file:///usr/share/doc/tascar/manual.pdf", GDK_CURRENT_TIME, NULL );
}

void tascar_window_t::on_menu_help_about()
{
  Gtk::AboutDialog* aboutdialog(NULL);
  m_refBuilder->get_widget("aboutdialog", aboutdialog);
  if(aboutdialog) {
    aboutdialog->run();
    aboutdialog->hide();
  }
}

void tascar_window_t::on_menu_transport_play()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    session->tp_start();
  }
}

void tascar_window_t::on_menu_transport_stop()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    session->tp_stop();
  }
}

void tascar_window_t::on_menu_transport_rewind()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    double cur_time(std::max(0.0, session->tp_get_time() - 10.0));
    session->tp_locate(cur_time);
  }
}

void tascar_window_t::on_menu_transport_forward()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    double cur_time(std::min(session->duration, session->tp_get_time() + 10.0));
    session->tp_locate(cur_time);
  }
}

void tascar_window_t::on_menu_transport_previous()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    session->tp_locate(0.0);
  }
}

void tascar_window_t::on_menu_transport_next()
{
  std::lock_guard<std::mutex> lock(session_mutex);
  if(session) {
    session->tp_locate(session->duration);
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
