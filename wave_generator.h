#ifndef DJZ_WAVE_GENERATOR
#define DJZ_WAVE_GENERATOR

#define TAU 6.283185307179586 //2*M_PI

class wave_generator //TODO support more waveforms (and also blend between square and sine)
{
public:
   wave_generator(float sample_rate_param);

   void set_pitch(float hz_param);
   void set_phase(float percent);

   void set_waveform(int waveform_param) { waveform=waveform_param; }
   void set_full() { is_full=true; }
   void set_pos()  { is_full=false; }

   void reset();
   float tick(float scale);

   int tick_count;
   float get_hz() {return hz; }

private:
   int waveform;
   float sample_rate;
   float hz;
   float inc_amount;
   float t;
   bool is_full;
};

#endif
