LDFLAGS+=  -ljack -lsndfile -lsamplerate -lasound -lpthread 
CXXFLAGS+= -O3  -I/usr/include/alsa 
RASPBERRY=yes

ifdef RASPBERRY
  LDFLAGS  +=-lwiringPi
  CXXFLAGS +=-DRASPBERRY -I/usr/local/include/stk
  LDFLAGS  += -lstk
else
  LDFLAGS  +=-L$(wildcard ~/stk-4.4.3/src) -lstk
  CXXFLAGS += -I$(wildcard ~/stk-4.4.3/include)
endif

CXXFLAGS += -DUSE_ALSA

# comb_filter.o tremolo.o 
COMMON_OBJECTS=mono_sample.o key_sample.o simple_envelope.o wave_position.o wave_generator.o key_additive.o midi.o audio.o

all: $(COMMON_OBJECTS) keyboard.o
	g++ keyboard.o $(COMMON_OBJECTS) $(LDFLAGS) -o keyboard

clean:
	rm -f *.o
	rm -f keyboard
