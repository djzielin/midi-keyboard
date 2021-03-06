#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <sched.h>
#include <sndfile.hh>
#include <sys/time.h>

#include <Delay.h> //STK 
#include <NRev.h>
#include <OnePole.h>
#include <TwoZero.h>
#include <BiQuad.h>
#include <Chorus.h>

//#include "single_key.h"
//#include "comb_filter.h"
//#include "tremolo.h"
#include "key_sample.h"
#include "key_additive.h"
#include "key_multiplex.h"
#include "music_math.h"
#include "midi.h"
#include "audio.h"

#include <jack/jack.h>

#include <vector>
#include <list>

#ifdef RASPBERRY
  #include <wiringPi.h>
#endif

using namespace std;

//#define DO_PROFILE

bool is_guitar=false;

key_prototype **skv;
key_prototype *active_samples[128];

stk::Delay *stk_d;
stk::OnePole op;
stk::BiQuad op2;

bool high_pass_filter=false;

#ifdef RASPBERRY

//for the B+
//#define LIGHT_PIN 26

//for the 2
#define LIGHT_PIN 13

void setup_gpio()
{
   pinMode(LIGHT_PIN,OUTPUT);
}
#endif

stk::Chorus *ch;
stk::NRev *rev;

//comb_filter *cf;
//tremolo *t;
//tremolo *pt;

int highest_sample=0;
float master_vol=0.75;
float boost_vol=1.0;
float boost_post=1.0;
float lp_vol=0.0;
float pt_increment_factor=1.0;

//bool trem_active=false;
//bool p_trem_active=false;
bool delay_active=false;
bool dist_active=false;
bool chorus_active=false;
bool in_sustain_mode=false;

bool is_sample=false;

bool rhodes_layers=false;
float boost_offset=0.0;

float bend_amount=1.0;
float inc_amount=1.0;

int (*obtain_midi_events)(int);
int (*setup_midi)(std::string);

void (*setup_audio)(int port);
void (*audio_shutdown)();
void (*send_audio_to_card)(float *,int,bool);
void (*audio_loop)();

const float atan_scaler=2.0f/M_PI;

unsigned long sample_count=0;

float *recording_buffer;
float keyboard_slider_volume=1.0f;

bool skv_setup=false;

int current_instrument=0;

class key_set
{
public:
   key_prototype *keys[128];
   float low_pass;
   float volume;
   bool do_rhodes_layers;
  
   key_set() {  low_pass=0.7; volume=0.8; do_rhodes_layers=false; }

   void set_active()
   {
      for(int i=0;i<128;i++)
      {
          if(skv_setup)
          {
             key_prototype *k=skv[i];
             if(k!=NULL)
             {
                k->key_off();
                k->in_pool=false;
             }
        }
      }

      highest_sample=0;
      skv=keys;
      skv_setup=true;
      op.setPole(low_pass);
      master_vol=volume;
      rhodes_layers=do_rhodes_layers;      
   }
};

key_set *current_key_set;
vector<key_set *> all_instruments;


void map_midi_functions_for_jack()
{
   obtain_midi_events=&obtain_midi_events_jack;
   setup_midi=&setup_midi_jack;
}

void map_midi_functions_for_alsa()
{
   obtain_midi_events=&obtain_midi_events_alsa;
   setup_midi=&setup_midi_alsa;
}

void map_audio_functions_for_jack()
{
   setup_audio=&setup_audio_jack;
   audio_shutdown=&audio_shutdown_jack;
   send_audio_to_card=&send_audio_to_card_jack;
   audio_loop=&audio_loop_jack;
}

void map_audio_functions_for_alsa()
{
   setup_audio=&setup_audio_alsa;
   audio_shutdown=&audio_shutdown_alsa;
   send_audio_to_card=&send_audio_to_card_alsa;
   audio_loop=&audio_loop_alsa;
}


float generated_samples[4096]; //TODO don't hardcode maximum

extern float audio_sample_rate;

float total_time=0;
float total_timeM=0;
float total_timeS=0;
int times_ran=0;

float up_amount=pow(2.0f,(2.0f/12.0f));  //todo - precalc this
float down_amount=pow(2.0f,(-2.0f/12.0f));


#ifdef RASPBERRY

// enable flush to zero to eliminate denormals
// http://raspberrypisynthesizer.blogspot.com/2014_08_01_archive.html
static inline void enable_runfast(void)
{
//#ifdef RPI
    uint32_t fpscr;
    __asm__ __volatile__ ("vmrs %0,fpscr" : "=r" (fpscr));
    fpscr |= 0x03000000;
    __asm__ __volatile__ ("vmsr fpscr,%0" : :"ri" (fpscr));
//#endif
}

#endif

// Moog VCF, variation 2
// http://www.musicdsp.org/showArchiveComment.php?ArchiveID=26
// Type : 24db resonant lowpass
// References : CSound source code, Stilson/Smith CCRMA paper., Timo Tossavainen (?) version
//
// Notes :
//  in[x] and out[x] are member variables, init to 0.0 the controls:
//  fc = cutoff, nearly linear [0,1] -> [0, fs/2]
//  res = resonance [0, 4] -> [no resonance, self-oscillation]

