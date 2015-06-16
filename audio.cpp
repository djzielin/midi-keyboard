#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <errno.h>
//#include <unistd.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <unistd.h> //for sleep( )

jack_port_t *output_port;
jack_client_t *jack_client;
float audio_sample_rate=44100; //TODO, figure out a way to get sample rate before we do heavy lifting (ie, init sounds takes alot of CPU)

extern int generate_samples(jack_nframes_t nframes, void *arg);

void audio_shutdown_jack()
{
   jack_client_close(jack_client);
}

static void jack_shutdown(void *arg)
{
   //exit(1);
}

void send_audio_to_card_jack(float *data, int nframes)
{
   jack_default_audio_sample_t *out = (jack_default_audio_sample_t *) jack_port_get_buffer (output_port, nframes);
   memcpy(out,data,nframes*sizeof(float)); //TODO: this will be slightly more complicated for stereo signals
}

void setup_audio_jack(int aport)
{
   printf("setting up for port: %d\n",aport);
   //exit(1);

   if ((jack_client = jack_client_open("dave_keyboard", JackNullOption, NULL)) == 0)
   {
      fprintf(stderr, "jack server not running?\n");
      exit(1);
   }
   
   audio_sample_rate=jack_get_sample_rate (jack_client);

   jack_set_process_callback (jack_client, generate_samples, 0);
   //jack_set_sample_rate_callback (jack_client, audio_sample_rate_changed_jack, 0); //I don't think we need to support changing sample rate on the fly
   //jack_on_shutdown (jack_client, jack_shutdown, 0);

    output_port = jack_port_register (jack_client, "audio_out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   //output_port2 = jack_port_register (jack_client, "audio out2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
 
   printf("connecting to jack...\n");
   if (jack_activate (jack_client))
   {
      fprintf(stderr, "cannot activate client");
      exit (1);
   }
   printf("   success!\n");

   printf("connecting to physical playback ports\n");

   const char **ports;
   ports = jack_get_ports (jack_client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
   if (ports == NULL)
   {
      fprintf(stderr, "no physical playback ports\n");
      exit (1);
   }

   if (jack_connect (jack_client, jack_port_name (output_port), ports[aport])) 
   {
      fprintf (stderr, "cannot connect playback ports\n");
      exit(1);
   }

   /*if (jack_connect (jack_client, jack_port_name (output_port), ports[1])) //right channel 
   {
      fprintf (stderr, "cannot connect output ports\n");
      //exit(1);
   }*/
}

void audio_loop_jack()
{
   while(1)
      sleep(1);
}

///////////////////////////////////////////////////////////////////////////////
// ALSA
///////////////////////////////////////////////////////////////////////////////

#include <asoundlib.h>

static char device[] = "hw:0,0";			// raw hardware - no sample conversion 
//static char device[] = "plughw:0,0";	   // plugin that will automatically convert all forwards
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE; //S32_LE;	/* sample format */
static unsigned int rate = 44100;			/* stream rate */
static unsigned int channels = 2;			/* count of channels */
static int resample = 0;				/* enable alsa-lib resampling */ //0 disables resampling
static int period_event = 0;				/* produce poll event after each period */

static snd_pcm_uframes_t buffer_size;
static snd_pcm_uframes_t period_size=128; //was doing 128
static snd_output_t *output = NULL;

snd_pcm_t *alsa_audio_handle;
int *alsa_samples;
snd_pcm_channel_area_t *alsa_areas;
snd_async_handler_t *ahandler;

snd_pcm_status_t *pcm_status;

int xrun_recovery(snd_pcm_t *handle, int err);

bool skip_poll=false;

void send_audio_to_card_alsa(float *data, int nframes)
{
   int format_bits = snd_pcm_format_width(format);
   unsigned int maxval = (1 << (format_bits - 1)) - 1; //TODO maybe calc this once?

   int index=0;
   int converted;
   //int *samples=alsa_samples;     //for 32 bit frames
   short int *samples=(short int *)alsa_samples; //for 16 bit frames

   for(int i=0;i<nframes;i++)
   {
      converted=data[i]*maxval;
      samples[index]=converted;
      index++;
      samples[index]=converted;
      index++;
   }

   int frames_to_write=nframes;
   int offset=0;

   skip_poll=false;

   while(frames_to_write>0) //do a "safe-write" where we keep trying to write if we can't get all the data sent
   {

      int err = snd_pcm_writei(alsa_audio_handle, &alsa_samples[offset], frames_to_write); //return frames written or error

      if(err==frames_to_write)
         return;

      if (err < 0)
      {
         if (xrun_recovery(alsa_audio_handle, err) < 0)
         {
            printf("Write error: %s\n", snd_strerror(err));
	    exit(EXIT_FAILURE);
         }
         else
         {
            printf("recovered from xrun\n");
            skip_poll=true;
            return; //TODO pcm.c code has us disable the next poll here. hmmmm.
         }
      }

      printf("we wanted to write: %d frames but could only write %d frames.\n",frames_to_write,err);

      frames_to_write-=err;
      offset+=err;

      //TODO, pcm.c had a wait for poll here, but do we really need to wait?
   }
}

int xrun_recovery(snd_pcm_t *handle, int err)
{
   if (err == -EPIPE)  /* under-run */
   {
      err = snd_pcm_prepare(handle);
      if (err < 0)
         printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
      return 0;
   }
   else if (err == -ESTRPIPE)
   {
      while ((err = snd_pcm_resume(handle)) == -EAGAIN)
         sleep(1); /* wait until the suspend flag is released */
      if (err < 0)
      {
         err = snd_pcm_prepare(handle);
         if (err < 0)
            printf("Can't recover from suspend, prepare failed: %s\n", snd_strerror(err));
      }
      //return 0;
   }
   return err;
}

/*
* Transfer method - write and wait for room in buffer using poll
*/

void wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count)
{
   if(skip_poll) return;

   unsigned short revents;
   int err;

   while (1)
   {
      poll(ufds, count, -1);
      snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);

      if (revents & POLLERR)
      {
         err = -EIO;
         break;
      }
      if (revents & POLLOUT)
      {
         err = 0;
         break;
      }
   }

   if (err < 0)
   {
      if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN || snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED)
      {
         err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE : -ESTRPIPE;
         if (xrun_recovery(handle, err) < 0)
         {
            printf("Write error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
         }
      }
      else
      {
         printf("Wait for poll failed\n");
         exit(EXIT_FAILURE);
      }
   }
}

