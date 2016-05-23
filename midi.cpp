#include <jack/jack.h>
#include <jack/midiport.h>

#include "asoundlib.h"

#include "midi.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>

#include <iostream>
#include <string>

using namespace std;

#define MIDI_DATA_SIZE 5

unsigned int *midi_data; //TODO: maybe don't hardcode this? or at the very least check for overruns?
                               //      also, we could consider using the datatype "unsigned short", since our time stamp is not likely over 65k

int current_midi_event=0;
int total_midi_events=0;


//jack variables
jack_port_t *input_port;
bool midi_setup=false;

extern jack_client_t *jack_client;

int get_total_events()
{
   return total_midi_events;
}

unsigned int *get_next_midi_event()
{
   //TODO: do we need a sanity check that current_midi_event isn't out of range? or would that be a waste?
 
   unsigned int *data=&midi_data[current_midi_event*MIDI_DATA_SIZE+2]; //the +2 makes us skip the time and port field. 
   current_midi_event++; 
 
   return data;
}

bool is_there_another_midi_event_for_frame(int frame)
{
   if(current_midi_event>=total_midi_events)
      return false;

   if(midi_data[current_midi_event*MIDI_DATA_SIZE]==frame)
     return true;

   return false;
}

int obtain_midi_events_jack(int nframes)
{
   //printf("obtain midi events!\n");

   if(midi_setup==false)
   {
      printf("midi is not yet setup\n");
      return 0;
   }

   jack_midi_event_t in_event;

   void* port_buf = jack_port_get_buffer(input_port, nframes);
   total_midi_events = jack_midi_get_event_count(port_buf);
   //printf("total midi events: %d\n",total_midi_events);

   for(int i=0;i<total_midi_events;i++)
   {
      //printf("we have %d midi events!\n",total_midi_events);
      jack_midi_event_get(&in_event, port_buf, i);

      int data_base=i*MIDI_DATA_SIZE;

      midi_data[data_base+0]=in_event.time;
      midi_data[data_base+1]=in_event.buffer[0] & 0x0f; //channel. TODO: also, maybe we don't need to store this?
      midi_data[data_base+2]=in_event.buffer[0] & 0xf0; //command 

      if(midi_data[data_base+2]==0xE0) //get jack to match our alsa values
      {
          float val=in_event.buffer[2]-64;
          if(val>0)
             val=val/63.0f*8191.0f;
          else
             val=val/64.0*8192.0f;

          midi_data[data_base+3]=val;
          midi_data[data_base+4]=0;
      }
      else
      {
         midi_data[data_base+3]=in_event.buffer[1];
         midi_data[data_base+4]=in_event.buffer[2];
      }
   }

   current_midi_event=0;
   return total_midi_events;
}

