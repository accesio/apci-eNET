all: check check_dma isp-fpga log

GCC ?= aarch64-none-linux-gnu-gcc

log: logAdcDataForRangeCode.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o logAdcDataForRangeCode logAdcDataForRangeCode.c apcilib.c -lm -lpthread -lncurses -lmenu -O3

check: check.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o check check.c apcilib.c -lm -lpthread -O3

check_dma: check_dma.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -g -o check_dma check_dma.c apcilib.c -lm -lpthread -O3

isp-fpga: isp-fpga.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o isp-fpga isp-fpga.c apcilib.c -lm

clean:
	rm check check_dma isp-fpga
