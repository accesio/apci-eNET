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

#include "apcilib.h"
#include "eNET-AIO.h"

#define DEVICEPATH "/dev/apci/pcie_adio16_16f_0"

/* the following set of #defines configure what the sample does; feel free to change these */
#define SAMPLE_RATE 1000000.0 /* Hz. Note: This is the overall sample rate, sample rate of each channel is SAMPLE_RATE / CHANNEL_COUNT */
#define LOG_FILE_NAME "samples.bin"
#define SECONDS_TO_LOG 10.0
#define START_CHANNEL 0
#define END_CHANNEL 15
#define ADC_RANGE (bmSingleEnded | bmAdcRange_u1V) // the sample makes all channels the same range but the device is more flexible

/* The rest of this is internal for the sample to use and should not be changed until you understand it all */
double Hz;
uint8_t CHANNEL_COUNT = END_CHANNEL - START_CHANNEL + 1;
#define NUM_CHANNELS (2 * CHANNEL_COUNT)
#define AMOUNT_OF_SAMPLES_TO_LOG (SECONDS_TO_LOG * SAMPLE_RATE * 2)

#define RING_BUFFER_SLOTS 16
static uint32_t ring_buffer[RING_BUFFER_SLOTS][SAMPLES_PER_TRANSFER];
static sem_t ring_sem;
volatile static int terminate;

#define DMA_BUFF_SIZE (BYTES_PER_TRANSFER * RING_BUFFER_SLOTS)
#define NUMBER_OF_DMA_TRANSFERS ((__u32)((AMOUNT_OF_SAMPLES_TO_LOG + SAMPLES_PER_TRANSFER - 1) / SAMPLES_PER_TRANSFER))

int fd = -1;
pthread_t logger_thread;
pthread_t worker_thread;

void abort_handler(int s)
{
	printf("Caught signal %d\n", s);
	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);usleep(5);

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
	FILE *out = fopen(LOG_FILE_NAME, "wb");

	if (out == NULL)
	{
		printf("Error opening file\n");
		apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything);usleep(5);
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
	printf("Recorded %d samples on %d channels at rate %f\n", samples, NUM_CHANNELS, SAMPLE_RATE);
}

