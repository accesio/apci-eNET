/*
 * ACCES I/O APCI Linux driver
 *
 * Copyright 1998-2013 Jimi Damon <jdamon@accesio.com>
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/sort.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/idr.h>
#include <linux/version.h>
#include <linux/list.h>

#include "apci_common.h"
#include "apci_dev.h"
#include "apci_fops.h"

#ifndef __devinit
#define __devinit
#define __devinitdata
#endif

//#define mPCIe_ADIO_IRQStatusAndClearOffset (0x40)
//#define mPCIe_ADIO_IRQEventMask (0xffff0000)
//#define bmADIO_FAFIRQStatus (1 << 20)
//#define bmADIO_DMADoneStatus (1 << 18)
#define bmADIO_DMADoneEnable (1 << 2)
#define bmADIO_ADCTRIGGERStatus (1 << 16)
#define bmADIO_ADCTRIGGEREnable (1 << 0)

#define mPCIe_ADIO_IRQStatusAndClearOffset (0x2C)
#define mPCIe_ADIO_IRQEventMask (0x0000000F)
#define bmADIO_DMADoneStatus (1 << 0)
#define bmADIO_FAFIRQStatus (1 << 1)


/* PCI table construction */
static struct pci_device_id ids[] = {
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_vC3_FPGA),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO16_16F),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO16_16A),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO16_16E),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI16_16F),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI16_16A),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI16_16E),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO12_16A),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO12_16),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AIO12_16E),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI12_16A),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI12_16),
    },
    {
        PCI_DEVICE(A_VENDOR_ID, eNET_AI12_16E),
    },
    {
        0,
    }};
MODULE_DEVICE_TABLE(pci, ids);

struct apci_lookup_table_entry
{
  int board_id;
  int counter;
  char *name;
};

/* Driver naming section */
#define APCI_MAKE_ENTRY(X) \
  {                        \
    X, 0, NAME_##X         \
  }
#define APCI_MAKE_DRIVER_TABLE(...) \
  {                                 \
    __VA_ARGS__                     \
  }
/* acpi_lookup_table */

static int
te_sort(const void *m1, const void *m2)
{
  struct apci_lookup_table_entry *mi1 = (struct apci_lookup_table_entry *)m1;
  struct apci_lookup_table_entry *mi2 = (struct apci_lookup_table_entry *)m2;
  return (mi1->board_id < mi2->board_id ? -1 : mi1->board_id != mi2->board_id);
}

#if 0
static struct apci_lookup_table_entry apci_driver_table[] = {
  { PCIe_IIRO_8, 0 , "iiro8" } ,
  { PCI_DIO_24D, 0 , "pci24d" },
  {0}
};

int APCI_LOOKUP_ENTRY(int x ) {
  switch(x) {
  case PCIe_IIRO_8:
    return 0;
  case PCI_DIO_24D:
    return 1;
  default:
    return -1;
  }
}

#define APCI_TABLE_SIZE sizeof(apci_driver_table) / sizeof(struct apci_lookup_table_entry)
#define APCI_TABLE_ENTRY_SIZE sizeof(struct apci_lookup_table_entry)

#else
static struct apci_lookup_table_entry apci_driver_table[] =
    APCI_MAKE_DRIVER_TABLE(
        APCI_MAKE_ENTRY(eNET_vC3_FPGA),
        APCI_MAKE_ENTRY(eNET_AIO16_16F),
        APCI_MAKE_ENTRY(eNET_AIO16_16A),
        APCI_MAKE_ENTRY(eNET_AIO16_16E),
        APCI_MAKE_ENTRY(eNET_AI16_16F),
        APCI_MAKE_ENTRY(eNET_AI16_16A),
        APCI_MAKE_ENTRY(eNET_AI16_16E),
        APCI_MAKE_ENTRY(eNET_AIO12_16A),
        APCI_MAKE_ENTRY(eNET_AIO12_16),
        APCI_MAKE_ENTRY(eNET_AIO12_16E),
        APCI_MAKE_ENTRY(eNET_AI12_16A),
        APCI_MAKE_ENTRY(eNET_AI12_16),
        APCI_MAKE_ENTRY(eNET_AI12_16E),
    );

