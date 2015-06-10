#ifndef DJZ_KEY_SAMPLE
#define DJZ_KEY_SAMPLE

#include "mono_sample.h"
#include "simple_envelope.h"
#include <string>
#include <OnePole.h>
#include "key_prototype.h"

//#include "wave_generator.h"

using namespace std;

class key_sample : public key_prototype
{
public:
   key_sample(float sample_rate);

   void add_sample(string filename, float s);
   void add_sample(mono_sample *m, float s);
   void set_current_layer(int which);

   ~key_sample();

   void key_off();
   void key_on(float volume);

   void set_speed(float s);
   float tick(float time_scale=1.0);
   mono_sample *get_current_msample() { return msample; }
   void patch_up();
   void patch_down();
   int get_num_layers() { return msample_vec.size(); }
   void set_attack(float t)  { attack_env->set_envelope(sample_rate*t); }
   void set_release(float t) { release_env->set_envelope(sample_rate*t); fade_env->set_envelope(sample_rate*t); }

   void sustain_pedal_pressed();
   void sustain_pedal_released();

private:
   float get_sample();

   mono_sample *msample;   
   vector <mono_sample *> msample_vec;
   
   bool has_a_sample;
   int current_layer; //rhodes has multiple layers (samples) per key
 
   float vol;
   float total_volume_modifier;

   float speed;
   wave_position position;

   mono_sample *old_msample; //to facilitate fade out of old note, while begining new note
   wave_position old_position;
   float old_volume_modifier;
   bool was_playing;

   int interpolation_type;

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
