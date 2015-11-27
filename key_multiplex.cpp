#include<math.h>
#include <stdio.h>
#include <Delay.h>
#include <stdlib.h>

#include "mono_sample.h"
#include "key_prototype.h"
#include "key_multiplex.h"

key_multiplex::key_multiplex(int note, float sample_rate, int num_waves ) : key_prototype(note,sample_rate,"key_multiplex")
{
   is_playing=false;

   release_env=new simple_envelope();
   release_env->set_envelope(0.005f*sample_rate); 
   is_releasing=false;

   attack_env=new simple_envelope();
   attack_env->set_envelope(0.001f*sample_rate);  //testing latency so make fast for now
   //attack_env->set_envelope(0.100*sample_rate);  //testing latency so make fast for now

   attack_filter_env=new simple_envelope();
   attack_filter_env->set_envelope(0.1f*sample_rate); 

   decay_filter_env=new simple_envelope();
   decay_filter_env->set_envelope(0.1f*sample_rate); 

   is_filter_attacking=false;
   is_attacking=false;

   //fade_env=new simple_envelope();
   //fade_env->set_envelope(0.001f*sample_rate); 
 
   for(int i=0;i<num_waves;i++)
   {
      multiplex_generator *w;
      w=new multiplex_generator(sample_rate);
      waves.push_back(w);
      
      w=new multiplex_generator(sample_rate);
      old_waves.push_back(w);
  
      vols.push_back(1.0f);
   }

   filter_val=1;
}

void key_multiplex::configure_single_wave(int index, int waveform, float hz, float vol, int num_harmonics)
{
   multiplex_generator *wg=waves[index];
   //wg->set_waveform(waveform);
   wg->set_pitch(hz,  num_harmonics);
   wg->set_phase((float)rand()/(float)RAND_MAX); //randomize phase to prevent weird effects. 
   
   //(*old_waves[index])=(*waves[index]); //make copy for old_waves. TODO: technically we should copy the position over, since we aren't resetting the phase each time? 

   vols[index]=vol;
}

key_multiplex::~key_multiplex() //TODO - actually impliment
{

}

void key_multiplex::key_on(float volume)
{ 
   if(is_playing==true)
   {
      was_playing=true;
      //fade_env->reset();
   }   
   else
      was_playing=false;

   //waves.swap(old_waves);   

   //old_volume_modifier=total_volume_modifier;

   master_vol=volume;
   for(int i=0;i<waves.size();i++)
   {
      waves[i]->reset(); 
      waves[i]->set_phase((float)rand()/(float)RAND_MAX); //randomize phase to prevent weird effects. 
   }
   is_playing=true;
   
   is_releasing=false;
   release_env->reset();

   is_attacking=true;
   is_filter_attacking=true;
   attack_env->reset();
   attack_filter_env->reset();
   decay_filter_env->reset();

   filter_val=0.0;

   is_sustained_via_pedal=false;
}

void key_multiplex::key_off()
{
   is_attacking=false;
   is_releasing=true;
}

float key_multiplex::tick(float time_scale)
{
   float sample=get_sample(time_scale);

   if(is_releasing)
      release_env->increment();
   if(is_attacking)
   {
      attack_env->increment();
   }
   if(is_filter_attacking)
     attack_filter_env->increment();
   else
     decay_filter_env->increment();

   return sample;
}

//int prev_harmonic=0;
//float prev_remainder=0;
//float prev_total_harmonic=0;

float key_multiplex::get_sample(float time_scale)
{ 
   if(is_playing==false) return 0;

   float sample=0.0f;
 /*  if(is_filter_attacking) //for filter envelope experiments
   {
      filter_val=(1.0f-attack_filter_env->get_value())*5.0f;
      if(attack_filter_env->get_state()==1)
      {
         is_filter_attacking=false;
         peak_filter_val=filter_val;
         printf("filter done attacking at peak: %f\n",filter_val);
      }
      //printf("filter: %f\n",filter_val);
   }
   else
   {
      filter_val=peak_filter_val;//*(decay_filter_env->get_value());
   }*/

 //  float harmonic_content=filter_val*50.0f;
 //  int number_of_waves=floor(harmonic_content);
 //  float remainder=harmonic_content-(float)number_of_waves;

 

   for(int i=0;i<waves.size();i++)
   {
      //float v;
      //if(filter_env->get_state()==0)
      //  v=1.0f-filter_env->get_value();//*((float)i/(float)waves.size());
      //else
      //  v=1.0f-(1.0f-filter_env2->get_value());//*((float)i/(float)waves.size());
      //if(i==0) v=1.0f;
      float v=waves[i]->tick(time_scale)*vols[i];
     // if(i!=number_of_waves-1)
         sample+=v;
    //  else
    //     sample+=v*2.0; //resonance
   }
  
      //printf("remainder: %f\n",remainder);
   /*   int next_harmonic=number_of_waves;
      if(next_harmonic<waves.size())
       sample+=waves[next_harmonic]->tick(time_scale)*vols[next_harmonic]*remainder;
   
   
   if(prev_total_harmonic!=harmonic_content)
   {
      for(int i=0;i<number_of_waves && i<waves.size();i++)
          printf("%.02f ",waves[i]->get_hz());
      printf("%.02ff \n",waves[next_harmonic]->get_hz());
   }

   if(prev_total_harmonic!=harmonic_content)
   {
      for(int i=0;i<number_of_waves && i<waves.size();i++)
          printf("%.02f ",vols[i]);
      printf("%.02ff \n",vols[next_harmonic]*remainder);
   }
   prev_total_harmonic=harmonic_content;
*/
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
   
   return sample; //*1/waves.size(); //TODO  precalc this
}

void key_multiplex::sustain_pedal_pressed()
{
   is_sustained_via_pedal=true;
}

void key_multiplex::sustain_pedal_released() 
{
   if(is_sustained_via_pedal)
      key_off(); 
}
