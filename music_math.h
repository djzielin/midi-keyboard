#ifndef DJZ_MUSIC_MATH
#define DJZ_MUSIC_MATH

#define MIDI_TO_HZ(midi_note) (440.0f*pow(2.0f,((float)midi_note-69.0f)/12.0f))
#define CALC_CENTS(cents)     (pow(2.0f,(float)cents/1200.0f))

#endif
