#include "simple_envelope.h"
#include <stdio.h>

simple_envelope::simple_envelope()
{ 
   set_envelope(1000);
   reset();
}

void simple_envelope::reset()
{
   _elapsed_samples=0;
   _envelope_state=0;
}

float simple_envelope::get_value()
{
   float value=0;

   if(_envelope_state==0) value=(1.0f-_elapsed_samples/_decay_time);
   

   return value;
}

void simple_envelope::increment()
{
   _elapsed_samples++;
   if(_envelope_state==0 && _elapsed_samples>_decay_time) 
   {
      _envelope_state++;
   } 
}

void simple_envelope::set_envelope(float d)
{
   _decay_time=d;
}