#define APCI_TABLE_SIZE sizeof(apci_driver_table) / sizeof(struct apci_lookup_table_entry)
#define APCI_TABLE_ENTRY_SIZE sizeof(struct apci_lookup_table_entry)

void *bsearch(const void *key, const void *base, size_t num, size_t size,
              int (*cmp)(const void *key, const void *elt))
{
  int start = 0, end = num - 1, mid, result;
  if (num == 0)
    return NULL;

  while (start <= end)
  {
    mid = (start + end) / 2;
    result = cmp(key, base + mid * size);
    if (result < 0)
      end = mid - 1;
    else if (result > 0)
      start = mid + 1;
    else
      return (void *)base + mid * size;
  }

  return NULL;
}

int APCI_LOOKUP_ENTRY(int fx)
{
  struct apci_lookup_table_entry driver_entry;
  struct apci_lookup_table_entry *driver_num;
  driver_entry.board_id = fx;
  driver_num = (struct apci_lookup_table_entry *)bsearch(&driver_entry,
                                                         apci_driver_table,
                                                         APCI_TABLE_SIZE,
                                                         APCI_TABLE_ENTRY_SIZE,
                                                         te_sort);

  if (!driver_num)
  {
    return -1;
  }
  else
  {
    return (int)(driver_num - apci_driver_table);
  }
}
#endif

static struct class *class_apci;

/* PCI Driver setup */
static struct pci_driver pci_driver = {
    .name = "acpi",
    .id_table = ids,
    .probe = probe,
    .remove = remove,
};

/* File Operations */
static struct file_operations apci_fops = {
    .read = read_apci,
    .open = open_apci,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
    .ioctl = ioctl_apci,
#else
    .unlocked_ioctl = ioctl_apci,
#endif
    .mmap = mmap_apci,
};

static const int NUM_DEVICES = 4;
static const char APCI_CLASS_NAME[] = "apci";
struct cdev apci_cdev;

static struct class *class_apci;
/* struct apci_my_info *head = NULL; */
struct apci_my_info head;

static int dev_counter = 0;

#define APCI_MAJOR 247

static dev_t apci_first_dev = MKDEV(APCI_MAJOR, 0);

