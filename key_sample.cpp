#include<math.h>
#include <stdio.h>
#include <Delay.h>
#include <stdlib.h>

#include "mono_sample.h"
#include "key_prototype.h"
#include "key_sample.h"

key_sample::key_sample(int note, float sample_rate) : key_prototype(note,sample_rate)
{
   speed=1.0;

   is_playing=false;
   interpolation_type=4;

   release_env=new simple_envelope();
   release_env->set_envelope(0.001f*sample_rate); 
   is_releasing=false;

   attack_env=new simple_envelope();
   attack_env->set_envelope(0.0001f*sample_rate); 
   is_attacking=false;

   fade_env=new simple_envelope();
   fade_env->set_envelope(0.001f*sample_rate); 
 
   has_a_sample=false;
}

void key_sample::add_sample(mono_sample *m, float s)
{
   printf("add_sample\n");
   printf("  pushing back sample\n");
   msample_vec.push_back(m);
   printf("  number of samples now: %d\n",msample_vec.size());
   
   printf("  setting this to current_sample\n");
   msample=msample_vec[msample_vec.size()-1];
   current_layer=msample_vec.size()-1;
   speed=s;
   has_a_sample=true;
   
   printf("  done with add sample\n");
}

void key_sample::add_sample(string filename, float s)
{
   msample_vec.push_back(new mono_sample(filename.c_str(),sample_rate));
   msample=msample_vec[msample_vec.size()-1];
   current_layer=msample_vec.size()-1;
   speed=s;
   has_a_sample=true;
}

void key_sample::set_current_layer(int which)
{
   if(has_a_sample==false) return;

   int list_size=msample_vec.size();
   if(which>=list_size)
     which=list_size-1;

   current_layer=which;
}

key_sample::~key_sample() //TODO - actually impliment
{
   //delete _msample;
}

void key_sample::key_on(float volume)
{ 
   //printf("passed in volume: %f\n",volume);
  
   if(is_playing==true)
   {
      was_playing=true;
      fade_env->reset();
      old_msample=msample;
   }   
   else
      was_playing=false;

   //printf("picking sample: %d\n",current_layer);
   msample=msample_vec[current_layer];

   old_position=position;
   old_volume_modifier=total_volume_modifier;


   vol=volume;
   position.reset();
   is_playing=true;
   
   is_releasing=false;
   release_env->reset();

   is_attacking=true;
   attack_env->reset();

   is_sustained_via_pedal=false;

}

void key_sample::key_off()
{
   is_attacking=false;
   is_releasing=true;
}

void key_sample::set_speed(float s)
{
   speed=s;
}

float key_sample::tick(float time_scale)
{
   float sample=get_sample();

   if(is_releasing)
      release_env->increment();
   if(is_attacking)
      attack_env->increment();

   float s=speed*time_scale;

   position.increment(s);

//   if(was_playing)
//      old_position.increment(s);

   if(msample->is_past_end(position))
      is_playing=false;  
 
   return sample;
}

float key_sample::get_sample()
{ 
   if(is_playing==false) return 0;

   float sample=msample->get_sample(position,interpolation_type);
   
   if(is_attacking)
   {  
      //printf("attacking\n");
      attack_volume_modifier=(1.0f-attack_env->get_value())*vol;
      total_volume_modifier=attack_volume_modifier; 
      //printf("  total volume modified: %f\n",total_volume_modifier);

      if(attack_env->get_state()==1)
      {
         //printf("attack complete: %f\n",total_volume_modifier);   
         is_attacking=false;        
      }
/*      if(was_playing) //deal with messy case where new key_on happens, while old sample still playing. fade out to prevent pop. 
      {
         //printf("was playing: add in previous sample\n");
         float s2=old_msample->get_sample(old_position,interpolation_type);
         float s2_vol=old_volume_modifier*fade_env->get_value(); 
         
         //sample+=s2*s2_vol;
 
         fade_env->increment();

         if(fade_env->get_state()==1)
             was_playing=false;
      }*/
   }
   else if(is_releasing)
   {  
      //printf("releasing\n");
      total_volume_modifier=attack_volume_modifier*release_env->get_value();
      //sample=sample*total_volume_modifier;
  
      if(release_env->get_state()==1)
      {
        //printf("release complete\n");
        is_playing=false;    
      }
   }
   
   sample=sample*total_volume_modifier;
   
   return sample*0.5;
}

void key_sample::sustain_pedal_pressed()
{
   is_sustained_via_pedal=true;
}

void key_sample::sustain_pedal_released() 
{
   if(is_sustained_via_pedal)
      key_off(); 
}
