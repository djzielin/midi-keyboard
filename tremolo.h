#include "wave_generator.h"

class tremolo
{
public:
   tremolo(float sample_rate);
   float tick(float s); 
   float tick2(float s);
   void set_speed(float s);
   void set_depth(float d);

private:

   float _depth;
   wave_generator *wg;
};
