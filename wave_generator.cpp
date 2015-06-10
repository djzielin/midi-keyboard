#include <math.h>
#include "wave_generator.h"
#include <stdio.h>

float inc_precompute;
const float tau_precompute=2.0f/TAU;

wave_generator::wave_generator(float sample_rate_param)
{
   sample_rate=sample_rate_param;
   inc_precompute=TAU/sample_rate;
   reset();
   set_pitch(440.0f);
   set_waveform(0);
   set_full();
}

void wave_generator::reset()
{
   t=0.0f;
   tick_count=0;
}

void wave_generator::set_pitch(float hz_param)
{
   hz=hz_param;
 
   inc_amount=inc_precompute*hz; //TAU*1.0f/samplerate*hz
   //printf("inc amount: %f\n",inc_amount);
   while(t>TAU) t-=TAU;

}

void wave_generator::set_phase(float percent)
{
   t+=TAU*percent;
   while(t>TAU) t-=TAU;
}

float wave_generator::tick(float scale)
{
   float val;

   if(waveform==0)
   {
      val=sin(t);
   }   
   else if(waveform==1)
   { 
      if(t<M_PI) val=1.0f;
      else       val=-1.0f;
   }      
   else if(waveform==2)
   {
      //val=(t/TAU)*2.0-1.0;
      val=t*tau_precompute-1.0f;
   }

   //printf("hz: %f t: %f inc: %f scale: %f\n",hz,t,inc_amount,scale); 
   t+=inc_amount*scale;  
   while(t>TAU) t-=TAU;

   tick_count++;

   if(is_full) return val;
   else return (val+1.0f)*0.5f;
}

   
