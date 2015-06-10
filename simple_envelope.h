#ifndef DJZ_SIMPLE_ENVELOPE
#define DJZ_SIMPLE_ENVELOPE


class simple_envelope
{
public:
   simple_envelope();
   void reset();
   float get_value();
   void increment();
   void set_envelope(float d);
   int get_state() { return _envelope_state; }
   float _elapsed_samples;
   float _decay_time; 

private:
   int   _envelope_state;
};

#endif
