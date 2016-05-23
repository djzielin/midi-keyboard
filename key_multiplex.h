#ifndef DJZ_key_multiplex
#define DJZ_key_multiplex

#include "mono_sample.h"
#include "simple_envelope.h"
#include <string>
#include "key_prototype.h"
#include "multiplex_generator.h"

using namespace std;

class key_multiplex : public key_prototype
{
public:
   key_multiplex(int note, float sample_rate, int num_waves);

   void configure_single_wave(int index, int waveform, float hz, float vol, int num_harmonics);

   ~key_multiplex();

   void key_off();
   void key_on(float volume);

   void set_speed(float s);
   float tick(float time_scale=1.0);
 
   void set_attack(float t)  { attack_env->set_envelope(sample_rate*t); }
   void set_release(float t) { release_env->set_envelope(sample_rate*t); fade_env->set_envelope(sample_rate*t); }

   void sustain_pedal_pressed();
   void sustain_pedal_released();


   vector <multiplex_generator *> waves;
 
   void set_filter_val(float val) { filter_val=val; }

private:
   float get_sample(float time_scale);
   float filter_val;


   vector <float> vols;

   vector <multiplex_generator *> old_waves;
   
   float master_vol;
   float total_volume_modifier;

   float speed;

   float old_volume_modifier;
   bool was_playing;

   simple_envelope *_release_env;
   bool is_releasing;

   simple_envelope *_attack_env;
   bool is_attacking;
  
   float attack_volume_modifier;

   simple_envelope *release_env;
   simple_envelope *attack_env;
   simple_envelope *fade_env;

   bool is_filter_attacking;
   simple_envelope *attack_filter_env;
   simple_envelope *decay_filter_env;
   float peak_filter_val;

};

#endif
