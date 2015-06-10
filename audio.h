void audio_shutdown_jack();
void send_audio_to_card_jack(float *data, int nframes);
void setup_audio_jack();

void audio_shutdown_alsa();
void send_audio_to_card_alsa(float *data, int nframes);
void setup_audio_alsa();
int audio_loop();

