### Project that we will compile:
CONTIKI_PROJECT = stops_sensors
### Sensor drivers we need (typically contained in
### ~/contiki/platform/zoul/dev). Remove # if necessary:
# CONTIKI_TARGET_SOURCEFILES += dht22.c tsl256x.c

### When we do "make", we will compile all the projects mentioned above:
all: $(CONTIKI_PROJECT)

### Compilation for zoul module:
TARGET = zoul

### Location of the "contiki" root folder relative to the current folder:
CONTIKI = ../../../contiki/
include $(CONTIKI)/Makefile.include

CONTIKI_TARGET_SOURCEFILES += dht22.c tsl256x.c
