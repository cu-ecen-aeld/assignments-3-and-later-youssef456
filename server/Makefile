CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -g -Wall -Werror

TARGET ?= aesdsocket

LDFLAGS ?= -lpthread -lrt

default: $(TARGET)

EXTRA_CFLAGS = -DUSE_AESD_CHAR_DEVICE=1

all: default

aesdsocket: aesdsocket.o
	$(CC) $(LDFLAGS) $(CCFLAGS) $(EXTRA_CFLAGS)  -c aesdsocket.c


aesdsocket.o: aesdsocket.c
	$(CC) $(LDFLAGS) $(CCFLAGS) $(EXTRA_CFLAGS)  -c aesdsocket.c


clean:
	rm -f $(TARGET) aesdsocket.o

