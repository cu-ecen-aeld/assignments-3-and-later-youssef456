CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -g -Wall -Werror

TARGET ?= aesdsocket

LDFLAGS ?= -lpthread -lrt

default: $(TARGET)

EXTRA_CFLAGS = -DUSE_AESD_CHAR_DEVICE=1

all: default

aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $(TARGET) aesdsocket.o $(LDFLAGS)


aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c aesdsocket.c -o aesdsocket.o


clean:
	rm -f $(TARGET) aesdsocket.o

