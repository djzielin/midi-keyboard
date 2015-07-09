#include<math.h>
#include <stdio.h>
#include <Delay.h>
#include <stdlib.h>

#include "mono_sample.h"
#include "key_prototype.h"
#include "key_additive.h"

key_additive::key_additive(int note, float sample_rate, int num_waves ) : key_prototype(note,sample_rate)
{
   is_playing=false;

   release_env=new simple_envelope();
   release_env->set_envelope(0.005f*sample_rate); 
   is_releasing=false;

   attack_env=new simple_envelope();
   attack_env->set_envelope(0.001f*sample_rate);  //testing latency so make fast for now

   is_attacking=false;

   fade_env=new simple_envelope();
   fade_env->set_envelope(0.001f*sample_rate); 
 
   for(int i=0;i<num_waves;i++)
   {
      wave_generator *w;
      w=new wave_generator(sample_rate);
      waves.push_back(w);
      
      w=new wave_generator(sample_rate);
      old_waves.push_back(w);
  
      vols.push_back(1.0f);
   }
}

void key_additive::configure_single_wave(int index, int waveform, float hz, float vol)
{
   wave_generator *wg=waves[index];
   wg->set_waveform(waveform);
   wg->set_pitch(hz);
   wg->set_phase((float)rand()/(float)RAND_MAX); //randomize phase to prevent weird effects. 
   
   //(*old_waves[index])=(*waves[index]); //make copy for old_waves. TODO: technically we should copy the position over, since we aren't resetting the phase each time? 

   vols[index]=vol;
}

key_additive::~key_additive() //TODO - actually impliment
{

}

void key_additive::key_on(float volume)
{ 
   if(is_playing==true)
   {
      was_playing=true;
      fade_env->reset();
   }   
   else
      was_playing=false;

   //waves.swap(old_waves);   

   //old_volume_modifier=total_volume_modifier;

   master_vol=volume;
   for(int i=0;i<waves.size();i++)
   {
      waves[i]->reset(); 
//      waves[i]->set_phase((float)rand()/(float)RAND_MAX); //randomize phase to prevent weird effects. 
   }
   is_playing=true;
   
   is_releasing=false;
   release_env->reset();

   is_attacking=true;
   attack_env->reset();

   is_sustained_via_pedal=false;
}

void key_additive::key_off()
{
   is_attacking=false;
   is_releasing=true;
}

float key_additive::tick(float time_scale)
{
   float sample=get_sample(time_scale);

   if(is_releasing)
      release_env->increment();
   if(is_attacking)
      attack_env->increment();

   return sample;
}

float key_additive::get_sample(float time_scale)
{ 
   if(is_playing==false) return 0;

   float sample=0.0f;

   for(int i=0;i<waves.size();i++)
   {
      sample+=waves[i]->tick(time_scale);
   }
   
   if(is_attacking)
   {  
      //printf("attacking\n");
      attack_volume_modifier=1.0f-attack_env->get_value();
      total_volume_modifier=attack_volume_modifier*master_vol; 
      sample=sample*total_volume_modifier;

      if(attack_env->get_state()==1)
      {
         //printf("attack complete: %f\n",_current_volume_modifier);   
         is_attacking=false;     
         //is_releasing=true;   
      }
      /*if(was_playing) //deal with messy case where new key_on happens, while old sample still playing. fade out to prevent pop. 
      {
         //printf("was playing: add in previous sample\n");
         float s2=0.0f;
         for(int i=0;i<waves.size();i++)
         {
            s2+=old_waves[i]->tick(time_scale);
         }
         
         float s2_vol=old_volume_modifier*fade_env->get_value(); 
         
         sample+=s2*s2_vol;
 
         fade_env->increment();

         if(fade_env->get_state()==1)
             was_playing=false;
      }*/
   }
   else if(is_releasing)
   {  
      float r_val=release_env->get_value();
      total_volume_modifier=attack_volume_modifier*r_val*master_vol;
      sample=sample*total_volume_modifier;

//      printf("releasing: vol %f, a: %f r: %f m: %f elapsed: %f out of %f\n",total_volume_modifier,attack_volume_modifier,r_val,master_vol,release_env->_elapsed_samples,release_env->_decay_time);
  
      if(release_env->get_state()==1)
      {
        is_playing=false;    
      }
   }
   else
   {
      total_volume_modifier=attack_volume_modifier*master_vol;
      sample=sample*total_volume_modifier;

   }
   
   return sample*1/waves.size(); //TODO  precalc this
}

void key_additive::sustain_pedal_pressed()
{
   is_sustained_via_pedal=true;
}

void key_additive::sustain_pedal_released() 
{
   if(is_sustained_via_pedal)
      key_off(); 
}
