#include "comb_filter.h"
#include <stdio.h>

comb_filter::comb_filter(float sample_rate, float max_delay_seconds)
{
   _sample_rate=sample_rate;
   _base_delay=800;
   _feedback=0.5;
   _effect_mix=1.0;

   _lfo_freq=0;
   _lfo_depth=0;


   _delay_line_max=max_delay_seconds*sample_rate;
   _delay_line=new float[_delay_line_max];
   for(int i=0;i<_delay_line_max;i++)
      _delay_line[i]=0.0f;

   printf("about to init delay: %d\n",_delay_line_max);
   _stk_d=new stk::Delay(_delay_line_max*0.5,_delay_line_max); //_delay_line_max);
   _wg=new wave_generator(_sample_rate);
}

void comb_filter::set_lfo_depth(float depth)
{
   _lfo_depth=depth*0.1;
   printf("%f\n",_lfo_depth); 
}
     
void comb_filter::set_lfo_freq(float freq)
{
   _lfo_freq=freq*1;
   printf("lfo freq will be: %f\n",_lfo_freq);
   _wg->set_speed(_lfo_freq);
}

void comb_filter::set_delay_time(float dt)
{
   _original_freq=dt;
   _base_delay=dt*_sample_rate;
   if(_base_delay<1) _base_delay=1;
//   if(_base_delay%2==1)
//      _base_delay++;

   _stk_d->setDelay(_base_delay); 
}

void comb_filter::set_feedback(float f)    
{
   _feedback=f;
}

void comb_filter::set_effect_mix(float em)
{
   _effect_mix=em;
} 

float comb_filter::tick(float input)
{
   float delay_val=_stk_d->nextOut();
   _stk_d->tick(input+delay_val*_feedback);
   float out=input+delay_val*(1.0-_effect_mix);

   if(_lfo_freq>0 && _lfo_depth>0) 
   {
      float w_val=_wg->tick();
      float new_delay=_base_delay+(w_val*2.0-1.0)*_base_delay*_lfo_depth;
      if(new_delay<1) new_delay=1;
      _stk_d->setDelay(new_delay); 
   }

   return out;
}


float comb_filter::tick_trails(float input)
{
   float delay_val=_stk_d->nextOut();
   _stk_d->tick(delay_val*_feedback);
   float out=input+delay_val*(1.0-_effect_mix);

   if(_lfo_freq>0 && _lfo_depth>0) 
   {
      float w_val=_wg->tick();
      float new_delay=_base_delay+(w_val*2.0-1.0)*_base_delay*_lfo_depth;
      if(new_delay<1) new_delay=1;
      _stk_d->setDelay(new_delay); 
   }

   return out;
}