void *
apci_alloc_driver(struct pci_dev *pdev, const struct pci_device_id *id)
{

  struct apci_my_info *ddata = kmalloc(sizeof(struct apci_my_info), GFP_KERNEL);
  int count;
  struct resource *presource;

  if (!ddata)
    return NULL;

  ddata->dac_fifo_buffer = NULL;

  /* Initialize with defaults, fill in specifics later */
  ddata->irq = 0;
  ddata->irq_capable = 0;

  ddata->dma_virt_addr = NULL;

  for (count = 0; count < 6; count++)
  {
    ddata->regions[count].start = 0;
    ddata->regions[count].end = 0;
    ddata->regions[count].flags = 0;
    ddata->regions[count].mapped_address = NULL;
    ddata->regions[count].length = 0;
  }

  ddata->plx_region.start = 0;
  ddata->plx_region.end = 0;
  ddata->plx_region.length = 0;
  ddata->plx_region.mapped_address = NULL;

  ddata->dev_id = id->device;

  spin_lock_init(&(ddata->irq_lock));
  /* ddata->next = NULL; */

  ddata->regions[0].start = pci_resource_start(pdev, 0);
  ddata->regions[0].end = pci_resource_end(pdev, 0);
  ddata->regions[0].flags = pci_resource_flags(pdev, 0);
  ddata->regions[0].length = ddata->regions[0].end - ddata->regions[0].start + 1;
  ddata->regions[1].start = pci_resource_start(pdev, 2);
  ddata->regions[1].end = pci_resource_end(pdev, 2);
  ddata->regions[1].flags = pci_resource_flags(pdev, 2);
  ddata->regions[1].length = ddata->regions[1].end - ddata->regions[1].start + 1;
  ddata->irq = pdev->irq;
  ddata->irq_capable = 1;
  apci_debug("[%04x]: regions[0].start = %08llx\n", ddata->dev_id, ddata->regions[0].start);
  apci_debug("        regions[0].end   = %08llx\n", ddata->regions[0].end);
  apci_debug("        regions[0].length= %08x\n", ddata->regions[0].length);
  apci_debug("        regions[0].flags = %lx\n", ddata->regions[0].flags);
  apci_debug("        regions[1].start = %08llx\n", ddata->regions[1].start);
  apci_debug("        regions[1].end   = %08llx\n", ddata->regions[1].end);
  apci_debug("        regions[1].length= %08x\n", ddata->regions[1].length);
  apci_debug("        regions[1].flags = %lx\n", ddata->regions[1].flags);
  apci_debug("        irq = %d\n", ddata->irq);

  apci_devel("setting up DMA in alloc\n");
  ddata->regions[0].start = pci_resource_start(pdev, 0);
  ddata->regions[0].end = pci_resource_end(pdev, 0);
  ddata->regions[0].flags = pci_resource_flags(pdev, 0);
  ddata->regions[0].length = ddata->regions[0].end - ddata->regions[0].start + 1;
  iounmap(ddata->plx_region.mapped_address);
  apci_debug("regions[0].start = %08llx\n", ddata->regions[0].start);
  apci_debug("regions[0].end   = %08llx\n", ddata->regions[0].end);
  apci_debug("regions[0].length= %08x\n", ddata->regions[0].length);
  apci_debug("regions[0].flags = %lx\n", ddata->regions[0].flags);

  /* request regions */
  for (count = 0; count < 6; count++)
  {
    if (ddata->regions[count].start == 0)
    {
      continue; /* invalid region */
    }
    if (ddata->regions[count].flags & IORESOURCE_IO)
    {

      apci_debug("requesting io region start=%08llx,len=%d\n", ddata->regions[count].start, ddata->regions[count].length);
      presource = request_region(ddata->regions[count].start, ddata->regions[count].length, "apci");
    }
    else
    {
      apci_debug("requesting mem region start=%08llx,len=%d\n", ddata->regions[count].start, ddata->regions[count].length);
      presource = request_mem_region(ddata->regions[count].start, ddata->regions[count].length, "apci");
      if (presource != NULL)
      {
        ddata->regions[count].mapped_address = ioremap(ddata->regions[count].start, ddata->regions[count].length);
      }
    }

    if (presource == NULL)
    {
      /* If we failed to allocate the region. */
      count--;

      while (count >= 0)
      {
        if (ddata->regions[count].start != 0)
        {
          if (ddata->regions[count].flags & IORESOURCE_IO)
          {
            /* if it is a valid region */
            release_region(ddata->regions[count].start, ddata->regions[count].length);
          }
          else
          {
            iounmap(ddata->regions[count].mapped_address);

            release_region(ddata->regions[count].start, ddata->regions[count].length);
          }
        }
      }
      goto out_alloc_driver;
    }
  }

  // cards where we support DMA. So far just the mPCIe_AI*(_proto) cards
  spin_lock_init(&(ddata->dma_data_lock));
  ddata->plx_region = ddata->regions[0];
  apci_debug("DMA spinlock init\n");

  return ddata;

out_alloc_driver:
  kfree(ddata);
  return NULL;
}

/**
 * @desc Removes all of the drivers from this module
 *       before cleanup
 */
void apci_free_driver(struct pci_dev *pdev)
{
  struct apci_my_info *ddata = pci_get_drvdata(pdev);
  int count;

  apci_devel("Entering free driver.\n");

  if (ddata->plx_region.flags & IORESOURCE_IO)
  {
    apci_debug("releasing memory of %08llx , length=%d\n", ddata->plx_region.start, ddata->plx_region.length);
    release_region(ddata->plx_region.start, ddata->plx_region.length);
  }
  else
  {
    // iounmap(ddata->plx_region.mapped_address);
    // release_mem_region(ddata->plx_region.start, ddata->plx_region.length);
  }

  for (count = 0; count < 6; count++)
  {
    if (ddata->regions[count].start == 0)
    {
      continue; /* invalid region */
    }
    if (ddata->regions[count].flags & IORESOURCE_IO)
    {
      release_region(ddata->regions[count].start, ddata->regions[count].length);
    }
    else
    {
      iounmap(ddata->regions[count].mapped_address);
      release_mem_region(ddata->regions[count].start, ddata->regions[count].length);
    }
  }

  if (NULL != ddata->dac_fifo_buffer)
  {
    kfree(ddata->dac_fifo_buffer);
  }
  kfree(ddata);
  apci_debug("Completed freeing driver.\n");
}

