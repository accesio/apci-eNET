#ifndef APCI_DEV_H
#define APCI_DEV_H


#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/version.h>

#include "apci_common.h"

#define eNET_vC3_FPGA 0xC2EC

#define eNET_AIO16_16F 0xCFEC
#define eNET_AIO16_16A 0xCFED
#define eNET_AIO16_16E 0xCFEE
#define eNET_AI16_16F 0x8FEC
#define eNET_AI16_16A 0x8FED
#define eNET_AI16_16E 0x8FEE
#define eNET_AIO12_16A 0xCF5C
#define eNET_AIO12_16 0xCF5D
#define eNET_AIO12_16E 0xCF5E
#define eNET_AI12_16A 0x8F5C
#define eNET_AI12_16 0x8F5D
#define eNET_AI12_16E 0x8F5E

#define NAME_eNET_vC3_FPGA              "enet_vc3_fpga"

#define NAME_eNET_AIO16_16F             "enet_aio16_16f"
#define NAME_eNET_AIO16_16A             "enet_aio16_16a"
#define NAME_eNET_AIO16_16E             "enet_aio16_16e"
#define NAME_eNET_AI16_16F              "enet_ai16_16f"
#define NAME_eNET_AI16_16A              "enet_ai16_16a"
#define NAME_eNET_AI16_16E              "enet_ai16_16e"
#define NAME_eNET_AIO12_16A             "enet_aio12_16a"
#define NAME_eNET_AIO12_16              "enet_aio12_16"
#define NAME_eNET_AIO12_16E             "enet_aio12_16e"
#define NAME_eNET_AI12_16A              "enet_ai12_16a"
#define NAME_eNET_AI12_16               "enet_ai12_16"
#define NAME_eNET_AI12_16E              "enet_ai12_16e"

enum ADDRESS_TYPE {INVALID = 0, IO, MEM};
typedef enum ADDRESS_TYPE address_type;

typedef struct {
    __u64 start;
    __u64 end;
    __u32 length;
    unsigned long flags;
    void *mapped_address;
} io_region;

struct apci_board {
    struct device *dev;
};

struct apci_my_info {
     __u32 dev_id;
     io_region regions[6], plx_region;
     const struct pci_device_id *id;
     int is_pcie;
     int irq;
     int irq_capable; /* is the card even able to generate irqs? */
     int waiting_for_irq; /* boolean for if the user has requested an IRQ */
     int irq_cancelled; /* boolean for if the user has cancelled the wait */

     /* List of drivers */
     struct list_head driver_list;
     spinlock_t driver_list_lock;

     wait_queue_head_t wait_queue;
     spinlock_t irq_lock;


     struct cdev cdev;

     struct pci_dev *pci_dev;

     int nchannels;

     struct apci_board boards[APCI_NCHANNELS];

     struct device *dev;

     dma_addr_t dma_addr;
     void *dma_virt_addr;
     int dma_last_buffer; //last dma started to fill
     int dma_first_valid; //first buffer containing valid data
     int dma_num_slots;
     size_t dma_slot_size;
     int dma_data_discarded;
     spinlock_t dma_data_lock;

     void *dac_fifo_buffer;
};

int probe(struct pci_dev *dev, const struct pci_device_id *id);
void remove(struct pci_dev *dev);
void delete_driver(struct pci_dev *dev);



#endif
