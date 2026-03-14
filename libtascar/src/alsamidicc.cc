/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

/*
 * MacOS Support Extension:
 * To compile this code on macOS, you must add the following members to the
 * 'midi_ctl_t' class definition in 'alsamidicc.h'. Ensure they are public or
 * add appropriate friend declarations for the callback function.
 *
 * #ifdef __APPLE__
 *   MIDIClientRef mac_client;
 *   MIDIPortRef mac_port_in;
 *   MIDIPortRef mac_port_out;
 *   MIDIEndpointRef mac_endpoint_in;
 *   MIDIEndpointRef mac_endpoint_out;
 *   std::deque<midi_event_data_t> midi_queue;
 *   std::mutex midi_queue_mutex;
 * #endif
 *
 * Also define the helper struct in the header or before the class:
 * struct midi_event_data_t { std::vector<uint8_t> data; };
 */

#include "alsamidicc.h"
#include "errorhandling.h"
#include "tscconfig.h"
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <CoreFoundation/CFRunLoop.h>
#include <CoreMIDI/MIDIServices.h>
#include <CoreFoundation/CFString.h>

// Static callback for CoreMIDI input
static void midiReadCallback(const MIDIPacketList* pktlist,
                             void* readProcRefCon, void* srcConnRefCon)
{
  TASCAR::midi_ctl_t* obj = static_cast<TASCAR::midi_ctl_t*>(readProcRefCon);
  if(!obj)
    return;

  // Lock the queue and push events
  std::lock_guard<std::mutex> lock(obj->midi_queue_mutex);

  const MIDIPacket* packet = &pktlist->packet[0];
  for(unsigned int i = 0; i < pktlist->numPackets; ++i) {
    midi_event_data_t ev;
    ev.data.assign(packet->data, packet->data + packet->length);
    obj->midi_queue.push_back(ev);
    packet = MIDIPacketNext(packet);
  }
}
#endif

TASCAR::midi_ctl_t::midi_ctl_t(const std::string& cname)
{
#if defined(__linux__)
  if(snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0)
    throw TASCAR::ErrMsg("Unable to open MIDI sequencer.");
  snd_seq_set_client_name(seq, cname.c_str());
  snd_seq_drop_input(seq);
  snd_seq_drop_input_buffer(seq);
  snd_seq_drop_output(seq);
  snd_seq_drop_output_buffer(seq);
  port_in.port = snd_seq_create_simple_port(
      seq, "control", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
      SND_SEQ_PORT_TYPE_APPLICATION);
  port_out.port = snd_seq_create_simple_port(
      seq, "feedback", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
      SND_SEQ_PORT_TYPE_APPLICATION);
  port_in.client = snd_seq_client_id(seq);
  port_out.client = snd_seq_client_id(seq);
#elif defined(__APPLE__)
  // macOS CoreMIDI Initialization
  CFStringRef clname_ref = CFStringCreateWithCString(NULL,cname.c_str(),CFStringBuiltInEncodings.UTF8);
  
  OSStatus status =
      MIDIClientCreate(cname, NULL, NULL, &mac_client);
  if(status != noErr) {
    throw TASCAR::ErrMsg("Unable to create MIDI client.");
  }

  status = MIDIInputPortCreate(mac_client, CFSTR("Input Port"),
                               midiReadCallback, this, &mac_port_in);
  if(status != noErr) {
    MIDIClientDispose(mac_client);
    throw TASCAR::ErrMsg("Unable to create MIDI input port.");
  }

  status =
      MIDIOutputPortCreate(mac_client, CFSTR("Output Port"), &mac_port_out);
  if(status != noErr) {
    MIDIPortDispose(mac_port_in);
    MIDIClientDispose(mac_client);
    throw TASCAR::ErrMsg("Unable to create MIDI output port.");
  }

  mac_endpoint_in = 0;
  mac_endpoint_out = 0;
#endif
}

