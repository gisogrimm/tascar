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

#include "session.h"

/**
 * @brief Actor module for moving objects along a predefined path.
 *
 * This module controls the position and orientation of a scene object (actor)
 * based on time. It supports interpolation of location and orientation tracks,
 * and can calculate orientation based on the direction of movement (tangent).
 */
class motionpath_t : public TASCAR::actor_module_t {
public:
  /**
   * @brief Constructor
   * @param cfg Module configuration containing XML parameters and session
   * context.
   */
  motionpath_t(const TASCAR::module_cfg_t& cfg);

  /**
   * @brief Destructor
   */
  ~motionpath_t();

  /**
   * @brief Updates the actor's position and orientation for the current audio
   * frame.
   * @param frame Current frame number (used if tascartime is enabled).
   * @param running True if the session transport is running.
   */
  void update(uint32_t frame, bool running);

  /**
   * @brief Static callback function for OSC messages to trigger the 'go'
   * method.
   * @param path OSC path (unused).
   * @param types OSC type tags.
   * @param argv OSC arguments (expects two floats: start time, end time).
   * @param argc Number of arguments.
   * @param msg OSC message (unused).
   * @param user_data Pointer to the motionpath_t instance.
   * @return 0 on success.
   */
  static int osc_go(const char* path, const char* types, lo_arg** argv,
                    int argc, lo_message msg, void* user_data);

  /**
   * @brief Starts playback of the motion path between specific times.
   * @param start Start time in seconds.
   * @param end End time (stop time) in seconds.
   */
  void go(double start, double end);

private:
  double time = 0.0;              ///< Current playback time in seconds.
  double stoptime = 3153600000.0; ///< Time at which playback stops
                                  ///< automatically (100 years from now).
  bool running = false;           ///< Playback state flag.
  std::string id;                 ///< Identifier for OSC methods.
  bool active = true; ///< Activation flag; if false, updates are skipped.
  bool tascartime =
      false; ///< If true, use session time instead of internal clock.
  TASCAR::track_t location; ///< Spatial track defining position over time.
  TASCAR::euler_track_t orientation; ///< Track defining orientation over time.
  double sampledorientation = 0.0;   ///< Distance offset in meters to calculate
                                   ///< orientation based on movement direction
                                   ///< (tangent). 0 disables this feature.
};

int motionpath_t::osc_go(const char*, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  // Validates OSC input and invokes the go method.
  if(user_data && (argc == 2) && (types[0] == 'f') && (types[1] == 'f'))
    ((motionpath_t*)user_data)->go(argv[0]->f, argv[1]->f);
  return 0;
}

void motionpath_t::go(double start, double end)
{
  // Resets and starts playback from 'start' to 'end'.
  running = false;
  stoptime = end;
  time = start;
  running = true;
}

motionpath_t::motionpath_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg)
{
  // Read XML attributes
  GET_ATTRIBUTE_BOOL(
      active, "Enables or disables the plugin. If set to false, the motion "
              "path is ignored and the actor remains in its current state.");
  GET_ATTRIBUTE_BOOL(tascartime,
                     "Determines the time reference. If true, the path "
                     "position is synchronized with the main TASCAR session "
                     "time. If false, the plugin uses its own internal clock, "
                     "controllable via OSC (/go, /start, /stop).");
  GET_ATTRIBUTE(
      id, "",
      "The identifier used for OSC control paths (e.g., /motionpath/go).");
  GET_ATTRIBUTE(
      sampledorientation, "m",
      "Defines how orientation is calculated. If 0, orientation is taken from "
      "the <orientation> track. If non-zero, orientation is calculated based "
      "on the direction of movement (tangent) looking ahead (positive value) "
      "or behind (negative value) by the specified distance along the path in "
      "meters.");

  // Set default ID if not provided
  if(id.empty())
    id = "motionpath";

  // Parse XML children for position, orientation, and creator nodes
  for(auto sne : tsccfg::node_get_children(e)) {
    if(sne && (tsccfg::node_get_name(sne) == "position")) {
      // Load the position track from XML
      location.read_xml(sne);
    }
    if(sne && (tsccfg::node_get_name(sne) == "orientation")) {
      // Load the orientation track from XML
      orientation.read_xml(sne);
    }
    if(sne && (tsccfg::node_get_name(sne) == "creator")) {
      // Process creator nodes to generate path points and calculate orientation
      // based on the direction of movement between points.
      for(auto node : tsccfg::node_get_children(sne))
        location.edit(node);
      TASCAR::track_t::iterator it_old = location.end();
      double old_azim(0);
      double new_azim(0);
      for(TASCAR::track_t::iterator it = location.begin(); it != location.end();
          ++it) {
        if(it_old != location.end()) {
          TASCAR::pos_t p = it->second;
          p -= it_old->second;
          new_azim = p.azim();
          // Unwrap azimuth to prevent jumps at +/- PI
          while(new_azim - old_azim > TASCAR_PI)
            new_azim -= TASCAR_2PI;
          while(new_azim - old_azim < -TASCAR_PI)
            new_azim += TASCAR_2PI;
          // Set orientation to face the direction of movement
          orientation[it_old->first] = TASCAR::zyx_euler_t(new_azim, 0, 0);
          old_azim = new_azim;
        }
        // Only update iterator if position changed
        if(TASCAR::distance(it->second, it_old->second) > 0)
          it_old = it;
      }
    }
  }

  session->set_variable_owner("motionpath");
  // Register OSC methods if not using TASCAR session time
  if(!tascartime) {
    session->add_method("/" + id + "/go", "ff", &motionpath_t::osc_go, this);
    session->add_bool_true("/" + id + "/start", &running);
    session->add_bool_false("/" + id + "/stop", &running);
    session->add_double("/" + id + "/locate", &time);
    session->add_double("/" + id + "/stoptime", &stoptime);
  }
  // Register active state OSC method
  session->add_bool("/" + id + "/active", &active);
  session->unset_variable_owner();
}

motionpath_t::~motionpath_t() {}

void motionpath_t::update(uint32_t tp_frame, bool)
{
  // Skip update if module is inactive
  if(!active)
    return;

  // Handle stop time
  if(time > stoptime) {
    time = stoptime;
    running = false;
  }

  // Increment internal time if running
  if(running)
    time += t_fragment;

  // Determine lookup time (internal or session time)
  double ltime(time);
  if(tascartime)
    ltime = tp_frame * t_sample;

  TASCAR::c6dof_t c6dof_;

  // Interpolate position
  c6dof_.position = location.interp(ltime);

  // Calculate orientation
  if(sampledorientation == 0) {
    // Use explicit orientation track
    c6dof_.orientation = orientation.interp(ltime);
  } else {
    // Calculate orientation based on the tangent of the path
    // (look-ahead/look-behind)
    double tp(location.get_time(location.get_dist(ltime) - sampledorientation));
    TASCAR::pos_t pdt(c6dof_.position);
    pdt -= location.interp(tp);
    if(sampledorientation < 0)
      pdt *= -1.0;
    c6dof_.orientation.z = pdt.azim();
    c6dof_.orientation.y = pdt.elev();
    c6dof_.orientation.x = 0.0;
  }

  // Apply the calculated transform to all attached objects
  for(auto iobj : obj) {
    iobj.obj->dorientation = c6dof_.orientation;
    iobj.obj->dlocation = c6dof_.position;
  }
}

REGISTER_MODULE(motionpath_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
