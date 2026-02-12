/**
 * @file   alsamidicc.h
 * @author Giso Grimm
 * @brief  MIDI CC message handling for controllers
 */
/*
 * License (GPL)
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
#ifndef ALSAMIDICC_H
#define ALSAMIDICC_H

#include "serviceclass.h"
#include <alsa/asoundlib.h>
#include <alsa/seq_event.h>
#include <string>
#include <vector>

namespace TASCAR {

  /**
     \brief Simple MIDI controller base class
  */
  class midi_ctl_t : public TASCAR::service_t {
  public:
    /**
       \param cname Client name
    */
    midi_ctl_t(const std::string& cname);
    virtual ~midi_ctl_t();
    /**
       \param client Source client
       \param port Source port
    */
    void connect_input(int client, int port);
    void connect_input(const std::string& name, bool warn_on_fail = false);
    /**
       \param client Destination client
       \param port Destination port
    */
    void connect_output(int client, int port);
    void connect_output(const std::string& name, bool warn_on_fail = false);
    /**
       \brief Send a CC event to output
       \param channel MIDI channel number
       \param param MIDI parameter
       \param value MIDI value
    */
    void send_midi(int channel, int param, int value);
    void send_midi_sysex(int len, char* data);
    void send_midi_channel_pressure(int channel, int param, int value);
    /**
       \brief Send a pitchbend event to output (14bit)
       \param channel MIDI channel number
       \param param MIDI parameter
       \param value MIDI value
    */
    void send_midi_pitchbend(int channel, int param, int value);
    /**
       \brief Send a NoteOn event to output
       \param channel MIDI channel number
       \param pitch Pitch of note
       \param velocity Note velocity
    */
    void send_midi_note(int channel, int pitch, int velocity);
    /**
       \brief Get current number of clients
    */
    int get_cur_clients();
    /**
       \brief Get maximum number of clients
    */
    int get_max_clients();
    /**
       \brief Get number of ports of client
    */
    int client_get_num_ports(int client);
    std::vector<int> client_get_ports(int client, unsigned int cap = 0);
    std::vector<int> get_client_ids();
    int get_client_id();
    void set_nonblock(bool nonblock);
    int32_t minwait = 0;

  protected:
    void service();
    /**
       \brief Callback to be called for incoming MIDI events
    */
    virtual void emit_event(int channel, int param, int value) = 0;
    virtual void emit_event_note(int, int, int) {};
    virtual void emit_event_mmc(uint8_t, uint8_t){};
    void drain_and_sync_output();

  private:
    // MIDI sequencer:
    snd_seq_t* seq;
    // input port:
    snd_seq_addr_t port_in;
    // output/feedback port:
    snd_seq_addr_t port_out;
  };
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