void audio_loop_alsa()
{
   struct pollfd *ufds;
   double phase = 0;
   signed short *ptr;
   int err, count, cptr, init;

   count = snd_pcm_poll_descriptors_count (alsa_audio_handle);

   if (count <= 0)
   {
      printf("Invalid poll descriptors count\n");
      //return count;
      return;

   }
   ufds = (pollfd *)malloc(sizeof(struct pollfd) * count);
   if (ufds == NULL) {
      printf("No enough memory\n");
      //return -ENOMEM;
      return;
   }

   if ((err = snd_pcm_poll_descriptors(alsa_audio_handle, ufds, count)) < 0) {
      printf("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
      //return err;
      return;
   }


   while (1)
   {
      generate_samples(period_size, 0);
 	    
      wait_for_poll(alsa_audio_handle,ufds,count);

      //snd_pcm_status(alsa_audio_handle,pcm_status);
      //snd_pcm_uframes_t avail_frames=snd_pcm_status_get_avail(pcm_status);
      //snd_pcm_sframes_t delay_frames=snd_pcm_status_get_delay (pcm_status);
      //printf("%d %d\n",avail_frames,delay_frames);	

   }
}

static int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
			snd_pcm_access_t access)
{
	unsigned int rrate;
	snd_pcm_uframes_t size;
	int err, dir;

  // period_size=period_size*snd_pcm_format_physical_width(format)/8*channels;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
	if (err < 0) {
		printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
  
   err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
	  	printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
	 	return err;
	}      

	/* set the stream rate */
	rrate = rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
		return err;
	}
	if (rrate != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	
  snd_pcm_uframes_t     period_size_min;
  snd_pcm_uframes_t     period_size_max;
  snd_pcm_uframes_t     buffer_size_min;
  snd_pcm_uframes_t     buffer_size_max;

  err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
  err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
  err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
  err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
  printf("Buffer size range from %lu to %lu\n",buffer_size_min, buffer_size_max);
  printf("Period size range from %lu to %lu\n",period_size_min, period_size_max);
   
   
   err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);
   if (err < 0) {
		printf("Unable to set period size %lu for playback: %s\n", period_size, snd_strerror(err));
		return err;
	} 

	err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
	if (err < 0) {
		printf("Unable to get period size for playback: %s\n", snd_strerror(err));
		return err;
	}
	period_size = size;
   printf("actual period size is: %lu dir: %d\n",period_size,dir);
 
   int periods=2;
   /* Set number of periods. Periods used to be called fragments. */ 
    if (snd_pcm_hw_params_set_periods(handle, params, periods, 0) < 0) {
      fprintf(stderr, "Error setting periods.\n");
      return(-1);
    }

  buffer_size=period_size*periods;
  err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
   if (err < 0) {
		printf("Unable to set buffer size %lu for playback: %s\n", buffer_size, snd_strerror(err));
		return err;
	}

	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
		return err;
	}

	buffer_size = size;
   printf("actual buffer size is: %lu\n",buffer_size);


	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size); //(buffer_size / period_size) * period_size
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size); 
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
	if (period_event) {
		err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

/*
 *   Underrun and suspend recovery
 */
 
/*
static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (verbose)
		printf("stream recovery\n");
	if (err == -EPIPE) {	//  under-run 
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	// wait until the suspend flag is released 
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}*/

/*
 *   Transfer method - asynchronous notification
 */
/*
struct async_private_data {
	signed short *samples;
	snd_pcm_channel_area_t *areas;
	double phase;
};

static void async_callback(snd_async_handler_t *ahandler)
{
	snd_pcm_sframes_t avail;
	int err;
	
	avail = snd_pcm_avail_update(alsa_audio_handle);
	while (avail >= period_size)
   {
      generate_samples(period_size,0);
		avail = snd_pcm_avail_update(alsa_audio_handle);
	}
}
*/

void audio_shutdown_alsa()
{
   free(alsa_areas);
	free(alsa_samples);
	snd_pcm_close(alsa_audio_handle);
}

void setup_audio_alsa(int port)
{	
   snd_pcm_status_malloc (&pcm_status); 	

   int err;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;

	unsigned int chn;

   audio_sample_rate=rate;
   
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);
	
	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		exit(1);
	}

	printf("Playback device is %s\n", device);
	printf("Stream parameters are %iHz, %s, %i channels\n", rate, snd_pcm_format_name(format), channels);
	
	if ((err = snd_pcm_open(&alsa_audio_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(1);
	}
	
	if ((err = set_hwparams(alsa_audio_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) { //do async
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = set_swparams(alsa_audio_handle, swparams)) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	alsa_samples = (int *)malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
	if (alsa_samples == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}
	
	alsa_areas = (snd_pcm_channel_area_t *)calloc(channels, sizeof(snd_pcm_channel_area_t));
	if (alsa_areas == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}
	for (chn = 0; chn < channels; chn++) {
		alsa_areas[chn].addr = alsa_samples;
		alsa_areas[chn].first = chn * snd_pcm_format_physical_width(format);
		alsa_areas[chn].step = channels * snd_pcm_format_physical_width(format);
	}

	//err = snd_async_add_pcm_handler(&ahandler, alsa_audio_handle, async_callback, 0);
	//if (err < 0) {
	//	printf("Unable to register async handler\n");
	//	exit(EXIT_FAILURE);
	//}
/*	for (int count = 0; count < 2; count++) { //initial write to fill buffer here
		generate_samples(period_size,0);
	}
	if (snd_pcm_state(alsa_audio_handle) == SND_PCM_STATE_PREPARED) {
		err = snd_pcm_start(alsa_audio_handle);
		if (err < 0) {
			printf("Start error: %s\n", snd_strerror(err));
			exit(EXIT_FAILURE);
		}
	}
*/

    
}