static void apci_class_dev_unregister(struct apci_my_info *ddata)
{
  struct apci_lookup_table_entry *obj = &apci_driver_table[APCI_LOOKUP_ENTRY((int)ddata->id->device)];
  apci_devel("entering apci_class_dev_unregister\n");
  /* ddata->dev = device_create(class_apci, &ddata->pci_dev->dev , apci_first_dev + id, NULL, "apci/%s_%d", obj->name, obj->counter ++ ); */

  apci_devel("entering apci_class_dev_unregister.\n");
  if (ddata->dev == NULL)
    return;

  device_unregister(ddata->dev);
  obj->counter--;
  dev_counter--;

  apci_devel("leaving apci_class_dev_unregister.\n");
}

static int __devinit
apci_class_dev_register(struct apci_my_info *ddata)
{
  int ret;
  struct apci_lookup_table_entry *obj = &apci_driver_table[APCI_LOOKUP_ENTRY((int)ddata->id->device)];
  apci_devel("entering apci_class_dev_register\n");

  ddata->dev = device_create(class_apci, &ddata->pci_dev->dev, apci_first_dev + dev_counter, NULL, "apci/%s_%d", obj->name, obj->counter);

  /* add pointer to the list of all ACCES I/O products */

  if (IS_ERR(ddata->dev))
  {
    apci_error("Error creating device");
    ret = PTR_ERR(ddata->dev);
    ddata->dev = NULL;
    return ret;
  }
  obj->counter++;
  dev_counter++;
  apci_devel("leaving apci_class_dev_register\n");
  return 0;
}

irqreturn_t apci_interrupt(int irq, void *dev_id)
{
  struct apci_my_info *ddata;
  bool notify_user = true;
  uint32_t irq_event = 0;

  ddata = (struct apci_my_info *)dev_id;

  // If this is a DMA DONE IRQ then tell the card to write to the next DMA buffer
  irq_event = ioread32(ddata->regions[BAR_REGISTER].mapped_address + ofsIrqStatus_Clear);
  if ((irq_event & 0x0000000F) == 0)
  {
    printk("%s NOT ME!\n", __FUNCTION__);
    return IRQ_NONE;
  }

  apci_devel("ISR: mPCIe-AxIO irq_event\n");

  if (irq_event & (0x01)) // DMA DONE IRQ, configure next buffer
  {
    dma_addr_t base = ddata->dma_addr;
    spin_lock(&(ddata->dma_data_lock));
    if (ddata->dma_first_valid == -1)
    {
      ddata->dma_first_valid = 0;
    }

    ddata->dma_last_buffer++;
    ddata->dma_last_buffer %= ddata->dma_num_slots;

    if (ddata->dma_last_buffer == ddata->dma_first_valid)
    {
      apci_error("ISR: data discarded");
      ddata->dma_last_buffer--;
      if (ddata->dma_last_buffer < 0)
        ddata->dma_last_buffer = ddata->dma_num_slots - 1;
      ddata->dma_data_discarded++;
    }
    spin_unlock(&(ddata->dma_data_lock));
    base += ddata->dma_slot_size * ddata->dma_last_buffer;

    iowrite32(base & 0xffffffff, ddata->regions[BAR_DMA].mapped_address + ofsDmaAddr32);
    iowrite32(base >> 32, ddata->regions[BAR_DMA].mapped_address + ofsDmaAddr64);
    iowrite32(ddata->dma_slot_size, ddata->regions[BAR_DMA].mapped_address + ofsDmaSize);
    iowrite32(DmaStart, ddata->regions[BAR_DMA].mapped_address + ofsDmaControl);
  }

  // clear whatever IRQ occurred and retain enabled IRQ sources
  iowrite32(irq_event, ddata->regions[BAR_REGISTER].mapped_address + ofsIrqStatus_Clear);
  apci_debug("ISR: irq_event = 0x%x, depth = 0x%x, IRQStatus = 0x%x\n", irq_event, ioread32(ddata->regions[1].mapped_address + 0x24), ioread32(ddata->regions[1].mapped_address + 0x2C));
  irq_event = ioread32(ddata->regions[BAR_REGISTER].mapped_address + ofsIrqStatus_Clear);

  /* Check to see if an application is actually waiting for an IRQ. If yes,
   * then we need to wake the queue associated with this device.
   * Right now it is not possible for any other code sections that access
   * the critical data to interrupt us so we won't disable other IRQs.
   */
  if (notify_user)
  {
    spin_lock(&(ddata->irq_lock));

    if (ddata->waiting_for_irq)
    {
      ddata->waiting_for_irq = 0;
      spin_unlock(&(ddata->irq_lock));
      wake_up_interruptible(&(ddata->wait_queue));
    }
    else
    {
      spin_unlock(&(ddata->irq_lock));
    }
  }
  apci_devel("ISR: IRQ Handled\n");
  return IRQ_HANDLED;
}

