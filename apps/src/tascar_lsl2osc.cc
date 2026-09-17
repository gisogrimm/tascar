/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2024 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cli.h"
#include "errorhandling.h"
#include <atomic>
#include <getopt.h>
#include <iostream>
#include <lo/lo.h>
#include <lsl_cpp.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl

std::atomic<bool> b_quit(false);

void lsl_thread(const std::string& name, const std::string& url,
                const std::string& prefix)
{
  try {
    std::string oscpath = prefix + "/" + name;
    lo_address loaddr = NULL;
    if(!url.empty())
      loaddr = lo_address_new_from_url(url.c_str());

    while(!b_quit) {
      // Resolve stream:
      std::vector<lsl::stream_info> infos =
          lsl::resolve_stream("name", name, 1, 1.0);
      if(infos.empty()) {
        if(!b_quit)
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }

      if(infos.size() > 1)
        std::cerr << "Warning: More than one LSL stream with name \"" << name
                  << "\" found. Using the first match." << std::endl;

      auto cfmt = infos[0].channel_format();
      if(!((cfmt == lsl::cf_float32) || (cfmt == lsl::cf_double64))) {
        std::cerr
            << "Warning: The LSL stream with name \"" << name
            << "\" has no floating point data. The stream will be ignored."
            << std::endl;
        return;
      }

      auto streamchannels = infos[0].channel_count();
      auto inlet = lsl::stream_inlet(infos[0]);
      std::vector<double> sample(streamchannels);

      while(!b_quit) {
        // get LSL sample, store time:
        double t = inlet.pull_sample(sample, 0.1);
        if(t > 0.0) {
          lo_message msg = lo_message_new();
          if(msg) {
            lo_message_add_double(msg, t);
            for(auto s : sample)
              lo_message_add_double(msg, s);
            if(loaddr)
              lo_send_message(loaddr, oscpath.c_str(), msg);
            else
              std::cerr << "No OSC URL provided, dropping message."
                        << std::endl;
            lo_message_free(msg);
          }
        }
      }
    }
    if(loaddr)
      lo_address_free(loaddr);
  }
  catch(const std::exception& e) {
    std::cerr << "Error in LSL thread for \"" << name << "\": " << e.what()
              << std::endl;
  }
}

int main(int argc, char** argv)
{
  std::vector<std::string> streams;
  std::string url;
  std::string prefix = "/lsl2osc";
  const char* options = "ha:p:u:";
  struct option long_options[] = {{"help", 0, 0, 'h'},
                                  {"add", 1, 0, 'a'},
                                  {"prefix", 1, 0, 'p'},
                                  {"url", 1, 0, 'u'},
                                  {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);

  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case '?':
      throw TASCAR::ErrMsg("Invalid option.");
      break;
    case ':':
      throw TASCAR::ErrMsg("Missing argument.");
      break;
    case 'h':
      TASCAR::app_usage(
          "lsl2osc", long_options, "",
          "Convert LSL streams to OSC messages.\n"
          "Example: lsl2osc -a MyStream -u osc.udp://localhost:9000");
      return 0;
    case 'a':
      streams.push_back(optarg);
      break;
    case 'p':
      prefix = optarg;
      break;
    case 'u':
      url = optarg;
      break;
    }
  }

  if(streams.empty()) {
    TASCAR::app_usage("lsl2osc", long_options, "",
                      "No streams defined. Use -a to add streams.");
    return -1;
  }

  std::vector<std::thread*> threads;
  for(auto s : streams) {
    std::cout << "Starting thread for stream: " << s << std::endl;
    threads.push_back(new std::thread(lsl_thread, s, url, prefix));
  }

  std::cout << "Running. Press Ctrl-C to stop." << std::endl;
  while(!b_quit)
    usleep(10000);

  std::cout << "Stopping..." << std::endl;
  for(auto th : threads) {
    th->join();
    delete th;
  }

  return 0;
}

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
