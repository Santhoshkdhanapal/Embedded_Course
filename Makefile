obj-m += kshield_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

CXX      := g++
CXXFLAGS := -O2 -Wall -Wextra -std=c++17 -pthread

all: driver daemon

driver:
	@echo "--- Building K-Shield Linux Kernel Module ---"
	$(MAKE) -C $(KDIR) M=$(PWD) modules

daemon: kshield_daemon.cpp kshield_common.h
	@echo "--- Compiling K-Shield User-Space Daemon ---"
	$(CXX) $(CXXFLAGS) kshield_daemon.cpp -o kshield_daemon

clean:
	@echo "--- Cleaning Build Directory ---"
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f kshield_daemon

load:
	sudo insmod kshield_driver.ko
	sudo chmod 666 /dev/kshield

unload:
	sudo rmmod kshield_driver

.PHONY: all driver daemon clean load unload

