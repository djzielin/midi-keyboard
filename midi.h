
#ifndef DJZ_MIDI_H
#define DJZ_MIDI_H


#include <string>

bool is_there_another_midi_event_for_frame(int frame);
unsigned int *get_next_midi_event();

int setup_midi_jack(std::string midi_device_name, std::string second_choice);
int obtain_midi_events_jack(int nframes);

int setup_midi_alsa(std::string midi_device_name, std::string second_choice);
int obtain_midi_events_alsa(int nframes);

int get_total_events();

#endif
