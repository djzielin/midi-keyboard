LDFLAGS+= -lstk -ljack -lsndfile -lsamplerate -lasound -lpthread -lwiringPi
CXXFLAGS+= -O3 -I/usr/include/stk -I/usr/include/alsa

# comb_filter.o tremolo.o 
COMMON_OBJECTS=mono_sample.o key_sample.o simple_envelope.o wave_position.o wave_generator.o key_additive.o midi.o audio.o

all: $(COMMON_OBJECTS) keyboard.o
	g++ keyboard.o $(COMMON_OBJECTS) $(LDFLAGS) -o keyboard

clean:
	rm -f *.o
	rm -f keyboard
