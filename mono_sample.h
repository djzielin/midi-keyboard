#ifndef DJZ_MONO_SAMPLE
#define DJZ_MONO_SAMPLE

#include "wave_position.h"

class mono_sample
{
public:
   mono_sample(unsigned long size);
   mono_sample (const char * fname, int system_sample_rate);
   ~mono_sample();

   float get_sample(wave_position &pos, int interpolation_type);
   bool is_past_end(wave_position &position);

   int midi_note;
 
   unsigned long get_size() { return _frames; }
   void set_sample(unsigned long pos, float sample) { _buffer[pos]=sample; }
   float *get_buffer() { return _buffer; }

private:
   float clamp(int ipos);

   float *_buffer;
   unsigned long _frames; 
   int _sample_rate;

};

#endif
