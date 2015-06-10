#ifndef DJZ_COMB_FILTER
#define DJZ_COMB_FILTER

#include <Delay.h>
#include "wave_generator.h"

class comb_filter
{
public:
   comb_filter(float sample_rate, float max_delay_seconds);

   void set_lfo_depth(float depth);
   void set_lfo_freq(float freq);
   void set_delay_time(float dt);
   void set_feedback(float f);
   void set_effect_mix(float em);
   
   float tick(float input);
   float tick_trails(float input);
   void set_active(bool value) { _is_active=value; }

private:
   stk::Delay *_stk_d;
   wave_generator *_wg;

   bool _is_active;
   
   unsigned int _delay_line_max;
   float *_delay_line;

   float _lfo_freq;
   float _lfo_depth;
   float _original_freq;

   float _sample_rate;
   int   _base_delay;
   float _feedback;
   float _effect_mix;

 //  bool _has_lfo;
  
};


#endif
