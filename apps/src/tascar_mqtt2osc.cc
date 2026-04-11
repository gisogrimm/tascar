/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2026 Giso Grimm
 */
/**
   \file tascar_mqtt2osc.cc
   \ingroup apptascar
   \brief subscribe to MQTT messages and convert them to OSC messages
   \author Giso Grimm
   \date 2026

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#include "cli.h"
#include "errorhandling.h"
#include <cstring>
#include <iostream>
#include <lo/lo.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

// Configuration:
std::string mqtt_host = "127.0.0.1";
int mqtt_port = 1883;
std::string mqtt_topic = "#";
std::string osc_target_url = "osc.udp://localhost:9877";
bool parse_json = false;

// Global OSC address object
lo_address osc_target;

void json_parse(const std::string& prefix, nlohmann::json& js)
{
  for(auto& [key, val] : js.items()) {
    std::string lprefix = prefix + "/" + key;
    if(val.is_object())
      json_parse(lprefix, val);
    else {
      switch(val.type()) {
      case nlohmann::json::value_t::number_float: {
        float v = val;
        lo_send(osc_target, lprefix.c_str(), "f", v);
        break;
      }
      case nlohmann::json::value_t::boolean:
      case nlohmann::json::value_t::number_integer:
      case nlohmann::json::value_t::number_unsigned: {
        int v = val;
        lo_send(osc_target, lprefix.c_str(), "i", v);
        break;
      }
      case nlohmann::json::value_t::string: {
        std::string v = val;
        lo_send(osc_target, lprefix.c_str(), "s", v.c_str());
        break;
      }
      default:
        lo_send(osc_target, lprefix.c_str(), "");
        break;
      }
    }
  }
}

// Callback: Handles incoming MQTT messages
void on_message(struct mosquitto*, void*,
                const struct mosquitto_message* message)
{
  std::string osc_path = message->topic;
  if(osc_path.empty())
    osc_path = "/";
  if(osc_path[0] != '/')
    osc_path.insert(0, "/");

  if(message->payloadlen) {
    std::cout << "[MQTT] Topic: " << message->topic
              << " | Payload: " << (char*)message->payload << std::endl;
    if(parse_json) {
      try {
        nlohmann::json js = nlohmann::json::parse((char*)message->payload);
        json_parse(osc_path, js);
      }
      catch(const std::exception& err) {
        std::cerr << "Error parsing payload as json\n";
      }
    } else {
      // Forward to OSC
      // We replace MQTT slashes '/' with OSC slashes '/' (they are the same)
      // We send the payload as a string argument.
      int lo_result =
          lo_send(osc_target, osc_path.c_str(), "s", (char*)message->payload);

      if(lo_result == -1) {
        std::cerr << "[OSC] Error sending message: "
                  << lo_address_errno(osc_target) << " "
                  << lo_address_errstr(osc_target) << std::endl;
      }
    }
  } else {
    std::cout << "[MQTT] Topic: " << message->topic << " | (empty payload)"
              << std::endl;
    // Send empty string or specific OSC nil if needed, here we send empty
    // string
    lo_send(osc_target, osc_path.c_str(), "");
  }
}

// Callback: Handles connection setup
void on_connect(struct mosquitto* mosq, void*, int result)
{
  if(result == 0) {
    // Subscribe to everything once connected
    mosquitto_subscribe(mosq, NULL, mqtt_topic.c_str(), 0);
  }
}

int main(int argc, char** argv)
{
  // parse command line parameters:
  const char* options = "m:p:u:t:jh";
  struct option long_options[] = {{"mqtthost", 1, 0, 'm'},
                                  {"mqttport", 1, 0, 'p'},
                                  {"mqtttopic", 1, 0, 't'},
                                  {"oscurl", 1, 0, 'u'},
                                  {"parsejson", 0, 0, 'j'},
                                  {"help", 0, 0, 'h'},
                                  {0, 0, 0, 0}};

  std::map<std::string, std::string> helpmap;
  helpmap["mqtthost"] =
      "Hostname or IP address of MQTT broker (" + mqtt_host + ")";
  helpmap["mqttport"] =
      "Port number of MQTT broker (" + std::to_string(mqtt_port) + ")";
  helpmap["mqtttopic"] = "MQTT topic (" + mqtt_topic + ")";
  helpmap["oscurl"] = "URL of OSC target (" + osc_target_url + ")";
  helpmap["parsejson"] = "Parse payload as json messages";
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        EOF) {
    switch(opt) {
    case '?':
      throw TASCAR::ErrMsg("Invalid option.");
      break;
    case ':':
      throw TASCAR::ErrMsg("Missing argument.");
      break;
    case 'm':
      mqtt_host = optarg;
      break;
    case 't':
      mqtt_topic = optarg;
      break;
    case 'p':
      mqtt_port = atoi(optarg);
      break;
    case 'j':
      parse_json = true;
      break;
    case 'u':
      osc_target_url = optarg;
      break;
    case 'h':
      TASCAR::app_usage(
          "tascar_mqtt2osc", long_options, "",
          "tascar_mqtt2osc connects to an MQTT broker, subscribes to "
          "messages, "
          "parses json payload, and forwards them as OSC messages.",
          helpmap);
      return 0;
    }
  }

  struct mosquitto* mosq = NULL;
  int rc = 0;

  // 1. Initialize OSC Target
  osc_target = lo_address_new_from_url(osc_target_url.c_str());
  if(!osc_target) {
    std::cerr << "Failed to create OSC address." << std::endl;
    return 1;
  }

  // 2. Initialize Mosquitto Library
  mosquitto_lib_init();

  // 3. Create a Mosquitto Client Instance (Clean Session: true)
  mosq = mosquitto_new(NULL, true, NULL);
  if(!mosq) {
    std::cerr << "Error: Out of memory." << std::endl;
    return 1;
  }

  // 4. Set Callbacks
  mosquitto_connect_callback_set(mosq, on_connect);
  mosquitto_message_callback_set(mosq, on_message);

  // 5. Connect to MQTT Broker
  std::cout << "Connecting to MQTT broker at " << mqtt_host << ":" << mqtt_port
            << "..." << std::endl;
  rc = mosquitto_connect(mosq, mqtt_host.c_str(), mqtt_port, 60);
  if(rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "Unable to connect to MQTT broker." << std::endl;
    return 1;
  }

  // 6. Main Loop
  // mosquitto_loop runs the network processing.
  // It blocks for a short period or returns immediately depending on
  // implementation, effectively acting as our main event loop.
  std::cout << "Running MQTT -> OSC Bridge..." << std::endl;
  while(1) {
    rc = mosquitto_loop(mosq, -1,
                        1); // -1 = block until network activity, timeout 1s
    if(rc) {
      std::cerr << "Connection error! Reconnecting..." << std::endl;
      sleep(1);
      mosquitto_reconnect(mosq);
    }
  }

  // 7. Cleanup
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  lo_address_free(osc_target);

  return 0;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