void TASCAR::midi_ctl_t::set_nonblock(bool nonblock)
{
#if defined(__linux__)
  snd_seq_nonblock(seq, nonblock);
#endif
}

void TASCAR::midi_ctl_t::connect_input(int src_client, int src_port)
{
#if defined(__linux__)
  snd_seq_addr_t src_port_;
  src_port_.client = src_client;
  src_port_.port = src_port;
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_port_subscribe_set_sender(subs, &src_port_);
  snd_seq_port_subscribe_set_dest(subs, &port_in);
  snd_seq_port_subscribe_set_queue(subs, 1);
  snd_seq_port_subscribe_set_time_update(subs, 1);
  snd_seq_port_subscribe_set_time_real(subs, 1);
  snd_seq_subscribe_port(seq, subs);
#elif defined(__APPLE__)
  throw TASCAR::ErrMsg(
      "Connecting by integer client/port is not supported on macOS, use name.");
#endif
}

void TASCAR::midi_ctl_t::connect_output(int src_client, int src_port)
{
#if defined(__linux__)
  snd_seq_addr_t src_port_;
  src_port_.client = src_client;
  src_port_.port = src_port;
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_port_subscribe_set_sender(subs, &port_out);
  snd_seq_port_subscribe_set_dest(subs, &src_port_);
  snd_seq_port_subscribe_set_queue(subs, 1);
  snd_seq_port_subscribe_set_time_update(subs, 1);
  snd_seq_port_subscribe_set_time_real(subs, 1);
  snd_seq_subscribe_port(seq, subs);
#elif defined(__APPLE__)
  throw TASCAR::ErrMsg(
      "Connecting by integer client/port is not supported on macOS, use name.");
#endif
}

void TASCAR::midi_ctl_t::service()
{
#if defined(__linux__)
  snd_seq_drop_input(seq);
  snd_seq_drop_input_buffer(seq);
  snd_seq_drop_output(seq);
  snd_seq_drop_output_buffer(seq);
  snd_seq_event_t* ev = NULL;
  while(run_service) {
    while(snd_seq_event_input(seq, &ev) >= 0) {
      if(ev) {
        switch(ev->type) {
        case SND_SEQ_EVENT_CONTROLLER:
          emit_event(ev->data.control.channel, ev->data.control.param,
                     ev->data.control.value);
          break;
        case SND_SEQ_EVENT_PITCHBEND:
          emit_event(ev->data.control.channel, ev->data.control.param,
                     ev->data.control.value);
          break;
        case SND_SEQ_EVENT_NOTE:
        case SND_SEQ_EVENT_NOTEON:
        case SND_SEQ_EVENT_NOTEOFF:
          emit_event_note(ev->data.note.channel, ev->data.note.note,
                          ((ev->type == SND_SEQ_EVENT_NOTEOFF)
                               ? 0
                               : (ev->data.note.velocity)));
          break;
        case SND_SEQ_EVENT_SYSEX:
          uint8_t* buf((uint8_t*)(ev->data.ext.ptr));
          if((ev->data.ext.len == 6) && (buf[0] == 0xf0) && (buf[1] == 0x7f) &&
             (buf[3] == 0x06) && (buf[5] == 0xf7))
            emit_event_mmc(buf[2], buf[4]);
          break;
        }
      }
    }
    usleep(10);
  }
#elif defined(__APPLE__)
  // On macOS, process the queue populated by the callback
  while(run_service) {
    std::vector<midi_event_data_t> local_queue;
    {
      std::lock_guard<std::mutex> lock(midi_queue_mutex);
      if(!midi_queue.empty()) {
        local_queue.swap(midi_queue);
      }
    }

    for(auto& ev_data : local_queue) {
      if(ev_data.data.empty())
        continue;

      uint8_t status = ev_data.data[0];
      uint8_t channel = status & 0x0F;
      uint8_t type = (status >> 4) & 0x0F;

      if((status >= 0x80) && (status <= 0xEF)) {
        if(ev_data.data.size() >= 2) {
          uint8_t data1 = ev_data.data[1];
          uint8_t data2 = (ev_data.data.size() > 2) ? ev_data.data[2] : 0;

          switch(type) {
          case 0x08: // Note Off
            emit_event_note(channel, data1, 0);
            break;
          case 0x09: // Note On
            emit_event_note(channel, data1, data2);
            break;
          case 0x0B: // Control Change
            emit_event(channel, data1, data2);
            break;
          case 0x0E: // Pitch Bend
          {
            int value = ((data2 << 7) | data1) - 8192;
            emit_event(channel, 0, value);
          } break;
          }
        }
      } else if(status == 0xF0) {
        // SysEx
        if(ev_data.data.size() >= 6 && ev_data.data[0] == 0xF0 &&
           ev_data.data[1] == 0x7F && ev_data.data[3] == 0x06 &&
           ev_data.data[5] == 0xF7) {
          emit_event_mmc(ev_data.data[2], ev_data.data[4]);
        }
      }
    }

    if(local_queue.empty()) {
      usleep(10);
    }
  }
#endif
}

