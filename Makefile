SYS := $(shell gcc -dumpmachine)

ifneq (, $(findstring arm, $(SYS)))
 RASPBERRY=yes
endif

LDFLAGS+=  -ljack -lsndfile -lsamplerate -lasound -lpthread 
CXXFLAGS+= -O3  -I/usr/include/alsa 

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

all: $(COMMON_OBJECTS) keyboard.o get_instrument_connected.o
	g++ keyboard.o $(COMMON_OBJECTS)  $(LDFLAGS) -o keyboard
	g++ get_instrument_connected.o  $(LDFLAGS) -o get_instrument_connected

clean:
	rm -f *.o
	rm -f keyboard
	rm -f get_instrument_connected
