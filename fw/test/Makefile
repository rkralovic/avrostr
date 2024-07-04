TESTS=$(patsubst %.cc,run_%,$(wildcard *_test.cc))
OBJECTS=$(patsubst %.cc,build/%.o,$(wildcard *.cc))
DEPENDS=$(OBJECTS:.o=.d)

AVR_F_CPU=4000000
#AVR_F_CPU=2000000
CFLAGS=-O3 -Wall -W -std=c++20 -DF_CPU=$(AVR_F_CPU)
LIBS=-lsimavr -lgmock_main -lgtest -lgmock 

CFLAGS += $(shell pkg-config --cflags cairo xcb cairo-pdf xcb-icccm)
LIBS   += $(shell pkg-config --libs cairo xcb cairo-pdf xcb-icccm)

AVR_ROOT=$(shell which avr-g++ | xargs dirname)/..
AVR_CFLAGS=-g3 -Wall -W -O3 -std=c++20 -flto \
           -I$(AVR_ROOT)/avr/include \
           -I/usr/include/simavr/avr \
           -mmcu=atmega328 -DF_CPU=$(AVR_F_CPU) -D__AVR_ATmega328PB__

all: $(TESTS)

build/%.d: %.cc
	echo -n $@" "build/> $@
	g++ $(CFLAGS) -MM $< >> $@

build/%_test_avr.d: %_test_avr.cc
	echo -n $@" "build/> $@
	avr-g++ $(AVR_CFLAGS) -MM $< >> $@

include $(DEPENDS)

build/%.o: %.cc Makefile
	g++ $(CFLAGS) -c $< -o $@

build/%_avr.o: %_avr.cc Makefile
	avr-g++ $(AVR_CFLAGS) -c $< -o $@

build/%_test: build/%_test.o build/window.o Makefile
	g++ $< build/window.o -o $@ $(LIBS)

build/%_test.elf: build/%_test_avr.o Makefile
	avr-g++ $(AVR_CFLAGS) $< -o $@

run_%_test: build/%_test build/%_test.elf
	./$<

PHONY.: clean
clean:
	rm -rf build/*