TASCAR::midi_ctl_t::~midi_ctl_t()
{
#if defined(__linux__)
  snd_seq_close(seq);
#elif defined(__APPLE__)
  if(mac_port_in)
    MIDIPortDispose(mac_port_in);
  if(mac_port_out)
    MIDIPortDispose(mac_port_out);
  if(mac_client)
    MIDIClientDispose(mac_client);
#endif
}

void TASCAR::midi_ctl_t::drain_and_sync_output()
{
#if defined(__linux__)
  int err = 1;
  int cnt = 10;
  while((err > 0) && (cnt > 0)) {
    if(minwait > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(minwait));
    err = snd_seq_drain_output(seq);
    --cnt;
    if(err != 0)
      DEBUG(err);
  }
  if(minwait > 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(minwait));
  err = snd_seq_sync_output_queue(seq);
  if(err < 0)
    DEBUG(err);
#elif defined(__APPLE__)
  if(minwait > 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(minwait));
#endif
}

void TASCAR::midi_ctl_t::send_midi(int channel, int param, int value)
{
#if defined(__linux__)
  snd_seq_event_t ev;
  memset(&ev, 0, sizeof(ev));
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type = SND_SEQ_EVENT_CONTROLLER;
  ev.data.control.channel = (unsigned char)(channel);
  ev.data.control.param = (unsigned char)(param);
  ev.data.control.value = (unsigned char)(value);
  int err = snd_seq_event_output_direct(seq, &ev);
  if(err < 0) {
    DEBUG(err);
  }
  drain_and_sync_output();
#elif defined(__APPLE__)
  if(mac_endpoint_out == 0)
    return;

  MIDIPacketList packetList;
  packetList.numPackets = 1;
  MIDIPacket* packet = &packetList.packet[0];

  packet->timeStamp = 0;
  packet->length = 3;
  packet->data[0] = 0xB0 | (channel & 0x0F);
  packet->data[1] = param & 0x7F;
  packet->data[2] = value & 0x7F;

  OSStatus status = MIDISend(mac_port_out, mac_endpoint_out, &packetList);
  if(status != noErr) {
    DEBUG(status);
  }
  drain_and_sync_output();
#endif
}

void TASCAR::midi_ctl_t::send_midi_sysex(int len, char* data)
{
#if defined(__linux__)
  snd_seq_event_t ev;
  memset(&ev, 0, sizeof(ev));
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  snd_seq_ev_set_sysex(&ev, len, data);
  int err = snd_seq_event_output_direct(seq, &ev);
  if(err < 0) {
    DEBUG(err);
    DEBUG(strerror(-err));
  }
  drain_and_sync_output();
#elif defined(__APPLE__)
  if(mac_endpoint_out == 0)
    return;

  char buffer[sizeof(MIDIPacketList) + len + 64];
  MIDIPacketList* pktList = (MIDIPacketList*)buffer;

  pktList->numPackets = 1;
  MIDIPacket* pkt = MIDIPacketListInit(pktList);

  pkt = MIDIPacketListAdd(pktList, sizeof(buffer), pkt, 0, len,
                          (const Byte*)data);

  if(pkt) {
    OSStatus status = MIDISend(mac_port_out, mac_endpoint_out, pktList);
    if(status != noErr) {
      DEBUG(status);
    }
  }
  drain_and_sync_output();
#endif
}

