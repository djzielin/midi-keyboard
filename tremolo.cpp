#include "tremolo.h"
#include <math.h>

tremolo::tremolo(float sample_rate)
{
   wg=new wave_generator(sample_rate);
   set_speed(4);
   set_depth(0);
}

//TODO features: multiwaveforms, switch between speeds, patterns vs rythm

float tremolo::tick(float s)
{
   float val=wg->tick();

   val=val*_depth+(1-_depth);

   return s*val;
}

float tremolo::tick2(float s)
{
   float val=wg->tick();

   val=(val*2.0-1.0)*_depth+1.0;
   return s*val;
}

void tremolo::set_speed(float s)
{
   wg->set_speed(s);
}

void tremolo::set_depth(float d)
{
   _depth=d;
}

