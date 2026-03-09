KERNELDIR := /home/tom/linux
CURRENT_PATH := $(shell pwd)

obj-m := my_ads1115.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
