// include API header for the new interface

#include <linux/gpio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define DEV_NAME "/dev/gpiochip1"
#define FPGA_RELOAD_GPIO_NUMBER 45

int inform(int fd);

void main(void)
{
    int fd, ret;
    struct gpiochip_info info;


    fd = open(DEV_NAME, O_RDONLY);
    if (fd < 0)
    {
        printf("Unabled to open %s: %s", DEV_NAME, strerror(errno));
        return;
    }

    // Query GPIO chip information
    ret = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
    if (ret == -1)
    {
        printf("Unable to get chip info from ioctl: %s", strerror(errno));
        close(fd);
        return;
    }
    printf("Chip name: %s\n", info.name);
    printf("Chip label: %s\n", info.label);
    printf("Number of lines: %d\n", info.lines);

    inform(fd);

    struct gpiohandle_request rq;
    rq.lineoffsets[0] = FPGA_RELOAD_GPIO_NUMBER;
    rq.lines = 1;
    rq.flags = GPIOHANDLE_REQUEST_OUTPUT;

    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &rq);
    if (ret == -1)
    {
        printf("Unable to get line handle from ioctl : %s", strerror(errno));
        close(fd);
        return;
    }
    inform(fd);
    printf("Setting GPIO45 HIGH\n");
    struct gpiohandle_data data;
    data.values[0] = 1; // HIGH offset 46
    ret = ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1)
    {
        printf("Unable to set line value using ioctl : %s", strerror(errno));
    }

    sleep(1);
    inform(fd);
    printf("Setting GPIO45 LOW\n");
    data.values[0] = 0; // LOW offset 46
    ret = ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1)
    {
        printf("Unable to set line value using ioctl : %s", strerror(errno));
    }

    sleep(1);

    inform(fd);

    close(rq.fd);
    rq.lineoffsets[0] = FPGA_RELOAD_GPIO_NUMBER;
    rq.lines = 1;
    rq.flags = GPIOHANDLE_REQUEST_INPUT;

    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &rq);
    if (ret == -1)
    {
        printf("Unable to line handle from ioctl : %s", strerror(errno));
        close(fd);
        return;
    }

    inform(fd);

    close(fd);
}

int inform(int fd)
{
    struct gpioline_info line_info;
    int ret, i = FPGA_RELOAD_GPIO_NUMBER;

    line_info.line_offset = i;
    ret = ioctl(fd, GPIO_GET_LINEINFO_IOCTL, &line_info);
    if (ret == -1)
    {
        printf("Unable to get line info from offset %d: %s", i, strerror(errno));
        return errno;
    }
    else
    {
        printf("offset: % 2d, Flags:[%14s][%14s][%14s][%14s][%14s]\n",
            i,
            (line_info.flags & GPIOLINE_FLAG_IS_OUT) ? "OUTPUT" : "INPUT",
            (line_info.flags & GPIOLINE_FLAG_ACTIVE_LOW) ? "ACTIVE_LOW" : "ACTIVE_HIGH",
            (line_info.flags & GPIOLINE_FLAG_OPEN_DRAIN) ? "OPEN_DRAIN" : "...",
            (line_info.flags & GPIOLINE_FLAG_OPEN_SOURCE) ? "OPENSOURCE" : "...",
            (line_info.flags & GPIOLINE_FLAG_KERNEL) ? "KERNEL" : "");
    }

    return 0;
}
