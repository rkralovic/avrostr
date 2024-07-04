# Compile parameters setup

AVR_ROOT=$(shell which avr-g++ | xargs dirname)/..
AVR_SPECS_DIR=$(AVR_ROOT)/avr/lib

HDRS=$(wildcard *.h ../*.h) ../gendata/image-a4.h ../gendata/image-a3.h
CFLAGS += -Wall -W -O3 -std=c++20 -flto -nodevicespecs \
          -I$(AVR_ROOT)/avr/include -Wl,--gc-sections

CFLAGS += -specs=$(AVR_SPECS_DIR)/avrxmega2/specs-avr64dd32 -DF_CPU=16000000UL
AVRDUDE_PARAMS = -p avr64dd32 -c serialupdi -P /dev/ttyACM0

# Hack to avoid linker bug with binutils < 2.39. We should upgrade binutils.
CFLAGS += -Wl,--defsym,__DATA_REGION_LENGTH__=0xffa0

ID ?= 1
CFLAGS += -DID=$(ID)

ELF=build/main_$(ID).elf
HEX=build/main_$(ID).hex
EEP=build/main_$(ID).eep

all: print_size

build: $(ELF) $(HEX) $(EEP)

print_size: $(ELF)
	avr-size $<

objdump: $(ELF)
	avr-objdump -D $< | less

write_flash: $(HEX)
	avrdude $(AVRDUDE_PARAMS) -U flash:w:$<:a

write_eeprom: $(EEP)
	avrdude $(AVRDUDE_PARAMS) -U eeprom:w:$<:a

write: write_flash write_eeprom

setup:
	avrdude $(AVRDUDE_PARAMS) -U bodcfg:w:0x76:m

build/%_$(ID).elf: %.cc $(HDRS) Makefile
	avr-g++ $(CFLAGS) -o $@ $<

%.hex: %.elf
	avr-objcopy -j .text -j .data -j .rodata -O ihex $< $@

%.eep: %.elf
	avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex $< $@

clean:
	rm build/*