void TASCAR::midi_ctl_t::send_midi_channel_pressure(int channel, int param,
                                                    int value)
{
#if defined(__linux__)
  snd_seq_event_t ev;
  memset(&ev, 0, sizeof(ev));
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type = SND_SEQ_EVENT_CHANPRESS;
  ev.data.control.channel = (unsigned char)(channel);
  ev.data.control.param = (unsigned char)(param);
  ev.data.control.value = (unsigned char)(value);
  int err = snd_seq_event_output_direct(seq, &ev);
  if(err < 0) {
    DEBUG(err);
    DEBUG(strerror(-err));
  }
  drain_and_sync_output();
#elif defined(__APPLE__)
  if(mac_endpoint_out == 0)
    return;

  MIDIPacketList packetList;
  packetList.numPackets = 1;
  MIDIPacket* packet = &packetList.packet[0];

  packet->timeStamp = 0;
  packet->length = 2;
  packet->data[0] = 0xD0 | (channel & 0x0F);
  packet->data[1] = value & 0x7F;

  OSStatus status = MIDISend(mac_port_out, mac_endpoint_out, &packetList);
  if(status != noErr) {
    DEBUG(status);
  }
  drain_and_sync_output();
#endif
}

void TASCAR::midi_ctl_t::send_midi_pitchbend(int channel, int param, int value)
{
#if defined(__linux__)
  snd_seq_event_t ev;
  memset(&ev, 0, sizeof(ev));
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type = SND_SEQ_EVENT_PITCHBEND;
  ev.data.control.channel = (unsigned char)(channel);
  ev.data.control.param = (unsigned char)(param);
  ev.data.control.value = value;
  int err = snd_seq_event_output_direct(seq, &ev);
  if(err < 0) {
    DEBUG(err);
    DEBUG(strerror(-err));
  }
  drain_and_sync_output();
#elif defined(__APPLE__)
  if(mac_endpoint_out == 0)
    return;

  MIDIPacketList packetList;
  packetList.numPackets = 1;
  MIDIPacket* packet = &packetList.packet[0];

  packet->timeStamp = 0;
  packet->length = 3;
  packet->data[0] = 0xE0 | (channel & 0x0F);

  int midiValue = value + 8192;
  packet->data[1] = midiValue & 0x7F;
  packet->data[2] = (midiValue >> 7) & 0x7F;

  OSStatus status = MIDISend(mac_port_out, mac_endpoint_out, &packetList);
  if(status != noErr) {
    DEBUG(status);
  }
  drain_and_sync_output();
#endif
}

void TASCAR::midi_ctl_t::send_midi_note(int channel, int param, int value)
{
#if defined(__linux__)
  snd_seq_event_t ev;
  memset(&ev, 0, sizeof(ev));
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type = SND_SEQ_EVENT_NOTEON;
  ev.data.note.channel = (unsigned char)(channel);
  ev.data.note.note = (unsigned char)(param);
  ev.data.note.velocity = (unsigned char)(value);
  int err = snd_seq_event_output_direct(seq, &ev);
  if(err < 0) {
    DEBUG(err);
    DEBUG(strerror(-err));
  }
  drain_and_sync_output();
#elif defined(__APPLE__)
  if(mac_endpoint_out == 0)
    return;

  MIDIPacketList packetList;
  packetList.numPackets = 1;
  MIDIPacket* packet = &packetList.packet[0];

  packet->timeStamp = 0;
  packet->length = 3;
  packet->data[0] = 0x90 | (channel & 0x0F);
  packet->data[1] = param & 0x7F;
  packet->data[2] = value & 0x7F;

  OSStatus status = MIDISend(mac_port_out, mac_endpoint_out, &packetList);
  if(status != noErr) {
    DEBUG(status);
  }
  drain_and_sync_output();
#endif
}

