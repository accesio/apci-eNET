#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <ncurses.h>
#include <menu.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#include "apcilib.h"
#include "eNET-AIO.h"

#define DEVICEPATH "/dev/apci/pcie_adio16_16f_0"

/* the following set of #defines configure what the sample does; feel free to change these */
#define SAMPLE_RATE 1000000.0 /* Hz. Note: This is the overall sample rate, sample rate of each channel is SAMPLE_RATE / CHANNEL_COUNT */
#define LOG_FILE_NAME "samples.bin"
#define SECONDS_TO_LOG 5.0
#define ADC_MAX_CHANNEL 16 // will need to change to support submuxed models
#define DEFAULT_START_CHANNEL 0
#define DEFAULT_END_CHANNEL 15
#define DEFAULT_ADC_RANGE (bmSingleEnded | bmAdcRange_u10V) // the sample makes all channels the same range but the device is more flexible

/* The rest of this is internal for the sample to use and should not be changed until you understand it all */
double Hz;
uint8_t CHANNEL_COUNT = DEFAULT_END_CHANNEL - DEFAULT_START_CHANNEL + 1;
#define NUM_CHANNELS (2 * CHANNEL_COUNT)
int SamplesToLog = (SECONDS_TO_LOG * SAMPLE_RATE * 2);

#define RING_BUFFER_SLOTS 16
static uint32_t ring_buffer[RING_BUFFER_SLOTS][SAMPLES_PER_TRANSFER];
static sem_t ring_sem;
volatile static int terminate;

#define DMA_BUFF_SIZE (BYTES_PER_TRANSFER * RING_BUFFER_SLOTS)
int NumberOfDmaTransfers = 0; // NUMBER_OF_DMA_TRANSFERS;

int fd = -1;
pthread_t logger_thread;
pthread_t worker_thread;
void txtPuts(const char *txt, ...);
void ctxtPuts(chtype color, const char *msg, ...);

const char *stringRanges[] = {"0-10V  ", "bip10V ", "0-5V   ", "bip5V  ",
							  "0-2V   ", "bip2V  ", "0-1V   ", "bip1V  "};

