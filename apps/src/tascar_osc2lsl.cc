/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
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
#include <cstdint>
#include <getopt.h>
#include <iostream>
#include <lo/lo.h>
#include <lo/lo_cpp.h>
#include <lsl_cpp.h>
#include <map>
#include <unistd.h>

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl

// stream map:
typedef std::map<std::string, lsl::stream_outlet*> stream_map_t;

// use first row as timestamp, and the rest as data:
bool timestamp(false);

static int send_something(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message, void* user_data)
{
  if(argc > timestamp) {
    bool all_same(true);
    for(int32_t k = 1; k < argc; ++k)
      if(types[k] != types[0])
        all_same = false;
    if(all_same) {
      std::string name(path);
      name += types;
      stream_map_t* smap((stream_map_t*)user_data);
      lsl::stream_outlet* sop(NULL);
      if(smap->find(name) == smap->end()) {
        // stream is new, add a new descriptor:
        lsl::channel_format_t lslfmt(lsl::cf_float32);
        switch(types[0]) {
        case 'd':
          lslfmt = lsl::cf_double64;
          break;
        case 'i':
          lslfmt = lsl::cf_int32;
          break;
        case 's':
          lslfmt = lsl::cf_string;
          break;
        }
        int numchannels(argc);
        if(timestamp && (argc > 1))
          --numchannels;
        lsl::stream_info info(path, "osc2lsl", numchannels, lsl::IRREGULAR_RATE,
                              lslfmt, name.c_str());
        sop = new lsl::stream_outlet(info);
        (*smap)[name] = sop;
        std::cout << "allocating name=" << path << " id=" << name << std::endl;
      } else {
        sop = (*smap)[name];
      }
      if(sop) {
        std::int32_t startchannel(timestamp);
        switch(types[0]) {
        case 'f': {
          std::vector<float> data;
          for(std::int32_t k = startchannel; k < argc; ++k)
            data.push_back(argv[k]->f);
          if(timestamp)
            sop->push_sample(data, argv[0]->f);
          else
            sop->push_sample(data);
          break;
        }
        case 'd': {
          std::vector<double> data;
          for(std::int32_t k = startchannel; k < argc; ++k)
            data.push_back(argv[k]->d);
          if(timestamp)
            sop->push_sample(data, argv[0]->d);
          else
            sop->push_sample(data);
          break;
        }
        case 'i': {
          std::vector<int32_t> data;
          for(std::int32_t k = startchannel; k < argc; ++k)
            data.push_back(argv[k]->i);
          if(timestamp)
            sop->push_sample(data, argv[0]->i);
          else
            sop->push_sample(data);
          break;
        }
        case 's': {
          std::string str(&(argv[0]->s));
          sop->push_sample(&str);
          // sop->push_sample({std::string(&(argv[0]->s))});
          break;
        }
        }
      }
    }
  }
  return 0;
}

void add_stream(stream_map_t& streams, const std::string& path_and_format)
{
  size_t p(path_and_format.find(":"));
  if(p != std::string::npos) {
    std::string types(path_and_format.substr(p + 1));
    std::string path(path_and_format.substr(0, p));
    bool all_same(true);
    for(uint32_t k = 1; k < types.size(); ++k)
      if(types[k] != types[0])
        all_same = false;
    if(all_same) {
      int32_t argc(types.size());
      std::string name(path);
      name += types;
      lsl::stream_outlet* sop(NULL);
      if(streams.find(name) == streams.end()) {
        lsl::channel_format_t lslfmt(lsl::cf_float32);
        switch(types[0]) {
        case 'f':
          break;
        case 'd':
          lslfmt = lsl::cf_double64;
          break;
        case 'i':
          lslfmt = lsl::cf_int32;
          break;
        case 's':
          lslfmt = lsl::cf_string;
          break;
        default:
          throw TASCAR::ErrMsg(std::string("Invalid format key '") + types[0] +
                               "' while parsing '" + path_and_format + "'.");
        }
        lsl::stream_info info(path, "osc2lsl", argc, lsl::IRREGULAR_RATE,
                              lslfmt, name.c_str());
        sop = new lsl::stream_outlet(info);
        streams[name] = sop;
        std::cout << "allocating name=" << path << " id=" << name << std::endl;
      }
    }
  }
}

int main(int argc, char** argv)
{
  stream_map_t streams;
  int32_t port(-1);
  const char* options = "ha:np:t";
  struct option long_options[] = {{"help", 0, 0, 'h'},      {"add", 1, 0, 'a'},
                                  {"noauto", 0, 0, 'n'},    {"port", 1, 0, 'p'},
                                  {"timestamp", 0, 0, 't'}, {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  bool noauto(false);
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
          "osc2lsl", long_options, "",
          "To add streams manually, specify it as '<path>:<format>', e.g., "
          "'/path:ff'.\n"
          "<format> can be 'i' (integer), 'f' (32 bit float) or 's' (string).");
      return 0;
    case 'a':
      add_stream(streams, optarg);
      break;
    case 'n':
      noauto = true;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      timestamp = true;
      break;
    }
  }
  if(port < 0) {
    TASCAR::app_usage(
        "osc2lsl", long_options, "",
        "To add streams manually, specify it as '<path>:<format>', e.g., "
        "'/path:ff'.\n"
        "<format> can be 'i' (integer), 'f' (32 bit float) or 's' (string).");
    return -1;
  }
  lo::ServerThread st(port);
  if(!st.is_valid()) {
    std::cerr << "Unable to start OSC server thread." << std::endl;
    return 1;
  }
  std::atomic<bool> b_quit(false);
  st.add_method("/osc2lsl/quit", "",
                [&b_quit](lo_arg**, int) { b_quit = true; });
  for(auto& t : streams) {
    st.add_method(t.second->info().name(),
                  t.first.substr(t.second->info().name().size()),
                  &send_something, &streams);
  }
  if(!noauto)
    st.add_method(NULL, NULL, &send_something, &streams);
  st.start();
  while(!b_quit)
    usleep(10000);
  st.stop();
  for(stream_map_t::iterator it = streams.begin(); it != streams.end(); ++it)
    delete it->second;
}

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
