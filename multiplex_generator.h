#ifndef DJZ_MULTIPLEX_GENERATOR
#define DJZ_MULTIPLEX_GENERATOR

#ifndef TAU
#define TAU 6.283185307179586 //2*M_PI
#endif

class multiplex_generator
{
public:
   multiplex_generator(float sample_rate_param);

   void set_pitch(float hz_param, int harmonics);
   void set_phase(float percent);

   void reset();
   float tick(float scale);

private:
   int num_harmonics; 
   int current_harmonic;
   float sample_rate;
   float base_freq;
   float *inc_amount;
   float t; 

};

#endif