int _AdcStartMode = bmAdcTriggerTypeChannel;
int _AdcStartRateChoice = 0;
int _AdcRangeChoice[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
double AdcRates_divisors[8] = {1.0, 2.0, 4.0, 10.0, 20.0, 100.0, 1000.0, 10000.0};
double AdcRates[8] = {1000000.0, 500000.0, 250000.0, 100000.0, 50000.0, 10000.0, 1000.0, 100.0};
int _AdcStartCh = DEFAULT_START_CHANNEL;
int _AdcEndCh = DEFAULT_END_CHANNEL;
double _AdcStartRate = SAMPLE_RATE;
char LogFileName[4096];
int done = 0;
int bRunLogging = 0;

#define SHOW_CONFIG_LEFT 70
void ShowConfig()
{
	double rateDivisor = 1.0;
	if (_AdcStartMode == bmAdcTriggerTypeScan)
	{
		rateDivisor = _AdcEndCh - _AdcStartCh + 1;
	}
	_AdcStartRate = AdcRates[_AdcStartRateChoice] / rateDivisor;

	char sRange[16][10];
	for (int GainGroup = 0; GainGroup < 16; GainGroup++)
		snprintf(sRange[GainGroup], 9, "%s", stringRanges[_AdcRangeChoice[GainGroup]]);
	char sStartMode[20];
	snprintf(sStartMode, 20, "%s", _AdcStartMode == bmAdcTriggerTypeChannel ? "Channel Start" : "Scan Start   ");
	char sStartChannel[2];
	snprintf(sStartChannel, 2, "%X", _AdcStartCh);
	char sEndChannel[2];
	snprintf(sEndChannel, 2, "%X", _AdcEndCh);
	char sStartRate[12];
	snprintf(sStartRate, 12, "%9.1f", _AdcStartRate / rateDivisor);
	mvprintw(4, SHOW_CONFIG_LEFT, "ADC Configuration");
	mvprintw(5, SHOW_CONFIG_LEFT, "ADC Start Mode: %s", sStartMode);
	mvprintw(6, SHOW_CONFIG_LEFT, "ADC Start Rate: %s Hz", sStartRate);
	mvprintw(7, SHOW_CONFIG_LEFT, "Acquiring channels %s..%s", sStartChannel, sEndChannel);
	mvprintw(8, SHOW_CONFIG_LEFT, "Logging %3.2f seconds of data", SECONDS_TO_LOG);
	for (int GainGroup = 0; GainGroup < 16; GainGroup++)
		mvprintw(9 + GainGroup, SHOW_CONFIG_LEFT, "Range Group %d Range: %s", GainGroup, sRange[GainGroup]);
	snprintf(LogFileName, 4000, "Log_%1.1fseconds_ch%X..%X_%s_%9.1fHz.bin", SECONDS_TO_LOG, _AdcStartCh, _AdcEndCh, stringRanges[_AdcRangeChoice[0]], _AdcStartRate);
}

void changeAdcStartChannel(char *unused)
{
	_AdcStartCh++;
	_AdcStartCh %= ADC_MAX_CHANNEL;
	_AdcEndCh = fmax(_AdcStartCh, _AdcEndCh);
}

void changeAdcEndChannel(char *unused)
{
	_AdcEndCh++;
	_AdcEndCh %= ADC_MAX_CHANNEL;
	_AdcEndCh = fmax(_AdcStartCh, _AdcEndCh);
}

void toggleAdcStartMode(char *unused)
{
	if (_AdcStartMode == bmAdcTriggerTypeChannel)
		_AdcStartMode = bmAdcTriggerTypeScan;
	else
		_AdcStartMode = bmAdcTriggerTypeChannel;
}

void changeAdcStartRate(char *unused)
{
	_AdcStartRateChoice++;
	_AdcStartRateChoice %= 8; // limit to a valid range (there are 8 valid ADC ranges)
}

// TODO: this will be a per-channel submenu, eventually...
void changeAdcRanges(char *unused)
{
	for (int GainGroup = 0; GainGroup < 16; ++GainGroup)
	{
		_AdcRangeChoice[GainGroup]++;
		_AdcRangeChoice[GainGroup] %= 8;
	}
}

typedef struct
{
	char *item;
	char *desc;
	void (*function)(char *name);
	char keystroke;
} MenuItem;

void func(char *name)
{
	bRunLogging = 1;
	done = 1;
}

const MenuItem TopMenuItems[] = {
	{"1. Toggle Scan/Channel ADC Start Mode", "desc1", toggleAdcStartMode, '1'},
	{"2. Change ADC Start Rate...", "desc1", changeAdcStartRate, '2'},
	{"3. Change Start Channel...", "desc1", changeAdcStartChannel, '3'},
	{"4. Change End Channel...", "desc1", changeAdcEndChannel, '4'},
	{"5. Change Ranges...", "desc1", changeAdcRanges, '5'},
	{"6. Run this configuration", "desc1", func, '6'},
	{"ESC Exit", "desc1", func, (char)27},
};

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{
	int length, x, y;
	float temp;

	if (win == NULL)
		win = stdscr;
	getyx(win, y, x);
	if (startx != 0)
		x = startx;
	if (starty != 0)
		y = starty;
	if (width == 0)
		width = 80;

	length = strlen(string);
	temp = (width - length) / 2;
	x = startx + (int)temp;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", string);
	wattroff(win, color);
	refresh();
}

int HandleMenu()
{
	ITEM **my_items;
	int c;
	MENU *my_menu;
	int n_choices, i;
	ITEM *cur_item;
	WINDOW *my_menu_win;

	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	n_choices = ARRAY_SIZE(TopMenuItems);
	my_items = (ITEM **)calloc(n_choices + 1, sizeof(ITEM *));

	for (i = 0; i < n_choices; ++i)
	{
		my_items[i] = new_item(TopMenuItems[i].item, TopMenuItems[i].desc);
		set_item_userptr(my_items[i], TopMenuItems[i].function);
	}
	my_menu = new_menu((ITEM **)my_items);
	menu_opts_off(my_menu, O_SHOWDESC);
	/* Create the window to be associated with the menu */
	my_menu_win = newwin(10, 50, 4, 4);
	keypad(my_menu_win, TRUE);

	/* Set main window and sub window */
	set_menu_win(my_menu, my_menu_win);
	set_menu_sub(my_menu, derwin(my_menu_win, 6, 48, 3, 1));

	/* Set menu mark to the string " * " */
	set_menu_mark(my_menu, " * ");
	box(my_menu_win, 0, 0);
	print_in_middle(my_menu_win, 1, 0, 48, "ADC Configuration", COLOR_PAIR(1));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, 48);
	mvwaddch(my_menu_win, 2, 49, ACS_RTEE);
	refresh();

	post_menu(my_menu);
	ShowConfig();
	wrefresh(my_menu_win);

	while (!done)
	{
		c = getch();
		switch (c)
		{
		case 27:
			done = 1;
			bRunLogging = 0;
			break;
		case KEY_DOWN:
			menu_driver(my_menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(my_menu, REQ_UP_ITEM);
			break;
		case 10: /* Enter */
		{
			ITEM *cur;
			void (*p)(char *);

			cur = current_item(my_menu);
			p = item_userptr(cur);
			p((char *)item_name(cur));
			pos_menu_cursor(my_menu);
			break;
		}
		default:
			menu_driver(my_menu, c);
			menu_driver(my_menu, REQ_CLEAR_PATTERN);
			break;
		}
		ShowConfig();
		wrefresh(my_menu_win);
	}
	unpost_menu(my_menu);
	for (i = 0; i < n_choices; ++i)
		free_item(my_items[i]);
	free_menu(my_menu);
}

#define textcolor(x) color_set(x, NULL)
chtype GetTextColor() // todo verify
{
	short color;
	attr_t attrs;
	attr_get(&attrs, &color, NULL);
	return color;
}

enum TColor
{
	cINSTRUCT,
	cBODY,
	cDATA,
};

void ctxtPuts(chtype color, const char *msg, ...)
{
	int oldColor = GetTextColor();
	char tmp[256];
	va_list arg;
	va_start(arg, msg);
	vsprintf(tmp, msg, arg);
	va_end(arg);

	textcolor(color);
	printw(tmp);
	refresh();
	textcolor(oldColor);
	//    if ( testRun.bDebug ) UpdateTestBottom();
}

void txtPuts(const char *txt, ...)
{
	char tmp[256];
	va_list ap;
	va_start(ap, txt);
	vsprintf(tmp, txt, ap);
	va_end(ap);
	ctxtPuts(cBODY, tmp);
}

void abort_handler(int s)
{
	txtPuts("Caught signal %d\n", s);
	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);
	usleep(5);

	terminate = 2;
	pthread_join(logger_thread, NULL);
	exit(1);
}

