#ifndef DJZ_KEY_ADDITIVE
#define DJZ_KEY_ADDITIVE

#include "mono_sample.h"
#include "simple_envelope.h"
#include <string>
#include "key_prototype.h"
#include "wave_generator.h"

using namespace std;

class key_additive : public key_prototype
{
public:
   key_additive(int note, float sample_rate, int num_waves);

   void configure_single_wave(int index, int waveform, float hz, float vol);

   ~key_additive();

   void key_off();
   void key_on(float volume);

   void set_speed(float s);
   float tick(float time_scale=1.0);
 
   void set_attack(float t)  { attack_env->set_envelope(sample_rate*t); }
   void set_release(float t) { release_env->set_envelope(sample_rate*t); fade_env->set_envelope(sample_rate*t); }

   void sustain_pedal_pressed();
   void sustain_pedal_released();


   vector <wave_generator *> waves;

private:
   float get_sample(float time_scale);


   vector <float> vols;

   vector <wave_generator *> old_waves;
   
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
};

#endif