int setup_midi_jack(string midi_device_name) //TODO - what about when we need to get data from multiple devices at the same time?
{
   int ret_val=0;
   printf("trying to setup jack midi\n");

   input_port = jack_port_register (jack_client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
   
   midi_data=(unsigned int *)malloc(10000);
   const char **ports;
   ports = jack_get_ports (jack_client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);

   printf("trying to find midi device: %s: \n",midi_device_name.c_str());
   int i;
   int times_seen_keystation=0;
   
   for(i=0;;i++) 
   {
      if(ports[i]==NULL) break;
      printf("  ports: %s\n",ports[i]);
      
      string s1=ports[i];
      bool is_first=(s1.find(midi_device_name)!=string::npos);
 
      if (is_first)
      {    
          

        std::cout << "  found our desired device: " << midi_device_name << std::endl;
               
        int jack_result=jack_connect (jack_client, ports[i], jack_port_name (input_port));
        if(jack_result==0)
        {
           printf("successfully connected to %s\n",s1.c_str());
           midi_setup=true;
           ret_val=0;
           break;
        }         
      }
   }
   if(midi_setup==false)
     exit(1);

   printf("   Done setting up jack midi\n");
   return ret_val;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA 
///////////////////////////////////////////////////////////////////////////////

pthread_t alsa_thread;
snd_seq_t *alsa_handle;
int alsa_client;
int alsa_queue;
snd_seq_queue_status_t *alsa_status;

int max_clients;
int max_ports;
int max_queues;

unsigned int alsa_data_buffer[2][10000];
int alsa_events[2]={0,0};
int buffer_num=0;

pthread_mutex_t alsa_midi_lock;

int recent_seconds=0;
int recent_nseconds=0;

#define TIME_CALC_FLOAT

int obtain_midi_events_alsa(int nframes)
{
   //printf("in obtain_midi_events_alsa\n");
   if(midi_setup==false) return 0;

  

   int bn=buffer_num;
   int err;
   int prev_seconds,prev_nseconds;

   //printf("getting mutex\n");   

   prev_seconds=recent_seconds;
   prev_nseconds=recent_nseconds;

   struct timeval ta;
   gettimeofday(&ta,NULL);

   pthread_mutex_lock(&alsa_midi_lock);

   struct timeval tb;
   gettimeofday(&tb,NULL);

      buffer_num=(buffer_num+1)%2; //rotate buffers. future events go into the other buffer
      alsa_events[buffer_num]=0;

    // printf("getting queue time\n");
      if ((err = snd_seq_get_queue_status(alsa_handle, alsa_queue, alsa_status))<0) {
			if (err == -ENOENT)
                        {
                           printf("queue status returned ENOENT\n");
                           return 0;
                        }
			fprintf(stderr, "queue %i info error: %s\n", alsa_queue, snd_strerror(err));
			exit(0);
   	}
      //printf("got back from queue time check\n"); fflush(stdout);

      recent_seconds=snd_seq_queue_status_get_real_time(alsa_status)->tv_sec;
      recent_nseconds=snd_seq_queue_status_get_real_time(alsa_status)->tv_nsec;
   pthread_mutex_unlock(&alsa_midi_lock);
   //printf("mutex released\n");
   unsigned int tmax;

  float  elapsedTime = (float)(tb.tv_sec - ta.tv_sec) * 1000.0;      // sec to$
  elapsedTime += (float)(tb.tv_usec - ta.tv_usec) / 1000.0;   // us to ms
  
  if(elapsedTime>1.0)
   printf("lock BLEW UP!\n");



   current_midi_event=0;
   total_midi_events=alsa_events[bn];

   if(total_midi_events==0) return 0;

   tmax=(recent_seconds-prev_seconds)*1e6+(recent_nseconds-prev_nseconds)/1e3;

#ifdef TIME_CALC_FLOAT
   //double factor=1.0/((double)tmax/(double)nframes);
   float factor=(float)nframes/(float)tmax;
#else
   unsigned int time_per_frame=tmax/nframes;
#endif

//   if(alsa_events[bn]>0) printf("midi events: %d\n",alsa_events[bn]); 
   for(int i=0;i<alsa_events[bn];i++) //get time stamps into frame numbers
   {
      int data_base=MIDI_DATA_SIZE*i;

#ifdef TIME_CALC_FLOAT
      float t=alsa_data_buffer[bn][data_base];
      unsigned int result_int=(unsigned int)(t*factor);
#else      
      unsigned int t=alsa_data_buffer[bn][data_base];
      unsigned int result_int=t/time_per_frame;
#endif
      
      if(result_int>(nframes-1)) //TODO may not need these sanity checks anymore
      { 
         printf("obtain event. over our range. raw: %d max :%d calcd: %d\n",alsa_data_buffer[bn][data_base],tmax,result_int);
         result_int=nframes-1;
      }
      if(result_int<0)
      { 
         printf("obtain event. under our range, clamping\n");
         result_int=0;
      }
      alsa_data_buffer[bn][data_base]=result_int;
      

//      int command=alsa_data_buffer[bn][data_base+2];
//      if(command==0x90)
//         printf("obtain event got note ON: %d\n",alsa_data_buffer[bn][data_base+3]);
//      if(command==0x80)
//         printf("obtain event got note OFF: %d\n",alsa_data_buffer[bn][data_base+3]);

//      printf("obtain event %d -- time: %d command: %X note: %d\n",i,alsa_data_buffer[bn][data_base],alsa_data_buffer[bn][data_base+2],alsa_data_buffer[bn][data_base+3]);
   }   

   midi_data=alsa_data_buffer[bn];
   //memcpy(midi_data,alsa_data_buffer[bn],total_midi_events*MIDI_DATA_SIZE*sizeof(unsigned int));
   return total_midi_events;
}


int decode_event_simple(snd_seq_event_t * ev)
{
	char space[] = "         ";

	/* decode the actual event data... */
	int e=(ev->type);
   int command=0;

   if(e==SND_SEQ_EVENT_NOTEON) 
   {
      command=0x90;
      //printf("decoder got note ON: %d\n",ev->data.note.note);
   }
   if(e==SND_SEQ_EVENT_NOTEOFF)
   {
      command=0x80;
      //printf("decoder got note OFF: %d\n",ev->data.note.note);

   }
   if(e==SND_SEQ_EVENT_PITCHBEND)
   {
      command=0xE0;
   }

   if(e==SND_SEQ_EVENT_NOTEON || e==SND_SEQ_EVENT_NOTEOFF || e==SND_SEQ_EVENT_PITCHBEND ||e==SND_SEQ_EVENT_CONTROLLER)
   {
      int sec=ev->time.time.tv_sec;
      int nsec=ev->time.time.tv_nsec;

      int tdiff;

      
      if(e==SND_SEQ_EVENT_NOTEON || e==SND_SEQ_EVENT_NOTEOFF)
      {
         
         pthread_mutex_lock(&alsa_midi_lock);

             
            tdiff=(sec-recent_seconds)*1e6+(nsec-recent_nseconds)/1e3;
            //printf("tdiff: %d recent: %d.%d event: %d.%d\n",tdiff,recent_seconds,recent_nseconds,sec,nsec);
            if(tdiff<0) tdiff=0;

            int data_base=MIDI_DATA_SIZE*alsa_events[buffer_num];
      
            alsa_data_buffer[buffer_num][data_base+0]=tdiff;
            alsa_data_buffer[buffer_num][data_base+1]=ev->data.note.channel; 
            alsa_data_buffer[buffer_num][data_base+2]=command; 
            alsa_data_buffer[buffer_num][data_base+3]=ev->data.note.note;
            alsa_data_buffer[buffer_num][data_base+4]=ev->data.note.velocity;

            alsa_events[buffer_num]++;
         pthread_mutex_unlock(&alsa_midi_lock);
      }
      if(e==SND_SEQ_EVENT_PITCHBEND)
      {
         pthread_mutex_lock(&alsa_midi_lock);
 
            tdiff=(sec-recent_seconds)*1e6+(nsec-recent_nseconds)/1e3;
            if(tdiff<0) tdiff=0;

       
            int data_base=MIDI_DATA_SIZE*alsa_events[buffer_num];
      
            alsa_data_buffer[buffer_num][data_base+0]=tdiff;
            alsa_data_buffer[buffer_num][data_base+1]=ev->data.control.channel; 
            alsa_data_buffer[buffer_num][data_base+2]=command; 
            alsa_data_buffer[buffer_num][data_base+3]=ev->data.control.value;

            alsa_events[buffer_num]++;
         pthread_mutex_unlock(&alsa_midi_lock);

      }
      if(e==SND_SEQ_EVENT_CONTROLLER)
      {
         pthread_mutex_lock(&alsa_midi_lock); 
            tdiff=(sec-recent_seconds)*1e6+(nsec-recent_nseconds)/1e3;
            if(tdiff<0) tdiff=0;

       
            int data_base=MIDI_DATA_SIZE*alsa_events[buffer_num];
      
            alsa_data_buffer[buffer_num][data_base+0]=tdiff;
            alsa_data_buffer[buffer_num][data_base+1]=ev->data.control.channel;
            alsa_data_buffer[buffer_num][data_base+2]=0xB0; 
            alsa_data_buffer[buffer_num][data_base+3]=ev->data.control.param; 
            alsa_data_buffer[buffer_num][data_base+4]=ev->data.control.value;

            alsa_events[buffer_num]++;
         pthread_mutex_unlock(&alsa_midi_lock);
      }
   }

	if(ev->flags & SND_SEQ_EVENT_LENGTH_MASK==SND_SEQ_EVENT_LENGTH_FIXED)
		return sizeof(snd_seq_event_t);

	return sizeof(snd_seq_event_t) + ev->data.ext.len;
}


void event_decoder_start_timer(int queue,
			       int client ATTRIBUTE_UNUSED,
			       int port ATTRIBUTE_UNUSED)
{
	int err;

	if ((err = snd_seq_start_queue(alsa_handle, queue, NULL))<0)
		fprintf(stderr, "Timer event output error: %s\n", snd_strerror(err));
	while (snd_seq_drain_output(alsa_handle)>0)
		sleep(1);
}

void *alsa_event_decoder_loop(void *data)
{
	snd_seq_event_t *ev;
	struct pollfd *pfds;
	int max;
	int err;

	max = snd_seq_poll_descriptors_count(alsa_handle, POLLIN);
	pfds = (pollfd *)alloca(sizeof(*pfds) * max);
	

   while (1) {
   	//dump_current_time(alsa_handle, queue);

		snd_seq_poll_descriptors(alsa_handle, pfds, max, POLLIN);
		if (poll(pfds, max, -1) < 0)
			break;
		do {
			if ((err = snd_seq_event_input(alsa_handle, &ev))<0)
				break;
			if (!ev)
				continue;
  //                      printf("got an event in alsa_event_decoder_loop\n");
			decode_event_simple(ev);
			snd_seq_free_event(ev);
		} while (err > 0);

	}
}

void set_alsa_name()
{
	int err;
	char name[64];
	
	sprintf(name, "Dave Alsa MIDI - %i", getpid());
	if ((err = snd_seq_set_client_name(alsa_handle, name)) < 0) {
		fprintf(stderr, "Set client info error: %s\n", snd_strerror(err));
		exit(0);
	}
}

void get_alsa_system_info()
{
	int err;
	snd_seq_system_info_t *sysinfo;
	
	snd_seq_system_info_alloca(&sysinfo);
	if ((err = snd_seq_system_info(alsa_handle, sysinfo))<0) {
		fprintf(stderr, "System info error: %s\n", snd_strerror(err));
		exit(0);
	}
	max_clients = snd_seq_system_info_get_clients(sysinfo);
	max_ports = snd_seq_system_info_get_ports(sysinfo);
	max_queues = snd_seq_system_info_get_ports(sysinfo);
}


int instrument;

int find_specific_alsa_client(string client_name)
{
   printf("trying to find a ALSA MIDI client: %s\n",client_name.c_str());

	int err, idx, min, max;
	snd_seq_client_info_t *info;

	snd_seq_client_info_alloca(&info);

   min=0;
   max=max_clients;   

	for (idx = min; idx < max; idx++) {
		if ((err = snd_seq_get_any_client_info(alsa_handle, idx, info))<0) {
			if (err == -ENOENT)
				continue;
			fprintf(stderr, "Client %i info error: %s\n", idx, snd_strerror(err));
			exit(0);
		}
      string name=snd_seq_client_info_get_name(info);

      printf("  found: %s\n",name.c_str());

      if((name.find(client_name)!=string::npos))
      {
         printf("    MATCH!\n");
         instrument=0;
         return idx;
      }


	}

   printf("no match exiting\n");
   exit(1);
}

void connect_alsa_ports()
{
	snd_seq_port_info_t *pinfo;
	snd_seq_port_subscribe_t *sub;
	snd_seq_addr_t addr;
	int client, port, max, err, v1, v2;
	char *ptr;

	if ((client = snd_seq_client_id(alsa_handle))<0) {
		fprintf(stderr, "Cannot determine client number: %s\n", snd_strerror(client));
		return;
	}
	printf("Client ID = %i\n", client);
	if ((alsa_queue = snd_seq_alloc_queue(alsa_handle))<0) {
		fprintf(stderr, "Cannot allocate queue: %s\n", snd_strerror(alsa_queue));
		return;
	}
	printf("Queue ID = %i\n", alsa_queue);
	if ((err = snd_seq_nonblock(alsa_handle, 1))<0)
		fprintf(stderr, "Cannot set nonblock mode: %s\n", snd_strerror(err));
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_name(pinfo, "Input");
	snd_seq_port_info_set_type(pinfo, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	snd_seq_port_info_set_capability(pinfo, SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_WRITE);
	if ((err = snd_seq_create_port(alsa_handle, pinfo)) < 0) {
		fprintf(stderr, "Cannot create input port: %s\n", snd_strerror(err));
		return;
	}
	port = snd_seq_port_info_get_port(pinfo);
	event_decoder_start_timer(alsa_queue, client, port);

	snd_seq_port_subscribe_alloca(&sub);
	addr.client = SND_SEQ_CLIENT_SYSTEM;
	addr.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
	snd_seq_port_subscribe_set_sender(sub, &addr);
	addr.client = client;
	addr.port = port;
	snd_seq_port_subscribe_set_dest(sub, &addr);
	snd_seq_port_subscribe_set_queue(sub, alsa_queue);
	snd_seq_port_subscribe_set_time_update(sub, 1);
	snd_seq_port_subscribe_set_time_real(sub, 1);
	if ((err = snd_seq_subscribe_port(alsa_handle, sub))<0) {
		fprintf(stderr, "Cannot subscribe announce port: %s\n", snd_strerror(err));
		return;
	}

	addr.client = SND_SEQ_CLIENT_SYSTEM;
	addr.port = SND_SEQ_PORT_SYSTEM_TIMER;
	snd_seq_port_subscribe_set_sender(sub, &addr);
	if ((err = snd_seq_subscribe_port(alsa_handle, sub))<0) {
		fprintf(stderr, "Cannot subscribe timer port: %s\n", snd_strerror(err));
		return;
	}

	snd_seq_port_subscribe_set_time_real(sub, 1);
	
   v1=addr.client = alsa_client;
	v2=addr.port = 0;
	snd_seq_port_subscribe_set_sender(sub, &addr);
	
	if ((err = snd_seq_subscribe_port(alsa_handle, sub))<0) {
			fprintf(stderr, "Cannot subscribe port %i from client %i: %s\n", v2, v1, snd_strerror(err));
			return;
	}
}

int setup_midi_alsa(string midi_device_name)
{
   int err;

   if ((err = snd_seq_open(&alsa_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0))<0)
   {
      fprintf(stderr, "Open error: %s\n", snd_strerror(err));
      exit(0);
   }

   set_alsa_name();
   get_alsa_system_info();
   alsa_client=find_specific_alsa_client(midi_device_name);

   connect_alsa_ports();

   printf("trying to setup midi data mutex\n"); fflush(stdout);
   if (pthread_mutex_init(&alsa_midi_lock,NULL)!=0)
   {
      printf("  FAIL!\n");
      exit(1);
   }
   printf("   DONE!\n");

   snd_seq_queue_status_malloc(&alsa_status);

   printf("trying to setup pthread for alsa decoder loop\n");
   err = pthread_create(&alsa_thread, NULL, alsa_event_decoder_loop, NULL);
   if (err)
   {
      printf("ERROR; return code from pthread_create() is %d\n", err);
      exit(-1);
   }
   printf("   DONE\n");

   midi_setup=true;

   return instrument;
}
 




