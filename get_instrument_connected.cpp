#include "asoundlib.h"

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


int find_specific_alsa_client(string client_name)
{
   printf("trying to find a ALSA MIDI client: %s\n",client_name.c_str());

   int err, idx, min, max;
   snd_seq_client_info_t *info;

   snd_seq_client_info_alloca(&info);

   min=0;
   max=max_clients;   

   for (idx = min; idx < max; idx++) 
   {
      if ((err = snd_seq_get_any_client_info(alsa_handle, idx, info))<0) 
      {
         if (err == -ENOENT)
	    continue;
	 fprintf(stderr, "Client %i info error: %s\n", idx, snd_strerror(err));
	 exit(0);
      }
      string name=snd_seq_client_info_get_name(info);

      //printf("  found: %s\n",name.c_str());

      if((name.find(client_name)!=string::npos))
      {
         printf("  found it!\n",client_name.c_str());
         return 1;
      }
   }

   return 0;
}

int main(int argc, char *argv[])
{
   int err;

   if ((err = snd_seq_open(&alsa_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0))<0)
   {
      fprintf(stderr, "Open error: %s\n", snd_strerror(err));
      exit(0);
   }

   set_alsa_name();
   get_alsa_system_info();
   
   if(find_specific_alsa_client("Guitar"))     return 1;
   if(find_specific_alsa_client("Keystation")) return 2;
   if(find_specific_alsa_client("Uno"))        return 3;
 
   printf("didn't find anything connected --- :(\n");
   return 0;
}
 




