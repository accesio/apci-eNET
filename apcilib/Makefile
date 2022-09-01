all: check check_dma isp-fpga logAdcData

GCC ?= aarch64-none-linux-gnu-gcc

logAdcData: logAdcData.c apcilib.h apcilib.c
	$(GCC) -o logAdcData logAdcData.c apcilib.c -lm -lpthread -lncurses -lmenu -O3

check: check.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o check check.c apcilib.c -lm -lpthread -O3

check_dma: check_dma.c eNET-AIO.h apcilib.h apcilib.c logging.cpp logging.h
	$(GCC) -std=gnu++2a -g -o check_dma check_dma.c apcilib.c logging.cpp -lm -lpthread -O3

isp-fpga: isp-fpga.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o isp-fpga isp-fpga.c apcilib.c -lm

clean:
	rm check check_dma isp-fpga logAdcData
