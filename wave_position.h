#ifndef DJZ_WAVE_POSITION
#define DJZ_WAVE_POSITION

#include <stdio.h>

class wave_position
{
public:
   wave_position();
   ~wave_position();

   void reset();
   void increment(float i);

   wave_position & operator= (const wave_position & wp2)
   {
        //printf("in assignment operator\n");

        whole_part=wp2.whole_part;
        fractional_part=wp2.fractional_part;
        
        return *this;
    }

   unsigned int whole_part;
   float fractional_part;
};

#endif