/* log_main(): background thread to save acquired data to disk.
 * Note this has to keep up or the current static-length ring buffer would overwrite data
 * Launched from Worker Thread
 */
void *log_main(void *arg)
{
	int samples = 0;
	int ring_read_index = 0;
	int status;
	int row = 0;
	int last_channel = -1;
	int16_t counts[NUM_CHANNELS];
	int channel;
	FILE *out = fopen(LogFileName, "wb");

	if (out == NULL)
	{
		txtPuts("Error opening file\n");
		apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);
		usleep(5);
		exit(1);
	}
	while (1)
	{
		int buffers_queued;
		sem_getvalue(&ring_sem, &buffers_queued);
		if (terminate && buffers_queued == 0)
			break;
		status = sem_wait(&ring_sem);
		if (terminate == 2)
			break;

		fwrite(ring_buffer[ring_read_index], sizeof(uint32_t), SAMPLES_PER_TRANSFER, out);

		ring_read_index++;
		ring_read_index %= RING_BUFFER_SLOTS;
	};
	fflush(out);
	fclose(out);
	txtPuts("Recorded %d samples on %d channels at rate %f\n", samples, NUM_CHANNELS, SAMPLE_RATE);
}

/* Background thread to acquire data and queue to logger_thread */
void *worker_main(void *arg)
{
	int status;

	// map the DMA destination buffer
	void *mmap_addr = (void *)mmap(NULL, DMA_BUFF_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (mmap_addr == NULL)
	{
		txtPuts("  Worker Thread: mmap_addr is NULL\n");
		return NULL; // was -1
	}

	status = sem_init(&ring_sem, 0, 0);
	if (status)
	{
		txtPuts("  Worker Thread: Unable to init semaphore\n");
		return NULL; // was -1
	}

	pthread_create(&logger_thread, NULL, &log_main, NULL);
	txtPuts("  Worker Thread: launched Logging Thread\n");

	int transfer_count = 0;
	int num_slots;
	int first_slot;
	int data_discarded;
	int buffers_queued;

	do
	{
		// txtPuts("  Worker Thread: About to call apci_dma_data_ready()\n");
		fflush(stdout);
		status = apci_dma_data_ready(fd, 1, &first_slot, &num_slots, &data_discarded);

		if (data_discarded != 0)
		{
			txtPuts("  Worker Thread: first_slot = %d, num_slots = %d, data_discarded = %d\n", first_slot, num_slots, data_discarded);
		}

		if (num_slots == 0)
		{
			// txtPuts("  Worker Thread: No data pending; Waiting for IRQ\n");
			status = apci_wait_for_irq(fd, 1); // thread blocking
			if (status)
			{
				txtPuts("  Worker Thread: Error waiting for IRQ\n");
				break;
			}
			continue;
		}

		// txtPuts("  Worker Thread: data [%d slots] in slot %d\n", num_slots, first_slot);

		if (first_slot + num_slots <= RING_BUFFER_SLOTS)
		{
			// txtPuts("  Worker Thread: Copying contiguous buffers from ring\n");
			memcpy(ring_buffer[first_slot], mmap_addr + (BYTES_PER_TRANSFER * first_slot), BYTES_PER_TRANSFER * num_slots);
			// memcpy(ring_buffer[0], mmap_addr + (BYTES_PER_TRANSFER * 0), BYTES_PER_TRANSFER * 1);
		}
		else
		{
			// txtPuts("  Worker Thread: Copying non-contiguous buffers from ring\n");
			memcpy(ring_buffer[first_slot],
				   mmap_addr + (BYTES_PER_TRANSFER * first_slot),
				   BYTES_PER_TRANSFER * (RING_BUFFER_SLOTS - first_slot));
			memcpy(ring_buffer[0],
				   mmap_addr,
				   BYTES_PER_TRANSFER * (num_slots - (RING_BUFFER_SLOTS - first_slot)));
		}

		__sync_synchronize();

		// txtPuts("  Worker Thread: Telling driver we've taken %d buffer%c\n", num_slots, (num_slots == 1) ? ' ' : 's');
		apci_dma_data_done(fd, 1, num_slots);

		for (int i = 0; i < num_slots; i++)
		{
			sem_post(&ring_sem);
		}

		sem_getvalue(&ring_sem, &buffers_queued);
		if (buffers_queued >= RING_BUFFER_SLOTS)
		{
			txtPuts("  Worker Thread: overran the ring buffer.  Saving the log was too slow. Aborting.\n");
			break;
		}
		transfer_count += num_slots;
		if (!(transfer_count % 1000))
			txtPuts("  Worker Thread: transfer count == %d / %d\n", transfer_count, NumberOfDmaTransfers);
	} while (transfer_count < NumberOfDmaTransfers);
	txtPuts("  Worker Thread: exiting; data acquisition complete.\n");
	terminate = 1;
}

void SetAdcStartRate(int fd, double *Hz)
{
	uint32_t base_clock = AdcBaseClock;
	double targetHz = *Hz;
	uint32_t divisor;
	uint32_t divisor_readback;

	divisor = round(base_clock / targetHz);
	*Hz = base_clock / divisor; /* actual Hz achieved, based on the limitation caused by integer divisors */

	apci_write32(fd, 1, BAR_REGISTER, ofsAdcRateDivisor, divisor);
	apci_read32(fd, 1, BAR_REGISTER, ofsAdcRateDivisor, &divisor_readback);
	txtPuts("  Target ADC Rate is %f\n  Actual rate will be %f (%d/%d)\n", targetHz, *Hz, base_clock, divisor_readback);
}

/* eNET-AIO16-16F Family:  ADC Data Acquisition sample
 * This program acquires ADC data for a source-configurable number of seconds at a source-configurable rate
 * and logs all data into a binary log file (an array of UInt32 (intel-byte-ordered) raw ADC values).
 */
int main(int argc, char **argv)
{
	initscr();

	cbreak();
	if (has_colors() == FALSE)
	{
		endwin();
		printf("Your terminal does not support color\n");
		exit(1);
	}

	start_color();
	init_pair(cINSTRUCT, COLOR_YELLOW, COLOR_BLACK);
	init_pair(cBODY, COLOR_GREEN, COLOR_BLACK);
	init_pair(cDATA, COLOR_WHITE, COLOR_BLACK);

	color_set(cINSTRUCT, NULL);

	txtPuts("\neNET-AIO16-16F Family ADC logging sample.\n");

	HandleMenu();

	txtPuts("\n");

	if (!bRunLogging)
	{
		txtPuts("ESC detected, exiting Logging Sample\n");
		goto err_out;
	}

	SamplesToLog = (SECONDS_TO_LOG * _AdcStartRate);
	NumberOfDmaTransfers = ((__u32)((SamplesToLog + SAMPLES_PER_TRANSFER - 1) / SAMPLES_PER_TRANSFER));

	int i;
	volatile void *mmap_addr;
	int status = 0;
	double rate = _AdcStartRate;
	uint32_t depth_readback;
	uint32_t start_command;
	int ring_write_index = 0;
	int last_status = -1;
	struct timespec dma_delay = {0};
	struct sigaction sigIntHandler;
	char *devicefile;
	time_t timerStart, timerEnd;
	char buffer[30];
	struct tm tm_info;
	terminate = 0;
	dma_delay.tv_nsec = 10;

	if (argc > 1) // if there's a parameter on the command line assume it is the device-to-operate
	{
		devicefile = strdup(argv[1]);
		fd = open(devicefile, O_RDONLY);
		if (fd < 0)
		{
			txtPuts("Device file %s could not be opened. Attempting to use default (%s)\n", devicefile, DEVICEPATH);
		}
	}

	if (fd < 0) // if the parameter-provided device failed to open or wasn't present try the default
	{
		devicefile = strdup(DEVICEPATH);
		// txtPuts("attempting to open default device: %s\n", devicefile);
		fd = open(devicefile, O_RDONLY);
		if (fd < 0)
		{
			txtPuts("Device file %s could not be opened. Add devicefile to parameters, and ensure the APCI driver module is loaded or try sudo?.\n", devicefile);
			exit(1);
		}
	}
	txtPuts("Using device file %s\n", devicefile);

	sigIntHandler.sa_handler = abort_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGABRT, &sigIntHandler, NULL);

	status = apci_dma_transfer_size(fd, 1, RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
	if (status)
	{
		txtPuts("Error setting apci_dma_transfer_size=%d\n", status);
		return -1;
	}

	pthread_create(&worker_thread, NULL, &worker_main, NULL);

	timerStart = time(NULL);

	/* The ADC chip needs 3 init-time conversions performed.  This is being added to APCI.ko but for now ... */
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcSoftwareStart, 0);
	usleep(1);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcSoftwareStart, 0);
	usleep(1);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcSoftwareStart, 0);
	usleep(1);

	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);
	usleep(5);

	uint32_t rev;
	apci_read32(fd, 1, BAR_REGISTER, ofsFPGARevision, &rev);
	txtPuts("FPGA reports Revision = 0x%08X\n", rev);

	apci_write32(fd, 1, BAR_REGISTER, ofsAdcFifoIrqThreshold, FIFO_SIZE);
	// apci_read32(fd, 1, BAR_REGISTER, ofsAdcFifoIrqThreshold, &depth_readback);
	// txtPuts("FAF IRQ Threshold readback from +%02X was %d\n", ofsAdcFifoIrqThreshold, depth_readback);

	apci_write32(fd, 1, BAR_REGISTER, ofsIrqEnables, bmIrqDmaDone | bmIrqFifoAlmostFull);
	SetAdcStartRate(fd, &rate);

	for (int channel = DEFAULT_START_CHANNEL; channel <= DEFAULT_END_CHANNEL; channel++)
		apci_write8(fd, 1, BAR_REGISTER, ofsAdcRange + channel, bmAdcRange_b10V);

	apci_write8(fd, 1, BAR_REGISTER, ofsAdcStartChannel, DEFAULT_START_CHANNEL);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcStopChannel, DEFAULT_END_CHANNEL);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcTriggerOptions, bmAdcTriggerTimer); // starts taking data

	do
	{
		sleep(1);
	} while (!terminate);

	txtPuts("Terminating...waiting for data to spool to disk\n");

	// wait for log data to spool to logging thread
	int buffers_queued;
	do
	{
		sem_getvalue(&ring_sem, &buffers_queued);
		usleep(1000);
	} while (buffers_queued > 0);

err_out: // Once a start has been issued to the card we need to tell it to stop before exiting
	/* put the card back in the power-up state */
	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);
	usleep(5);
	if (bRunLogging)
	{
		terminate = 1;
		sem_post(&ring_sem);
		txtPuts("Done acquiring. Waiting for log file to flush.\n");
		fflush(stdout);
		pthread_join(logger_thread, NULL);

		timerEnd = time(NULL);
		time_t timeDelta = timerEnd - timerStart;

		txtPuts("Finished recording in %ld seconds\n", timeDelta);
		localtime_r(&timerStart, &tm_info);
		strftime(buffer, 30, "Start: %Y-%m-%d %H:%M:%S\n", &tm_info);
		txtPuts(buffer);
		localtime_r(&timerEnd, &tm_info);
		strftime(buffer, 30, "End:   %Y-%m-%d %H:%M:%S\n", &tm_info);
		txtPuts(buffer);

		txtPuts("Done. Data logged to %s\n", LogFileName);
	}
	txtPuts("Press any key to exit\n");
	getch();
	endwin();
}
