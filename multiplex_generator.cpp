#include <math.h>
#include "multiplex_generator.h"
#include <stdio.h>

multiplex_generator::multiplex_generator(float sample_rate_param)
{
   sample_rate=sample_rate_param;
   reset();
   set_pitch(440.0f,1);
   current_harmonic=0;
}

void multiplex_generator::reset()
{
   t=0.0f;
}

void multiplex_generator::set_pitch(float hz_param, int harmonics)
{
   base_freq=hz_param;
   num_harmonics=harmonics;

   float partial_sum=0.0f;

   for(int i=0;i<num_harmonics;i++)
   {
      partial_sum+=1.0f/((float)i+1.0f);
      printf("partial sum: %f\n",partial_sum);
   }
   float actual_base=base_freq*partial_sum;
   //actual_base=actual_base*0.5; //drop by an octave for more bass!
   printf("using base of: %f\n",actual_base);

   inc_amount=new float[harmonics];
   
   for(int i=0;i<num_harmonics;i++)
   {
      inc_amount[i]=TAU*actual_base*(1.0f+(float)i)*1.0f/sample_rate;
   }

   while(t>TAU) t-=TAU;
}

void multiplex_generator::set_phase(float percent)
{
   t+=TAU*percent;
   while(t>TAU) t-=TAU;
}

float multiplex_generator::tick(float scale)
{
   float vol=1.0f/((float)(current_harmonic+1)); //if we want to account for decreased volume of upper harmonics
   float val=sin(t)*vol;

   t+=inc_amount[current_harmonic]*scale;  

   if(t>=(TAU))
   {
      current_harmonic=(current_harmonic+1)%num_harmonics;
   }

   while(t>TAU) t-=TAU;

   return val;
}

   