void remove(struct pci_dev *pdev)
{
  struct apci_my_info *ddata = pci_get_drvdata(pdev);
  struct apci_my_info *child;
  struct apci_my_info *_temp;
  apci_devel("entering remove\n");

  spin_lock(&(ddata->irq_lock));

  if (ddata->irq_capable)
    free_irq(pdev->irq, ddata);

  spin_unlock(&(ddata->irq_lock));

  if (ddata->dma_virt_addr != NULL)
  {
    dma_free_coherent(&(ddata->pci_dev->dev),
                      ddata->dma_num_slots * ddata->dma_slot_size,
                      ddata->dma_virt_addr,
                      ddata->dma_addr);
  }

  apci_class_dev_unregister(ddata);

  cdev_del(&ddata->cdev);

  spin_lock(&head.driver_list_lock);
  list_for_each_entry_safe(child, _temp, &head.driver_list, driver_list)
  {
    if (child == ddata)
    {
      apci_debug("Removing node with address %p\n", child);
      list_del(&child->driver_list);
    }
  }
  spin_unlock(&head.driver_list_lock);

  apci_free_driver(pdev);

  apci_devel("leaving remove\n");
}

/*
 * @note Adds the driver to the list of all ACCES I/O Drivers
 * that are supported by this driver
 */
int probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
  struct apci_my_info *ddata;
  struct apci_my_info *_temp;
  struct apci_my_info *child;
  int ret;
  apci_devel("entering probe\n");

  if (pci_enable_device(pdev))
  {
    return -ENODEV;
  }

  ddata = (struct apci_my_info *)apci_alloc_driver(pdev, id);
  if (ddata == NULL)
  {
    return -ENOMEM;
  }
  /* Setup actual device driver items */
  ddata->nchannels = APCI_NCHANNELS;
  ddata->pci_dev = pdev;
  ddata->id = id;
  ddata->is_pcie = (ddata->plx_region.length >= 0x100 ? 1 : 0);
  apci_debug("Is device PCIE : %d\n", ddata->is_pcie);

  /* Spin lock init stuff */

  /* Request Irq */
  if (ddata->irq_capable)
  {
    apci_debug("Requesting Interrupt, %u\n", (unsigned int)ddata->irq);
    ret = request_irq((unsigned int)ddata->irq,
                      apci_interrupt,
                      IRQF_SHARED,
                      "apci",
                      ddata);
    if (ret)
    {
      apci_error("error requesting IRQ %u\n", ddata->irq);
      ret = -ENOMEM;
      goto exit_free;
    }

    // TODO: Fix this when HW is available to test MEM version
    if (ddata->is_pcie)
    {
      if (ddata->dev_id != 0xC0E8)
      {
        if (ddata->plx_region.flags & IORESOURCE_IO)
        {
          outb(0x9, ddata->plx_region.start + 0x69);
        }
        else
        {
          apci_debug("Enabling IRQ MEM/IO plx region\n");
          iowrite8(0x9, ddata->plx_region.mapped_address + 0x69);
        }
      }
    }

    init_waitqueue_head(&(ddata->wait_queue));
  }

  /* add to sysfs */

  cdev_init(&ddata->cdev, &apci_fops);
  ddata->cdev.owner = THIS_MODULE;

  ret = cdev_add(&ddata->cdev, apci_first_dev + dev_counter, 1);
  if (ret)
  {
    apci_error("error registering Driver %d", dev_counter);
    goto exit_irq;
  }

  /* add to the list of devices supported */
  spin_lock(&head.driver_list_lock);
  list_add(&ddata->driver_list, &head.driver_list);
  spin_unlock(&head.driver_list_lock);

  pci_set_drvdata(pdev, ddata);
  ret = apci_class_dev_register(ddata);

  if (ret)
    goto exit_pci_setdrv;

  //Need to prime the ADC chip with three reads that are ignored
  //on the eNET device
  {
    int i;
    for (i = 0; i < 3; i++)
    {
      iowrite8(0, ddata->regions[BAR_REGISTER].mapped_address + ofsAdcSoftwareStart);
      udelay(1);
    }
  }

  apci_debug("Added driver %d\n", dev_counter - 1);
  apci_debug("Value of irq is %d\n", pdev->irq);
  return 0;