float in1=0;
float in2=0;
float in3=0;
float in4=0;
float out1=0;
float out2=0;
float out3=0;
float out4=0;

float moog_fb;
float moog_coeff;
float moog_gain;


float update_moog_parameters(float fc, float res)
{
   float f = fc * 1.16;
   float fsq=f*f;
   
   moog_fb = 4.0*res * (1.0 - 0.15 * fsq); //DJZ - added 4.0 mod to let res input be [0,1]
   moog_coeff=1-f;
   moog_gain=fsq*fsq*0.35013;
}

float do_moog_filter(float input)
{

   input -= out4 * moog_fb; //feedback creates the resonance peak

   //input += 0.000001*((float)rand()/(float)RAND_MAX); //add some noise per ALSA modular synth source code
   input *= moog_gain;
   out1 = input + 0.3 * in1 + moog_coeff * out1; // Pole 1
   in1 = input;
   out2 = out1 + 0.3 * in2 + moog_coeff * out2; // Pole 2
   in2 = out1;
   out3 = out2 + 0.3 * in3 + moog_coeff * out3; // Pole 3
   in3 = out2;
   out4 = out3 + 0.3 * in4 + moog_coeff * out4; // Pole 4
   in4 = out3;
   return out4;
}

/*
float y1b,y2,y3,y4;
float oldx;
float oldy1,oldy2,oldy3;

    
float do_moog_filter(float in, float cutoff, float resonance)
{
    // empirical tuning
    float p = cutoff * (1.8f - 0.8f * cutoff);
    // k = p + p - T(1.0);
    // A much better tuning seems to be:
    float k = 2.0f * sin(cutoff * M_PI * 0.5f) - 1.0f;

    float t1 = (1.0f - p) * 1.386249f;
    float t2 = 12.0f + t1 * t1;
    float r = resonance * (t2 + 6.0f * t1) / (t2 - 6.0f * t1);
  

    // process input
    float x = in - r * y4;

    // four cascaded one-pole filters (bilinear transform)
    y1b =  x * p + oldx  * p - k * y1b;
    y2 = y1b * p + oldy1 * p - k * y2;
    y3 = y2 * p + oldy2 * p - k * y3;
    y4 = y3 * p + oldy3 * p - k * y4;

    // clipper band limited sigmoid
    y4 -= (y4 * y4 * y4) / 6.0f;

    oldx = x; oldy1 = y1b; oldy2 = y2; oldy3 = y3;

    return y4;
}
*/

float filter_freq=1.0;
float filter_res=0.5;

int note_elapsed=0;
int sw_note=70;
int prev_sw=0;

int prev_pitch_wheel=0;

bool skip_first_frames=true;
int old_guitar_val=0;

float compute_volume_factor(int note, int note_low, int note_hi, float val_low, float val_high)
{
   float x=note-note_low; //get 0--->?
   if(x<0) x=0;     
   x=x/(note_hi-note_low);       //get 0--->1
   if(x>1.0) x=1.0; 
   x=1.0f-x;        //get 1-->0
   x=x*(val_low-val_high);        //get 0.75-->0
   x+=val_high;         //get 1-->0.25
   return x;
}

unsigned int total_frames=0;

int old_rasp_value=-1;
float filter_val=1;

