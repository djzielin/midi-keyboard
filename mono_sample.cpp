#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <cstdio>
#include <cstring>
#include <sndfile.hh>
#include <samplerate.h>
#include "mono_sample.h"

//http://paulbourke.net/miscellaneous/interpolation/

float Interpolate_Nearest(float y1, float y2, float mu)
{
   if(mu>=0.5) return y2;
   else        return y1;
}

float Interpolate_Linear(float y1, float y2, float mu)
{
   return(y1*(1.0-mu)+y2*mu);
}

float Interpolate_Cubic(float y0, float y1, float y2, float y3, float mu)
{
   float a0,a1,a2,a3,mu2;

   mu2 = mu*mu;
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;

   return(a0*mu*mu2+a1*mu2+a2*mu+a3);
}

float Interpolate_Catmull_Rom(float y0,float y1,float y2,float y3,float mu)
{
   float a0,a1,a2,a3,mu2;

   mu2 = mu*mu;

   a0 = -0.5*y0 + 1.5*y1 - 1.5*y2 + 0.5*y3;
   a1 = y0 - 2.5*y1 + 2*y2 - 0.5*y3;
   a2 = -0.5*y0 + 0.5*y2;
   a3 = y1;

   return(a0*mu*mu2+a1*mu2+a2*mu+a3);
}

float compute_mu(float pos)
{
   int pos_as_int=(int)pos;
   return (pos-pos_as_int);
}

float mono_sample::clamp(int ipos)
{
   if(ipos<0)
      return _buffer[0];
   else if(ipos>=_frames)
      return _buffer[_frames-1];
   else
      return _buffer[ipos];
}
   
float mono_sample::get_sample(wave_position &pos, int interpolation_type)
{
   float sample=0;
   float mu=pos.fractional_part;
   unsigned long ipos=pos.whole_part;
   
   if(is_past_end(pos)) return 0;

   switch (interpolation_type)
   {
      case 0 : 
         sample=_buffer[ipos];
         break;
      case 1 :
         //mu=compute_mu(pos);
         sample=Interpolate_Nearest(_buffer[ipos], clamp(ipos+1), mu);
         break;
      case 2 :
         //mu=compute_mu(pos);
         sample=Interpolate_Linear(_buffer[ipos], clamp(ipos+1), mu);
         break;
      case 3 :
         //mu=compute_mu(pos);
         sample=Interpolate_Cubic(clamp(ipos-1), _buffer[ipos], clamp(ipos+1), clamp(ipos+2), mu);
         break; 
      case 4 :
         //mu=compute_mu(pos);
         sample=Interpolate_Catmull_Rom(clamp(ipos-1), _buffer[ipos], clamp(ipos+1), clamp(ipos+2), mu);
         break;
   }
     
   return sample;
}      

bool mono_sample::is_past_end(wave_position &position)
{
   return (position.whole_part>=(_frames-1)); 
} 

mono_sample::~mono_sample()
{
   printf("in mono_sample destructor\n");
   delete _buffer;
}

mono_sample::mono_sample(unsigned long size)
{
   _frames=size;
   _buffer=new float[_frames];
   
}

mono_sample::mono_sample (const char * fname, int system_sample_rate)
{	
   SndfileHandle file ;

   file = SndfileHandle (fname) ;
   _frames=file.frames();

   if(_frames==0) 
   {
      printf("error: 0 frames in file: %s\n",fname);
      exit(1);
   }

   bool is_stereo=(file.channels()==2);
   int file_sample_rate=file.samplerate(); //TODO use libsample rate to convert if sample rates of system is different from file
   _sample_rate=system_sample_rate;

   printf ("Opened file '%s'\n", fname) ;
   printf ("    Sample rate : %d\n", file.samplerate ()) ;
   printf ("    Channels    : %d\n", file.channels ()) ;
   printf ("    Frames    : %ld\n", file.frames ()) ;

   if(file.channels()>2) 
   {
      printf("Channels > 2 are not supported!\n");
      exit(1);
   }
  

   unsigned long number_of_samples=file.frames()*file.channels();
   float *full_buffer=new float[number_of_samples];
   _buffer=new float[file.frames()];

   file.read (full_buffer, number_of_samples);

   if(is_stereo)
      for(int i=0;i<_frames;i++) 
         _buffer[i]=full_buffer[i*2];
   else 
      for(int i=0;i<_frames;i++)
         _buffer[i]=full_buffer[i];
  
   if(file_sample_rate!=_sample_rate)
   {
      printf("sample rate of file (%d) does not match system: (%d)\n",file_sample_rate,_sample_rate);
      printf("we will use libsamplerate to convert\n");

      SRC_DATA sd;
      sd.data_in=_buffer;
      sd.input_frames=_frames;

      double src_ratio=(double)(_sample_rate)/(double)(file_sample_rate);
      double num_output_frames=(double)_frames*(double)src_ratio;
      float *new_buffer=new float[(long)num_output_frames];

      sd.output_frames=num_output_frames;
      sd.data_out=new_buffer;
      sd.src_ratio=src_ratio;
      
      int error_code=src_simple (&sd,0,1);

      if(error_code!=0) 
      {
         printf("error code: %d\n",error_code);
         printf("as string: %s\n", src_strerror(error_code));
         exit(1);
      }

      _frames=num_output_frames;
      delete _buffer;
      _buffer=new_buffer;         
   }	

   delete full_buffer;
}



