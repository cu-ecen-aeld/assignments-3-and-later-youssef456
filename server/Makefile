# Makefile for aesdsocket

CC ?= gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt

TARGET = aesdsocket
SRC = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRC:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.o