int generate_samples(jack_nframes_t nframes, void *arg)
{
//printf("asked to generate %d samples\n",nframes);

//  printf("we are asked to generate samples: %d\n",nframes);
#ifdef RASPBERRY
  enable_runfast();
#endif

#ifdef DO_PROFILE
   struct timeval ta;
   gettimeofday(&ta,NULL);
#endif

   int num_midi_events=obtain_midi_events(nframes);
   //if(num_midi_events!=0)
   //  printf("number of midi events: %d\n",num_midi_events);


#ifdef DO_PROFILE
   struct timeval taa;
   gettimeofday(&taa,NULL);
#endif


   for(int i = 0; i < nframes; i++)
   {  
/*     int sw_value=digitalRead(SWITCH_PIN);

     if(note_elapsed>0)
     {
        note_elapsed--;
        if(note_elapsed==0)
           skv[sw_note]->key_off(); 
     }
     if(note_elapsed==0)
     {
       if(sw_value==HIGH && prev_sw==LOW)
       {
          printf("starting sw note\n");
          
               key_prototype *sk;          
               sk=skv[sw_note];

               if(sk->in_pool==false)
               {
//                 printf("we weren't playing yet, so added to note list\n");
                  active_samples[highest_sample]=sk;
                  highest_sample++;
                  sk->in_pool=true;
               }
               sk->key_on(1.0);
          note_elapsed=5000;
 
       }
     } 
     prev_sw=sw_value;
*/
    if(skip_first_frames==false) //skip midi events in the first frame
      while(is_there_another_midi_event_for_frame(i))
      {
         unsigned int *midi_event=get_next_midi_event();

         unsigned char command=midi_event[0];

         if (command == 0x90 && midi_event[2]!=0) //midi note on received
         { 
            int note = midi_event[1];
            if(is_guitar) note-=12;

            int vol = midi_event[2]; 
            //vol=0;  
            float vol_f=vol/127.0;
 

           if(is_guitar)
           {
              float factor;
              if(note<36)
                  factor=compute_volume_factor(note,28,36,1.0,0.5);
              else
                 factor=compute_volume_factor(note,36,76,0.5,0.25);
              //printf("  factor: %f\n",factor);
              vol_f=0.75*factor;
           }


           float vol_sq=vol_f*vol_f;
 
            if(is_guitar)
            {
//               for(int e=0;e<127;e++)
  //               skv[e]->key_off();


            }

       //#ifdef DEBUG
            printf("keyboard.cpp -- got note on: %d %.02f %.02f time: %lu\n",note,vol_f,vol_sq,sample_count);
       //#endif
            
 
            
            {        
               key_prototype *sk;          
               sk=skv[note];

               if(sk->in_pool==false)
               {
//                 printf("we weren't playing yet, so added to note list\n");
                  active_samples[highest_sample]=sk;
                  highest_sample++;
                  sk->in_pool=true;
               }
//               else
//                  printf("already in the pool, no need to add\n");
            
               if(rhodes_layers==true) //TODO push this down to the single key class
               {
                  int layer;
           
                  if(vol>111) layer=0;
                  else if(vol>95) layer=1;
                  else if(vol>72) layer=2;
                  else if(vol>47) layer=3;
                  else layer=4;
                  //printf("  layer: %d\n");
                  ((key_sample *)sk)->set_current_layer(4-layer);
               }
               //vol_f=1.0;
               //printf("key on with volume: %f\n",vol_f);
               sk->key_on(vol_f);                        

               //sk=skv[note+5]; //code for harmony
               //if(sk->in_pool==false)
               //{
//                 printf("we weren't playing yet, so added to note list\n");
               //   active_samples[highest_sample]=sk;
               //   highest_sample++;
               //   sk->in_pool=true;
               //}
               //sk->key_on(vol_f);              
            }
     
         }        
         if (command == 0x80 || (command==0x90 && midi_event[2]==0)) //midi note off received
         {
          
            int note = midi_event[1];
            if(is_guitar) note-=12;

            int note_index=note;
             
  //          printf("keyboard.cpp -- got note off: %d at %d\n",note,sample_count);

            key_prototype *sk;
            sk=skv[note_index];

            if(in_sustain_mode==false)
               sk->key_off();
            else
               sk->sustain_pedal_pressed();

           //skv[note_index+5]->key_off();  //hmmm technically we only want to turn this off if  not used by actual note, hmmm
         }
         if(command ==0xB0) //midi cc message received 
         {
            unsigned char cc=  midi_event[1];
            unsigned char val= midi_event[2];
            float val_f=(float)val/127.0;
            float val_sq=val_f*val_f;
            float val_rsq=1.0-((1.0-val_f)*(1.0-val_f));
           
          //#ifdef DEBUG
             printf("cc: %d val: %d\n",cc,val);
          //#endif
            if(cc==1 && is_guitar) 
            {  //printf("is guitar controller\n");

               
               if(val>50 && old_guitar_val==0)
               { 
                  old_guitar_val=1;
                  printf("incrementing patch!\n");
                 current_instrument=(current_instrument+1)%all_instruments.size();
   
                   all_instruments[current_instrument]->set_active();
               }
               if(val<50)
                  old_guitar_val=0;

            }
            if(cc==7 && is_guitar==false) { keyboard_slider_volume=val_f; }
            if(cc==7 && is_guitar){
               filter_val=val_f;
               printf("setting filter value to: %f\n",filter_val);
            }
           // if(cc==22) { op.setPole(val_rsq); }
            //if(cc==13) { lp_vol=val_f; }
            if(cc==64)
            {
               if(val==127) in_sustain_mode=true;
               if(val==0) 
               {
                  //delay_active=false;

                  in_sustain_mode=false;
                  for(int q=0;q<highest_sample;q++) active_samples[q]->sustain_pedal_released();
               }    
            }         
            //if(cc==2)  for(int q=0;q<127;q++) skv[q]->set_attack(val_sq*3.0+0.001);
            //if(cc==14) for(int q=0;q<127;q++) skv[q]->set_release(val_sq*3.0+0.001);
            //if(cc==3)  t->set_depth(val_f);
            //if(cc==15) t->set_speed(val_f*10.0);
            //if(cc==4)  cf->set_effect_mix(1.0-val_f);
            //if(cc==16) cf->set_delay_time(0.5*val_f);
            //if(cc==5)  cf->set_feedback(val_f);
            //if(cc==6)  cf->set_lfo_depth(val_f);
            //if(cc==18) cf->set_lfo_freq(val_f);
            //if(cc==45) { if(val==127) play_pressed=true; else play_pressed=false; }
            //if(cc==46) { if(val==127) if(play_pressed) key_split=0; }
            //if(cc==9)  { boost_vol=1.0+val_f*50.0; }
            //if(cc==20) { boost_post=val_f; }

            //if(cc==8)  pt->set_depth(val_f*0.1);
            //if(cc==19) pt->set_speed(val_f*10.0);
            //if(cc==24) { if(val==127) trem_active=true; else trem_active=false; }
            //if(cc==28) { if(val==127) p_trem_active=true; else p_trem_active=false; }
            //if(cc==25) { if(val==127) delay_active=true; else delay_active=false; }
            //if(cc==29) { if(val==127) dist_active=true; else dist_active=false; }
            //if(cc==30) { if(val==127) chorus_active=true; else chorus_active=false; }
            //if(cc==12) { ch->setModDepth(val_f); }
            //if(cc==19) { ch->setModFrequency(val_f); }
         }
         if(command==0xE0)
         {
            //printf("OMG pitch bend\n");
            int v=(int)midi_event[1];
            //printf("pitch: %d %d\n",v,v2);
            //int b1 = in_event.buffer[1];
            //int b2 = in_event.buffer[2]; 
            //bend_amount=(float)((b2<<7)+b1)/16384.0*2.0;
             // printf("bend_amount: %f\n",bend_amount);
            //if(bend_amount<1) bend_amount=0.5+(bend_amount)*0.5;
            //printf("btotal: %f\n",bend_amount);
            //TODO: get this working for jack's midi data             
            
            int pitch_wheel=0;

            if(v>0)
            {
               float shift_normalized=(float)v/8191.0f; // 0 to 1
               //bend_amount=shift_normalized*(up_amount-1);   // 0 to (up_amount-1)
               //bend_amount++;                // 1 to up_amount
               if(shift_normalized>0.25)
               {
                  pitch_wheel=1.0;
               }
            }
               
            else
            {
               float shift_normalized=(float)v/(-8192.0f); // 0 to 1
               //bend_amount=shift_normalized*-1*(1-down_amount);
               //bend_amount++;
               if(shift_normalized>0.25)
               {
                  pitch_wheel=-1;
               }
            }
            
            if(abs(pitch_wheel)==1 && prev_pitch_wheel==0 && is_guitar==false)
            {
               printf("switching patches now\n");
               current_instrument+=pitch_wheel;
               if(current_instrument<0) current_instrument=all_instruments.size()-1;
               if(current_instrument>=all_instruments.size()) current_instrument=0;
   
               all_instruments[current_instrument]->set_active();

            }
            prev_pitch_wheel=pitch_wheel;

             //whammy bar for filter
/*            if(v<0)
            {
               float normed=1+v/8192.0f; //to [0,1]
           
               filter_freq=normed*normed;;
               if(filter_freq<0.01) filter_freq=0.01;
               if(filter_freq>1.0)  filter_freq=1.0;


               update_moog_parameters(filter_freq,filter_res);

               //printf("filter freq: %f\n",filter_freq);
            }
*/
         }
      }
 
      float sum=0;
      
      for (int q=0;q<highest_sample;q++)
      {     
         key_prototype *sk=active_samples[q];
         if(sk->key_type=="key_additive")
            ((key_additive *)sk)->set_filter_val(filter_val);
         sum+=sk->tick(bend_amount);   
      }
            
      //sum=rev->tick(sum); //run through reverb

      //if(trem_active) 
      //   sum=t->tick(sum);
      //if(dist_active)
      //   sum=atan(sum*boost_vol)*boost_post; //do the boost pre delay
      //if(chorus_active)
      //   sum=ch->tick(sum);
    // op.setPole(0.99);

      sum=op.tick(sum); //low pass filter

      //if(high_pass_filter==true) //high pass filter
     //    sum=op2.tick(sum);

         //   cf->set_effect_mix(0.25);
         //   cf->set_delay_time(0.5);
         //   cf->set_feedback(0.5);
     
// sum=do_moog_filter(sum);         
  
   //sum=cf->tick(sum);
      //if(delay_active)
      // sum=cf->tick(sum); //TODO redo this so there's only one tick function
      //else             sum=cf->tick_trails(sum);

      //if(p_trem_active)
      //   pt_increment_factor=pt->tick2(1.0);
      //else
      //   pt_increment_factor=1.0;
         
 
      generated_samples[i]=atan(sum*keyboard_slider_volume*1.0)*atan_scaler*master_vol; //TODO, batch atan_scaler up with master_vol changes
      //if(generated_samples[i]>master_vol)
      //  printf("ERROR: final: %f pre-tan: %f mastervol: %f\n",generated_samples[i],sum,master_vol);
      //if(do_record) recording_buffer[total_frames]=out[i]; //todo: stop recording if exceed buffer, otherwise we get segfault
      //total_frames++;
      //recording_buffer[sample_count]=generated_samples[i];
   
      sample_count++;
   }
   struct timeval taaa;
   gettimeofday(&taaa,NULL);

       
   send_audio_to_card(generated_samples,nframes,false);

   int real_index=0;
   //printf("samples active: %d\n",highest_sample);
   for (int q=0;q<highest_sample;q++) //remove any samples that aren't playing
   {      
      key_prototype *sd=active_samples[q];
      if(sd->get_is_playing()==true)
      { 
         //printf("  playing: %d tick count: %d\n",sd->midi_note,((key_additive *)sd)->waves[0]->tick_count);
         if(real_index!=q)
           active_samples[real_index]=sd;
         real_index++;
      }
     else
     {
       //printf("removing note from pool\n");
       sd->in_pool=false; //not playing so removing from pool

     }
   }

   //int diff=highest_sample-real_index;
   //if(diff>0) { printf("erased %d samples. %d %d\n",diff,highest_sample,real_index); }

   highest_sample=real_index;
  
#ifdef RASPBERRY
  //Below is for LED stuff

   int new_rasp_value=highest_sample>0;

   if(new_rasp_value!=old_rasp_value)
   {
      if(new_rasp_value)
         digitalWrite(LIGHT_PIN,HIGH);
      else
         digitalWrite(LIGHT_PIN,LOW);
   }

   old_rasp_value=new_rasp_value;
#endif
   //printf("samples: %d\n",highest_sample);

   //float val=2;
   //for(int i=0;i<100000;i++)
   //  val=val*val;

   //printf("%f\n",val);


   
#ifdef DO_PROFILE  
   struct timeval tb;
   gettimeofday(&tb,NULL);

  float  elapsedTime = (float)(taaa.tv_sec - taa.tv_sec) * 1000.0;      // sec to $
  elapsedTime += (float)(taaa.tv_usec - taa.tv_usec) / 1000.0;   // us to ms

  float  elapsedTimeS = (float)(tb.tv_sec - taaa.tv_sec) * 1000.0;      // sec to $
  elapsedTimeS += (float)(tb.tv_usec - taaa.tv_usec) / 1000.0;   // us to ms

  float  elapsedTimeM = (float)(taa.tv_sec - ta.tv_sec) * 1000.0;      // sec to $
  elapsedTimeM += (float)(taa.tv_usec - ta.tv_usec) / 1000.0;   // us to ms

  total_time+=elapsedTime;
  total_timeM+=elapsedTimeM;
  total_timeS+=elapsedTimeS;
  times_ran++;
  
  if(elapsedTime>2.0)
    printf("audio time too high: %f midievents: %d\n",elapsedTime,get_total_events());
  if(elapsedTimeS>2.0)
    printf("send time too high: %f\n",elapsedTimeS);
  if(elapsedTimeM>2.0)
    printf("midi time too high: %f\n",elapsedTimeM);

  if(times_ran==100)
  {
     printf("frames: %d audio_time: %f send_time: %f midi_time: %f\n",nframes,total_time/100.0,total_timeS/100.0,total_timeM/100.0);
     total_time=0;
     total_timeM=0;
     total_timeS=0;
     times_ran=0;
  }
#endif

   total_frames++;
   if(skip_first_frames==true)
   {
       if(total_frames>100)
          skip_first_frames=false;

   }  
    return 0;      
}


