obj-m += pmccntr-test.o
KDIR := /usr/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC = gcc
CFLAGS = -Wall -Wextra -O2
USR_TARGET = pmccntr-benchmark
USR_SRC = $(USR_TARGET).c

.PHONY: all clean test benchmark



all: module $(USR_TARGET)

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USR_TARGET): $(USR_SRC)
	$(CC) $(CFLAGS) -o $(USR_TARGET) $(USR_SRC)

test: module
	sudo dmesg -C
	sudo insmod pmccntr-test.ko
	sudo rmmod pmccntr-test.ko
	dmesg

benchmark: $(USR_TARGET)
	./$(USR_TARGET)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USR_TARGET)
