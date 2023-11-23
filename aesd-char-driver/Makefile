# See example Makefile from scull project
# Comment/uncomment the following line to disable/enable debugging
# DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ARCH := arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

# Exclude problematic options introduced by the Linaro toolchain
EXTRA_CFLAGS += -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -mno-80387 -mno-red-zone -mfunction-return=thunk-extern -mrecord-mcount
# Exclude the -mcmodel=kernel option
EXTRA_CFLAGS += $(call cc-option,-mcmodel=small,-mcmodel=kernel)

ifeq ($(KERNELRELEASE),)
# call from kernel build system
obj-m := aesdchar.o
aesdchar-y := aesd-circular-buffer.o main.o
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