void TASCAR::midi_ctl_t::connect_input(const std::string& src,
                                       bool warn_on_fail)
{
#if defined(__linux__)
  snd_seq_addr_t sender;
  memset(&sender, 0, sizeof(sender));
  if(snd_seq_parse_address(seq, &sender, src.c_str()) == 0)
    connect_input(sender.client, sender.port);
  else {
    if(warn_on_fail)
      TASCAR::add_warning("Invalid MIDI address " + src);
    else
      throw TASCAR::ErrMsg("Invalid MIDI address " + src);
  }
#elif defined(__APPLE__)
  ItemCount nSources = MIDIGetNumberOfSources();
  MIDIEndpointRef foundEndpoint = 0;

  for(ItemCount i = 0; i < nSources; ++i) {
    MIDIEndpointRef endpoint = MIDIGetSource(i);
    if(endpoint) {
      CFStringRef name = NULL;
      MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
      if(name) {
        char cName[256];
        CFStringGetCString(name, cName, sizeof(cName), kCFStringEncodingUTF8);
        CFRelease(name);
        if(src == cName) {
          foundEndpoint = endpoint;
          break;
        }
      }
    }
  }

  if(foundEndpoint != 0) {
    OSStatus status = MIDIPortConnectSource(mac_port_in, foundEndpoint, NULL);
    if(status != noErr) {
      if(warn_on_fail)
        TASCAR::add_warning("Failed to connect to MIDI source " + src);
      else
        throw TASCAR::ErrMsg("Failed to connect to MIDI source " + src);
    } else {
      mac_endpoint_in = foundEndpoint;
    }
  } else {
    if(warn_on_fail)
      TASCAR::add_warning("MIDI source not found: " + src);
    else
      throw TASCAR::ErrMsg("MIDI source not found: " + src);
  }
#endif
}

void TASCAR::midi_ctl_t::connect_output(const std::string& src,
                                        bool warn_on_fail)
{
#if defined(__linux__)
  snd_seq_addr_t sender;
  memset(&sender, 0, sizeof(sender));
  if(snd_seq_parse_address(seq, &sender, src.c_str()) == 0)
    connect_output(sender.client, sender.port);
  else {
    if(warn_on_fail)
      TASCAR::add_warning("Invalid MIDI address " + src);
    else
      throw TASCAR::ErrMsg("Invalid MIDI address " + src);
  }
#elif defined(__APPLE__)
  ItemCount nDestinations = MIDIGetNumberOfDestinations();
  MIDIEndpointRef foundEndpoint = 0;

  for(ItemCount i = 0; i < nDestinations; ++i) {
    MIDIEndpointRef endpoint = MIDIGetDestination(i);
    if(endpoint) {
      CFStringRef name = NULL;
      MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
      if(name) {
        char cName[256];
        CFStringGetCString(name, cName, sizeof(cName), kCFStringEncodingUTF8);
        CFRelease(name);
        if(src == cName) {
          foundEndpoint = endpoint;
          break;
        }
      }
    }
  }

  if(foundEndpoint != 0) {
    mac_endpoint_out = foundEndpoint;
  } else {
    if(warn_on_fail)
      TASCAR::add_warning("MIDI destination not found: " + src);
    else
      throw TASCAR::ErrMsg("MIDI destination not found: " + src);
  }
#endif
}