float calc_speed(int midi_note, int base)
{
   return pow(2.0,((midi_note-base)/12.0));
}

float calc_hz_from_midinote(int midi_note)
{
   return 440.0*pow(2.0,(midi_note-69.0)/12.0);
}

char prefix_names[30][4]={"G2","G#2","A2","A#2","B2","C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3","C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4","C5"};

char instrument_names[10][100]={"Cello","CombinedChoir","GC3Brass","M300A","M300B","MkIIBrass","MkIIFlute","MkIIViolins","StringSection","Woodwind2"};

char rhodes_names[10][100]=
{"rhodes_A0.wav",  
"rhodes_A1.wav", 
"rhodes_A2.wav",
"rhodes_A3.wav",
"rhodes_A4.wav", 
"rhodes_A5.wav",
"rhodes_EB1.wav",
"rhodes_EB2.wav",
"rhodes_EB3.wav",
"rhodes_EB4.wav"};

int rhodes_notes[10]={21,33,45,57,69,81,27,39,51,63};

bool does_file_exist(string filename)
{
   FILE *f=fopen(filename.c_str(),"r");

   if(f==NULL) return false;
  
   fclose(f);
   
   return true;
}


void init_mellotron()
{
   key_set *mellotron=new key_set();

   key_sample *sk;
   mono_sample *sample_array[30][10];
 
   char filename[1000]; 

   for(int i=7;i<8;i++) //0 to 10
   {
      for(int e=0;e<30;e++)
      {  
         sprintf(filename,"../ksamples/mellotron/%s/%s.wav",instrument_names[i],prefix_names[e]);
         sample_array[e][i]=new mono_sample(filename,audio_sample_rate);
      }
   }

   printf("now assigning samples to keys\n");
   for(int i=7;i<8;i++) //0 to 10
   {
       //if(i!=8) continue;

       for(int e=0;e<128;e++)
       {           
         sk=new key_sample(e,audio_sample_rate);

         if(e<43)
         {
 
            float speed=calc_speed(e,43);

            sk->add_sample(sample_array[0][i],speed);
         }
         else if(e<73)
         {   
            sk->add_sample(sample_array[e-43][i],1.0);
         }
         else
         {
            float speed=calc_speed(e,72);
            sk->add_sample(sample_array[29][i],speed);
         }
         mellotron->keys[e]=sk;
       }
   } 
   all_instruments.push_back(mellotron);
   current_instrument=all_instruments.size()-1;
   
   mellotron->low_pass=0.95;
   mellotron->volume=0.8;

   all_instruments[current_instrument]->set_active();

}


