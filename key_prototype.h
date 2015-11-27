#ifndef DJZ_KEY_PROTOTYPE
#define DJZ_KEY_PROTOTYPE

#include<string>

class key_prototype
{
public:
   key_prototype(int note, float sample_r, std::string ktype) : midi_note(note), sample_rate(sample_r), is_sustained_via_pedal(false), in_pool(false), key_type(ktype) { }
   virtual float tick(float speed=1.0f) { }
   virtual void key_on(float velocity=1.0f) { }
   virtual void key_off() { }
   virtual void sustain_pedal_pressed() { }
   virtual void sustain_pedal_released() { }
   bool get_is_playing() { return is_playing; } 
   int midi_note;
   int in_pool;
   std::string key_type;

protected:
   float sample_rate;  
   bool is_sustained_via_pedal;
   bool is_playing;
};

#endif