/* Background thread to acquire data and queue to logger_thread */
void *worker_main(void *arg)
{
	int status;

	// map the DMA destination buffer
	void *mmap_addr = (void *)mmap(NULL, DMA_BUFF_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (mmap_addr == NULL)
	{
		printf("  Worker Thread: mmap_addr is NULL\n");
		return NULL; // was -1
	}

	status = sem_init(&ring_sem, 0, 0);
	if (status)
	{
		printf("  Worker Thread: Unable to init semaphore\n");
		return NULL; // was -1
	}

	pthread_create(&logger_thread, NULL, &log_main, NULL);
	printf("  Worker Thread: launched Logging Thread\n");

	int transfer_count = 0;
	int num_slots;
	int first_slot;
	int data_discarded;
	int buffers_queued;

	do
	{
		// printf("  Worker Thread: About to call apci_dma_data_ready()\n");
		fflush(stdout);
		status = apci_dma_data_ready(fd, 1, &first_slot, &num_slots, &data_discarded);

		if (data_discarded != 0)
		{
			printf("  Worker Thread: first_slot = %d, num_slots = %d, data_discarded = %d\n", first_slot, num_slots, data_discarded);
		}

		if (num_slots == 0)
		{
			// printf("  Worker Thread: No data pending; Waiting for IRQ\n");
			status = apci_wait_for_irq(fd, 1); // thread blocking
			if (status)
			{
				printf("  Worker Thread: Error waiting for IRQ\n");
				break;
			}
			continue;
		}

		// printf("  Worker Thread: data [%d slots] in slot %d\n", num_slots, first_slot);

		if (first_slot + num_slots <= RING_BUFFER_SLOTS)
		{
			// printf("  Worker Thread: Copying contiguous buffers from ring\n");
			memcpy(ring_buffer[first_slot], mmap_addr + (BYTES_PER_TRANSFER * first_slot), BYTES_PER_TRANSFER * num_slots);
			// memcpy(ring_buffer[0], mmap_addr + (BYTES_PER_TRANSFER * 0), BYTES_PER_TRANSFER * 1);
		}
		else
		{
			// printf("  Worker Thread: Copying non-contiguous buffers from ring\n");
			memcpy(ring_buffer[first_slot],
				   mmap_addr + (BYTES_PER_TRANSFER * first_slot),
				   BYTES_PER_TRANSFER * (RING_BUFFER_SLOTS - first_slot));
			memcpy(ring_buffer[0],
				   mmap_addr,
				   BYTES_PER_TRANSFER * (num_slots - (RING_BUFFER_SLOTS - first_slot)));
		}

		__sync_synchronize();

		// printf("  Worker Thread: Telling driver we've taken %d buffer%c\n", num_slots, (num_slots == 1) ? ' ' : 's');
		apci_dma_data_done(fd, 1, num_slots);

		for (int i = 0; i < num_slots; i++)
		{
			sem_post(&ring_sem);
		}

		sem_getvalue(&ring_sem, &buffers_queued);
		if (buffers_queued >= RING_BUFFER_SLOTS)
		{
			printf("  Worker Thread: overran the ring buffer.  Saving the log was too slow. Aborting.\n");
			break;
		}
		transfer_count += num_slots;
		if (!(transfer_count % 1000))
			printf("  Worker Thread: transfer count == %d / %d\n", transfer_count, NUMBER_OF_DMA_TRANSFERS);
	} while (transfer_count < NUMBER_OF_DMA_TRANSFERS);
	printf("  Worker Thread: exiting; data acquisition complete.\n");
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
	printf("  Target ADC Rate is %f\n  Actual rate will be %f (%d/%d)\n", targetHz, *Hz, base_clock, divisor_readback);
}

/* eNET-AIO16-16F Family:  ADC Data Acquisition sample
 * This program acquires ADC data for a source-configurable number of seconds at a source-configurable rate
 * and logs all data into a binary log file (an array of UInt32 (intel-byte-ordered) raw ADC values).
 */
int main(int argc, char **argv)
{
	int i;
	volatile void *mmap_addr;
	int status = 0;
	double rate = SAMPLE_RATE;
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

	printf("\nmPCIe-ADIO16-16F Family ADC logging sample.\n");
	printf("Source Configured to take CH%d to CH%d\nat %f rate\n", START_CHANNEL, END_CHANNEL, rate);
	printf("Logging raw data to %s\nAll channels gaincode=%01X\n", LOG_FILE_NAME, ADC_RANGE);

	if (argc > 1) // if there's a parameter on the command line assume it is the device-to-operate
	{
		devicefile = strdup(argv[1]);
		fd = open(devicefile, O_RDONLY);
		if (fd < 0)
		{
			printf("Device file %s could not be opened. Attempting to use default (%s)\n", devicefile, DEVICEPATH);
		}
	}

	if (fd < 0) // if the parameter-provided device failed to open or wasn't present try the default
	{
		devicefile = strdup(DEVICEPATH);
		printf("attempting to open default device: %s\n", devicefile);
		fd = open(devicefile, O_RDONLY);
		if (fd < 0)
		{
			printf("Device file %s could not be opened. Please ensure the APCI driver module is loaded or try sudo?.\n", devicefile);
			exit(1);
		}
	}
	printf("Using device file %s\n", devicefile);

	sigIntHandler.sa_handler = abort_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGABRT, &sigIntHandler, NULL);

	status = apci_dma_transfer_size(fd, 1, RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
	if (status)
	{
		printf("Error setting apci_dma_transfer_size=%d\n", status);
		return -1;
	}

	pthread_create(&worker_thread, NULL, &worker_main, NULL);

	timerStart = time(NULL);

	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything); usleep(5);

	uint32_t rev;
	apci_read32(fd, 1, BAR_REGISTER, ofsFPGARevision, &rev);
	printf("FPGA reports Revision = 0x%08X\n", rev);

	apci_write32(fd, 1, BAR_REGISTER, ofsAdcFifoIrqThreshold, FIFO_SIZE);
	apci_read32(fd, 1, BAR_REGISTER, ofsAdcFifoIrqThreshold, &depth_readback);
	printf("FAF IRQ Threshold readback from +%02X was %d\n", ofsAdcFifoIrqThreshold, depth_readback);

	apci_write32(fd, 1, BAR_REGISTER, ofsIrqEnables, bmIrqDmaDone | bmIrqFifoAlmostFull);
	SetAdcStartRate(fd, &rate);

	for (int channel = START_CHANNEL; channel <= END_CHANNEL; channel++)
		apci_write8(fd, 1, BAR_REGISTER, ofsAdcRange + channel, bmAdcRange_b10V);

	apci_write8(fd, 1, BAR_REGISTER, ofsAdcStartChannel, START_CHANNEL);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcStopChannel, END_CHANNEL);
	apci_write8(fd, 1, BAR_REGISTER, ofsAdcTriggerOptions, bmAdcTriggerTimer);

	do
	{
		sleep(1);
	} while (!terminate);

	printf("Terminating...waiting for data to spool to disk\n");

	// wait for log data to spool to logging thread
	int buffers_queued;
	do
	{
		sem_getvalue(&ring_sem, &buffers_queued);
		usleep(1000);
	} while (buffers_queued > 0);

err_out: // Once a start has been issued to the card we need to tell it to stop before exiting
	/* put the card back in the power-up state */
	apci_write8(fd, 1, BAR_REGISTER, ofsReset, bmResetEverything); usleep(5);

	terminate = 1;
	sem_post(&ring_sem);
	printf("Done acquiring. Waiting for log file to flush.\n");
	fflush(stdout);
	pthread_join(logger_thread, NULL);

	timerEnd = time(NULL);
	time_t timeDelta = timerEnd - timerStart;

	printf("Finished recording in %ld seconds\n", timeDelta);
	localtime_r(&timerStart, &tm_info);
	strftime(buffer, 30, "Start: %Y-%m-%d %H:%M:%S", &tm_info);
	puts(buffer);
	localtime_r(&timerEnd, &tm_info);
	strftime(buffer, 30, "End:   %Y-%m-%d %H:%M:%S", &tm_info);
	puts(buffer);

	printf("Done. Data logged to %s\n", LOG_FILE_NAME);
}
