<<<<<<< HEAD
all: logAdcData check check_dma isp-fpga

GCC ?= aarch64-none-linux-gnu-gcc

logAdcData: logAdcData.c apcilib.h apcilib.c
	$(GCC) -o logAdcData logAdcData.c apcilib.c -lm -lpthread -lncurses -lmenu -O3
=======
all: check check_dma isp-fpga log

GCC ?= aarch64-none-linux-gnu-gcc

log: logAdcDataForRangeCode.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o logAdcDataForRangeCode logAdcDataForRangeCode.c apcilib.c -lm -lpthread -lncurses -lmenu -O3
>>>>>>> 3bec6ce6c1533eff039c95dece57a0268f41245b

check: check.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o check check.c apcilib.c -lm -lpthread -O3

check_dma: check_dma.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -g -o check_dma check_dma.c apcilib.c -lm -lpthread -O3

isp-fpga: isp-fpga.c eNET-AIO.h apcilib.h apcilib.c
	$(GCC) -o isp-fpga isp-fpga.c apcilib.c -lm

clean:
	rm check check_dma isp-fpga