void init_rhodes2()
{
   key_set *rhodes=new key_set();

   key_sample *sk;

   int rhodes_notes[15]={29,35,40,45,50,55,59,62,65,71,76,81,86,91,96};
   char filename[1000];

   mono_sample *rhodes_array2[15][5];

   for(int i=0;i<15;i++)
   {
      for(int e=4;e>=0;e--)
      {
         sprintf(filename,"../ksamples/rhodes2/%d_%d.wav",rhodes_notes[i],e+1);     
         if(does_file_exist(filename))
         {
            rhodes_array2[i][e]=new mono_sample(filename,audio_sample_rate); 
            rhodes_array2[i][e]->midi_note=rhodes_notes[i];
         }
         else
         {
            printf("i: %d e: %d\n",i,e);
            rhodes_array2[i][e]=rhodes_array2[i][e+1]; //pick sample from lower layer
            printf("%d\n",rhodes_array2[i][e]->midi_note); //=rhodes_notes[i];
         }
      }
   }

   printf("done loading rhodes2 samples\n");
   printf("assigning rhodes2 notes\n");
  
   for(int i=0;i<128;i++)
   {
      key_sample *sk; 
      sk=new key_sample(i,audio_sample_rate);
      rhodes->keys[i]=sk;
   }

   for(int q=4;q>=0;q--)
   {  printf("q: %d\n",q);
      for(int e=0;e<128;e++)
      {
         int closest_note=0;
         int closest_sample=0;
         int closest_distance=1000;
   
         for(int i=0;i<15;i++)
         {
            int note_candidate=rhodes_array2[i][q]->midi_note;
           // printf("e: %d candidate: %d\n",e,note_candidate);
            int note_diff=abs(e-note_candidate);
            if(note_diff<closest_distance)
            {
               closest_distance=note_diff;
               closest_note=note_candidate;   
               closest_sample=i; 
            }
         }
         if(closest_distance==1000) printf("error: never found a note!\n"); 

         printf("trying to find a match for: %d. found: %d. distance: %d\n",e,closest_note,closest_distance);
         float speed=calc_speed(e,closest_note);
         printf("setting up note: %d %f\n",e,speed);
         printf("going to add sample\n"); fflush(stdout);
         sk=(key_sample *)rhodes->keys[e]; 
         sk->add_sample(rhodes_array2[closest_sample][q],speed);
         printf("really done with add sample\n");
      }
   }
   rhodes->do_rhodes_layers=true;


   all_instruments.push_back(rhodes);
   current_instrument=all_instruments.size()-1;
   
   rhodes->low_pass=0.8;
   rhodes->volume=0.8;

   all_instruments[current_instrument]->set_active();
}