int TASCAR::midi_ctl_t::get_max_clients()
{
#if defined(__linux__)
  int clients = 0;
  snd_seq_system_info_t* info;
  int err = snd_seq_system_info_malloc(&info);
  if(err == 0) {
    int err = snd_seq_system_info(seq, info);
    if(err == 0)
      clients = snd_seq_system_info_get_clients(info);
    snd_seq_system_info_free(info);
  }
  return clients;
#elif defined(__APPLE__)
  return MIDIGetNumberOfSources() + MIDIGetNumberOfDestinations();
#endif
}

int TASCAR::midi_ctl_t::get_cur_clients()
{
#if defined(__linux__)
  int clients = 0;
  snd_seq_system_info_t* info;
  int err = snd_seq_system_info_malloc(&info);
  if(err == 0) {
    int err = snd_seq_system_info(seq, info);
    if(err == 0)
      clients = snd_seq_system_info_get_cur_clients(info);
    snd_seq_system_info_free(info);
  }
  return clients;
#elif defined(__APPLE__)
  return get_max_clients();
#endif
}

int TASCAR::midi_ctl_t::client_get_num_ports(int client)
{
#if defined(__linux__)
  int numports = 0;
  snd_seq_client_info_t* info;
  int err = 0;
  err = snd_seq_client_info_malloc(&info);
  if(err == 0) {
    err = snd_seq_get_any_client_info(seq, client, info);
    if(err == 0) {
      numports = snd_seq_client_info_get_num_ports(info);
    }
  }
  return numports;
#elif defined(__APPLE__)
  return 0;
#endif
}

std::vector<int> TASCAR::midi_ctl_t::get_client_ids()
{
  std::vector<int> clients;
#if defined(__linux__)
  snd_seq_client_info_t* info;
  int err = 0;
  err = snd_seq_client_info_malloc(&info);
  if(err == 0) {
    err = snd_seq_get_any_client_info(seq, 0, info);
    if(err == 0) {
      clients.push_back(0);
      while(snd_seq_query_next_client(seq, info) == 0) {
        clients.push_back(snd_seq_client_info_get_client(info));
      }
    }
  }
#elif defined(__APPLE__)
  for(int i = 0; i < MIDIGetNumberOfSources(); i++)
    clients.push_back(i);
  for(int i = 0; i < MIDIGetNumberOfDestinations(); i++)
    clients.push_back(i + 1000);
#endif
  return clients;
}

std::vector<int> TASCAR::midi_ctl_t::client_get_ports(int client,
                                                      unsigned int cap)
{
  std::vector<int> ports;
#if defined(__linux__)
  if(client_get_num_ports(client) > 0) {
    snd_seq_port_info_t* info;
    int err = 0;
    err = snd_seq_port_info_malloc(&info);
    if(err == 0) {
      err = snd_seq_get_any_port_info(seq, client, 0, info);
      if(err == 0) {
        unsigned int pcap = 0;
        pcap = snd_seq_port_info_get_capability(info);
        if((cap == 0) || (cap & pcap))
          ports.push_back(0);
        while(snd_seq_query_next_port(seq, info) == 0) {
          int port = snd_seq_port_info_get_port(info);
          pcap = snd_seq_port_info_get_capability(info);
          if((cap == 0) || (cap & pcap))
            ports.push_back(port);
        }
      }
    }
  }
#elif defined(__APPLE__)
  if(client < 1000) {
    if(cap == 0 || cap & SND_SEQ_PORT_CAP_READ)
      ports.push_back(0);
  } else {
    if(cap == 0 || cap & SND_SEQ_PORT_CAP_WRITE)
      ports.push_back(0);
  }
#endif
  return ports;
}

int TASCAR::midi_ctl_t::get_client_id()
{
#if defined(__linux__)
  int client = 0;
  snd_seq_client_info_t* info;
  int err = 0;
  err = snd_seq_client_info_malloc(&info);
  if(err == 0) {
    err = snd_seq_get_client_info(seq, info);
    if(err == 0) {
      client = snd_seq_client_info_get_client(info);
    }
  }
  return client;
#elif defined(__APPLE__)
  return 0;
#endif
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