exit_pci_setdrv:
  /* Routine to remove the driver from the list */
  spin_lock(&head.driver_list_lock);
  list_for_each_entry_safe(child, _temp, &head.driver_list, driver_list)
  {
    if (child == ddata)
    {
      apci_debug("Would delete address %p\n", child);
    }
  }
  spin_unlock(&head.driver_list_lock);

  pci_set_drvdata(pdev, NULL);
  cdev_del(&ddata->cdev);
exit_irq:
  if (ddata->irq_capable)
    free_irq(pdev->irq, ddata);
exit_free:
  apci_free_driver(pdev);
  return ret;
}

/* Configure the default /dev/{devicename} permissions */
static char *apci_devnode(struct device *dev, umode_t *mode)
{
  if (!mode)
    return NULL;
  *mode = APCI_DEFAULT_DEVFILE_MODE;
  return NULL;
}

int __init
apci_init(void)
{
  void *ptr_err;
  int result;
  int ret;
  dev_t dev = MKDEV(APCI_MAJOR, 0);
  apci_debug("performing init duties\n");
  spin_lock_init(&head.driver_list_lock);
  INIT_LIST_HEAD(&head.driver_list);
  sort(apci_driver_table, APCI_TABLE_SIZE, APCI_TABLE_ENTRY_SIZE, te_sort, NULL);

  ret = alloc_chrdev_region(&apci_first_dev, 0, MAX_APCI_CARDS, APCI);
  if (ret)
  {
    apci_error("Unable to allocate device numbers");
    return ret;
  }

  /* Create the sysfs entry for this */
  class_apci = class_create(THIS_MODULE, APCI_CLASS_NAME);
  if (IS_ERR(ptr_err = class_apci))
    goto err;
  class_apci->devnode = apci_devnode; // set device file permissions

  cdev_init(&apci_cdev, &apci_fops);
  apci_cdev.owner = THIS_MODULE;
  apci_cdev.ops = &apci_fops;

  result = cdev_add(&apci_cdev, dev, 1);

#ifdef TEST_CDEV_ADD_FAIL
  result = -1;
  if (result == -1)
  {
    apci_error("Going to delete device.\n");
    cdev_del(&apci_cdev);
    apci_error("Deleted device.\n");
  }
#endif

  if (result < 0)
  {
    apci_error("cdev_add failed in apci_init");
    goto err;
  }

  /* needed to get the probe and remove to be called */
  result = pci_register_driver(&pci_driver);

  return 0;
err:

  apci_error("Unregistering chrdev_region.\n");

  unregister_chrdev_region(MKDEV(APCI_MAJOR, 0), 1);

  class_destroy(class_apci);
  return PTR_ERR(ptr_err);
}

static void __exit apci_exit(void)
{
  apci_debug("performing exit duties\n");
  pci_unregister_driver(&pci_driver);
  cdev_del(&apci_cdev);
  unregister_chrdev_region(MKDEV(APCI_MAJOR, 0), 1);
  class_destroy(class_apci);
}

module_init(apci_init);
module_exit(apci_exit);

MODULE_AUTHOR("John Hentges <JHentges@accesio.com>");
MODULE_LICENSE("GPL");