void init_square_waves()
{
   key_set *square_waves=new key_set();

   key_additive *sk;

   for(int e=0;e<128;e++)
   {
      sk=new key_additive(e,audio_sample_rate,1);
      float pitch=MIDI_TO_HZ(e);
      //printf("midi note: %d pitch: %f\n",e,pitch);

      sk->configure_single_wave(0,1,pitch,1.0);
//      sk->configure_single_wave(1,0,MIDI_TO_HZ(e-12),1.0);

      square_waves->keys[e]=sk;
   }
   all_instruments.push_back(square_waves);
   current_instrument=all_instruments.size()-1;
   skv=square_waves->keys;

   square_waves->low_pass=0.8;
   square_waves->volume=0.3; 

   all_instruments[current_instrument]->set_active();

}


void init_multiplex_2()
{
   key_set *multiplex_waves=new key_set();

   key_multiplex *sk;

   for(int e=0;e<128;e++)
   {
      sk=new key_multiplex(e,audio_sample_rate,1);
      float pitch=MIDI_TO_HZ(e);
      //printf("midi note: %d pitch: %f\n",e,pitch);

      sk->configure_single_wave(0,1,pitch,1.0,2);
//      sk->configure_single_wave(1,0,MIDI_TO_HZ(e-12),1.0);

      multiplex_waves->keys[e]=sk;
   }
   all_instruments.push_back(multiplex_waves);
   current_instrument=all_instruments.size()-1;
   skv=multiplex_waves->keys;

   multiplex_waves->low_pass=0.8;
   multiplex_waves->volume=0.3; 

   all_instruments[current_instrument]->set_active();
}



void init_super_saw()
{
   key_set *super_saw=new key_set();

   key_additive *sk;

   for(int e=0;e<128;e++)
   {
      sk=new key_additive(e,audio_sample_rate,7);
      float pitch=MIDI_TO_HZ(e);
      
      //for estimate of JP8000 detune: 
      //http://www.kvraudio.com/forum/viewtopic.php?t=386339&start=15

      float detune=0.25;
      sk->configure_single_wave(0,2,pitch*CALC_CENTS(-191*detune),0.25);
      sk->configure_single_wave(1,2,pitch*CALC_CENTS(-109*detune),0.5);
      sk->configure_single_wave(2,2,pitch*CALC_CENTS( -37*detune),0.75);
      sk->configure_single_wave(3,2,pitch*CALC_CENTS(   0*detune),1.0);
      sk->configure_single_wave(4,2,pitch*CALC_CENTS(  31*detune),0.75);
      sk->configure_single_wave(5,2,pitch*CALC_CENTS( 107*detune),0.5);
      sk->configure_single_wave(6,2,pitch*CALC_CENTS( 181*detune),0.25);

      super_saw->keys[e]=sk;
   }
   all_instruments.push_back(super_saw);
   current_instrument=all_instruments.size()-1;
   
   super_saw->low_pass=0.8;
   super_saw->volume=0.7;

   all_instruments[current_instrument]->set_active();
}

