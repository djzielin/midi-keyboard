#include "wave_position.h"
#include <math.h>

wave_position::wave_position()
{
   reset();
}

wave_position::~wave_position()
{
   printf("we are in wave_position destructor\n");
}

void wave_position::reset()
{
   whole_part=0;
   fractional_part=0.0;
}

void wave_position::increment(float i)
{
   fractional_part+=i;
   unsigned int roll_over=(int)fractional_part;
   fractional_part-=(float)roll_over;
   whole_part+=roll_over;
}