#define MAX_WAVEFORMS 2000

void init_band_limited_square()
{
   key_set *band_square=new key_set();

   key_additive *sk;

   float pitches[MAX_WAVEFORMS];
   float amps[MAX_WAVEFORMS];

   for(int e=0;e<128;e++)
   {
      //https://en.wikipedia.org/wiki/Square_wave
      //frequency 2k-1*freq
      //amplitude 1/(2k-1)

      

      int number_of_sines=0;
      float base_pitch=MIDI_TO_HZ(e);

      for(int i=0;i<MAX_WAVEFORMS;i++)
      {
         pitches[i]=base_pitch*(float)(2*i+1); //1 3 5...
         amps[i]=1.0f/((float)i*2.0f+1.0f);

         if(pitches[i]<(float)audio_sample_rate/2.0)
            number_of_sines++;
      }

      sk=new key_additive(e,audio_sample_rate,number_of_sines);

      printf("key: %d waves: %d\n",e,number_of_sines);
      for(int i=0;i<number_of_sines;i++)
      {
         //printf("freq: %f amp: %f\n",pitches[i],amps[i]);
         sk->configure_single_wave(i,0,pitches[i],amps[i]);
      }

      band_square->keys[e]=sk;
   }
   all_instruments.push_back(band_square);
   current_instrument=all_instruments.size()-1;
   
   band_square->low_pass=0.8;
   band_square->volume=0.7;

   all_instruments[current_instrument]->set_active();
}

void init_band_limited_saw()
{
   key_set *band_square=new key_set();

   key_additive *sk;

   float pitches[MAX_WAVEFORMS];
   float amps[MAX_WAVEFORMS];

   for(int e=0;e<128;e++)
   {
      //https://en.wikipedia.org/wiki/Sawtooth_wave
      //frequency k*freq
      //amplitude 1/k

      

      int number_of_sines=0;
      float base_pitch=MIDI_TO_HZ(e);

      for(int i=0;i<MAX_WAVEFORMS;i++)
      {
         pitches[i]=base_pitch*(float)(i+1); //1 2 3 4 5...
         amps[i]=1.0f/((float)(i+1));
         //amps[i]=((float)MAX_WAVEFORMS-(float)i)/((float)MAX_WAVEFORMS);

         if(pitches[i]<(float)audio_sample_rate/2.0)
            number_of_sines++;
      }

      sk=new key_additive(e,audio_sample_rate,number_of_sines);

      printf("key: %d waves: %d\n",e,number_of_sines);
      for(int i=0;i<number_of_sines;i++)
      {
         //printf("freq: %f amp: %f\n",pitches[i],amps[i]);
         sk->configure_single_wave(i,0,pitches[i],amps[i]);
      }

      band_square->keys[e]=sk;
   }
   all_instruments.push_back(band_square);
   current_instrument=all_instruments.size()-1;
   
   band_square->low_pass=0.8;
   band_square->volume=0.7;

   all_instruments[current_instrument]->set_active();
}

void init_band_limited_pow2()
{
   key_set *band_square=new key_set();

   key_additive *sk;

   float pitches[MAX_WAVEFORMS];
   float amps[MAX_WAVEFORMS];

   for(int e=0;e<128;e++)
   {
      //https://en.wikipedia.org/wiki/Sawtooth_wave
      //frequency k*freq
      //amplitude 1/k

      

      int number_of_sines=0;
      float base_pitch=MIDI_TO_HZ(e);

      for(int i=0;i<MAX_WAVEFORMS;i++)
      {
         float harmonic=pow(2,i);
         pitches[i]=base_pitch*harmonic; //1 2 4...
         amps[i]=1.0f/harmonic;

         //amps[i]=((float)MAX_WAVEFORMS-(float)i)/((float)MAX_WAVEFORMS);

         if(pitches[i]<(float)audio_sample_rate/2.0)
            number_of_sines++;
      }

      sk=new key_additive(e,audio_sample_rate,number_of_sines);

      printf("key: %d waves: %d\n",e,number_of_sines);
      for(int i=0;i<number_of_sines;i++)
      {
         //printf("freq: %f amp: %f\n",pitches[i],amps[i]);
         sk->configure_single_wave(i,0,pitches[i],amps[i]);
      }

      band_square->keys[e]=sk;
   }
   all_instruments.push_back(band_square);
   current_instrument=all_instruments.size()-1;
   
   band_square->low_pass=0.8;
   band_square->volume=0.7;

   all_instruments[current_instrument]->set_active();
}

void init_band_limited_even()
{
   key_set *band_square=new key_set();

   key_additive *sk;

   float pitches[MAX_WAVEFORMS];
   float amps[MAX_WAVEFORMS];

   for(int e=0;e<128;e++)
   {
      //https://en.wikipedia.org/wiki/Sawtooth_wave
      //frequency k*freq
      //amplitude 1/k

      

      int number_of_sines=0;
      float base_pitch=MIDI_TO_HZ(e);

      for(int i=0;i<MAX_WAVEFORMS;i++)
      {
         float harmonic=i*2; //1 2 4 6 8 10 12 14
         if(i==0) harmonic=1.0f;

         pitches[i]=base_pitch*harmonic; //1 2 4...
         amps[i]=1.0f/harmonic;

         //amps[i]=((float)MAX_WAVEFORMS-(float)i)/((float)MAX_WAVEFORMS);

         if(pitches[i]<(float)audio_sample_rate/2.0)
            number_of_sines++;
      }

      sk=new key_additive(e,audio_sample_rate,number_of_sines);

      printf("key: %d waves: %d\n",e,number_of_sines);
      for(int i=0;i<number_of_sines;i++)
      {
         //printf("freq: %f amp: %f\n",pitches[i],amps[i]);
         sk->configure_single_wave(i,0,pitches[i],amps[i]);
      }

      band_square->keys[e]=sk;
   }
   all_instruments.push_back(band_square);
   current_instrument=all_instruments.size()-1;
   
   band_square->low_pass=0.8;
   band_square->volume=0.7;

   all_instruments[current_instrument]->set_active();
}



void init_organ()
{
   key_set *organ=new key_set();

   key_additive *sk;

   for(int e=0;e<128;e++)
   {
      sk=new key_additive(e,audio_sample_rate,2);

      sk->configure_single_wave(0,0,MIDI_TO_HZ(e),0.5);
      sk->configure_single_wave(1,0,MIDI_TO_HZ(e+12),0.25);

      organ->keys[e]=sk;
   }
   all_instruments.push_back(organ);
   current_instrument=all_instruments.size()-1;

   organ->low_pass=0.7;
   organ->volume=0.6;

   all_instruments[current_instrument]->set_active();

}


void init_detuned()
{
   key_set *organ=new key_set();

   key_additive *sk;

   for(int e=0;e<128;e++)
   {
      sk=new key_additive(e,audio_sample_rate,3);

      sk->configure_single_wave(0,0,MIDI_TO_HZ(e-12),0.5);
      sk->configure_single_wave(1,2,MIDI_TO_HZ(e),0.5);
      sk->configure_single_wave(2,2,MIDI_TO_HZ(e+12)*CALC_CENTS(10),0.25);

      organ->keys[e]=sk;
   }
   all_instruments.push_back(organ);
   current_instrument=all_instruments.size()-1;

   organ->low_pass=0.7;
   organ->volume=0.6;

   all_instruments[current_instrument]->set_active();

}

void init_sounds()
{
   if(is_guitar==false)
   {
      init_rhodes2();
      init_square_waves();
      init_organ();
   }
   else
   {
      init_multiplex_2();
      init_square_waves();
      init_organ();
   }
   
   //init_detuned();
   //init_mellotron();
   //init_super_saw();
   //init_band_limited_square();
   //init_band_limited_even();
   //init_band_limited_saw();
   //init_band_limited_pow2();

   int index=0;

   //if(kit_name=="rhodes")     index=0;
   //if(kit_name=="mellotron")  index=1;
   //if(kit_name=="square")     index=2;
   //if(kit_name=="supersaw")   index=3;
   //if(kit_name=="organ")      index=4;

   current_instrument=index;
   all_instruments[current_instrument]->set_active();
  
   //rev->setEffectMix(0.25);
   //t=new tremolo(audio_sample_rate);
   //pt=new tremolo(audio_sample_rate);
   //ch=new stk::Chorus(1000);

   printf("done with initsounds\n");
}

void dump_recording()
{
   SNDFILE              *outfile ;
   SF_INFO              sfinfo ;

   sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24   ;
   sfinfo.channels = 1;
   sfinfo.samplerate=audio_sample_rate;

   char outfilename[]="test.wav";

   printf("opening file for output\n");

   if (! (outfile = sf_open (outfilename, SFM_WRITE, &sfinfo)))
   {
      printf ("Error : could not open file : %s\n", outfilename) ;
      puts (sf_strerror (NULL)) ;
      exit (1) ;
   }

   printf("   writing data\n");
   sf_write_float (outfile, recording_buffer, sample_count) ;

   printf("   done writing\n");
   sf_close (outfile) ;
}

static void signal_handler(int sig)
{
   
   fprintf(stderr, "signal received, exiting ...\n");
   audio_shutdown();
   //dump_recording();
   exit(0);
}

int main(int argc, char **argv)
{
#ifdef RASPBERRY
   wiringPiSetupGpio();
   setup_gpio();
#endif

   srand ( time(NULL) );
   update_moog_parameters(filter_freq,filter_res);

#ifdef USE_ALSA
   map_midi_functions_for_alsa();
   map_audio_functions_for_alsa();
#else
   map_midi_functions_for_jack();
   map_audio_functions_for_jack();
#endif


   if(argc>1 && strcmp("Guitar",argv[1])==0)
   {
      printf("We are using guitar!\n");
      is_guitar=true; //now we can setup the sounds available based on whats plugged in!
   }

   init_sounds();

        
   signal(SIGQUIT, signal_handler);
   signal(SIGTERM, signal_handler);
   signal(SIGHUP, signal_handler);
   signal(SIGINT, signal_handler);

   setup_audio(0);   

   int instrument=0;
   if(argc>1)
      instrument=setup_midi(argv[1]);

   if(instrument==1)
   {
      is_guitar=true;
      printf("we are using guitar!\n");

      current_instrument=4;
      all_instruments[current_instrument]->set_active();

   }

   audio_loop();
}

