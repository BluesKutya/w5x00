/****************************************************************************
 *
 * driver/module.c
 */

/*********************************************
 * Module support
 *********************************************/

#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include "w5x00.h"

static wiz_t gDrvInfo;

static int param_mac_size = 6;

struct spi_device *spi_device = NULL;

static int param_pin_interrupt = W5X00_DEFAULT_PIN_INTERRUPT;
static int param_pin_reset = W5X00_DEFAULT_PIN_RESET;
static int param_select = W5X00_DEFAULT_SELECT;
static unsigned char param_mac[6] = W5X00_DEFAULT_MAC;

module_param(param_pin_interrupt, int, 0);
MODULE_PARM_DESC(param_pin_interrupt, "Interrupt pin number");

module_param(param_pin_reset, int, 0);
MODULE_PARM_DESC(param_pin_reset, "Reset pin number");

module_param(param_select, int, 0);
MODULE_PARM_DESC(param_select, "SPI select number");

module_param_array(param_mac, byte, &param_mac_size, 0);
MODULE_PARM_DESC(param_mac, "MAC Address");

static int w5x00_probe(struct spi_device *spi)
{
	printk("w5x00 probe [int %d, rst %d, sel %d, mac %x:%x:%x:%x:%x:%x]\n",
	       param_pin_interrupt,
		   param_pin_reset,
		   param_select,
		   param_mac[0], param_mac[1], param_mac[2], 
		   param_mac[3], param_mac[4], param_mac[5]);

	printk("chip select before: %d\n", spi->chip_select);

	spi->chip_select = param_select;
	spi_device = spi;

	/* Initial field */
	gDrvInfo.base = 0;
	gDrvInfo.pin_interrupt = param_pin_interrupt;
	gDrvInfo.pin_reset = param_pin_reset;
	gDrvInfo.irq = gpio_to_irq(param_pin_interrupt);

	/* mac address */
	memcpy(gDrvInfo.macaddr, param_mac, 6);
	
	/* initialize device */
	if (wiz_dev_init(&gDrvInfo) < 0) {
		return -EFAULT;
	}

	/* create network interface */
	gDrvInfo.dev = wiznet_drv_create(&gDrvInfo);

	return 0;
}

static int w5x00_remove(struct spi_device *spi)
{
	printk("w5x00 remove\n");

	/* de-initialize device */
	if (gDrvInfo.dev) {
		wiznet_drv_delete(gDrvInfo.dev);
		gDrvInfo.dev = NULL;
	}

	/* free irq */
	wiz_dev_exit(&gDrvInfo);

	spi_device  = NULL;
	return 0;
}

static int spi_device_found(struct device *dev, void *data)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	pr_info(DRV_NAME":      %s %s %dkHz %d bits mode=0x%02X\n",
		spi->modalias, dev_name(dev), spi->max_speed_hz/1000,
		spi->bits_per_word, spi->mode);

	return 0;
}

static void pr_spi_devices(void)
{
	pr_info(DRV_NAME":  SPI devices registered:\n");
	bus_for_each_dev(&spi_bus_type, NULL, NULL, spi_device_found);
}

static struct spi_driver w5x00_driver = {
	.driver = {
		.name = DRV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = w5x00_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	.remove = __devexit_p(w5x00_remove),
#else
	.remove = w5x00_remove,
#endif
};

#ifdef MODULE
static void wiz_device_spi_delete(struct spi_master *master, unsigned cs) {
	struct device *dev;
	char str[32];
	
	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (dev) {
		pr_info(DRV_NAME": Deleting %s\n", str);
		device_del(dev);
	}
}

static int wiz_device_spi_device_register(struct spi_board_info *spi)
{
	struct spi_master *master;
	struct spi_device *spi_dev = NULL;

	
	master = spi_busnum_to_master(spi->bus_num);
	if (!master) {
		pr_err(DRV_NAME":  spi_busnum_to_master(%d) returned NULL\n",
								spi->bus_num);
		return -EINVAL;
	}
	/* make sure it's available */
	wiz_device_spi_delete(master, spi->chip_select);
	spi_dev = spi_new_device(master, spi);
	put_device(&master->dev);
	if (!spi_dev) {
		pr_err(DRV_NAME ":    spi_new_device() returned NULL\n");
		return -EPERM;
	}
	return 0;
}
#else
static int wiz_device_spi_device_register(struct spi_board_info *spi)
{
	return spi_register_board_info(spi, 1);
}
#endif

static int __init
wiz_module_init(void)
{
	int ret;
	struct spi_board_info *spi = &(struct spi_board_info) {
		.modalias = DRV_NAME,
		.max_speed_hz = 26000000,
		.chip_select = 0,
		.bus_num = 0,
		.mode = SPI_MODE_0,
	};
	
	printk(KERN_INFO "%s: %s\n", DRV_NAME, DRV_VERSION);

	pr_spi_devices(); /* print list of registered SPI devices */

	strncpy(spi->modalias, DRV_NAME, SPI_NAME_SIZE);
	spi->chip_select = param_select;
	spi->bus_num = 0; // make dynamic ...

	ret = wiz_device_spi_device_register(spi);
	if (ret) {
		printk( DRV_NAME": failed to register SPI device\n");
		return ret;
	}

	ret = spi_register_driver(&w5x00_driver);
	if(ret < 0)
	{
		printk( DRV_NAME": spi_register_driver failed\n");
		return ret;
	}

	printk("w5x00 spi register succeed\n");

	pr_spi_devices(); /* print list of registered SPI devices */

	return 0;
}

static void __exit 
wiz_module_exit(void)
{
	spi_unregister_driver(&w5x00_driver);
}

module_init(wiz_module_init);
module_exit(wiz_module_exit);

MODULE_AUTHOR("WIZnet");
MODULE_AUTHOR("Olaf Lüke <olaf@tinkerforge.com>");
MODULE_DESCRIPTION("Support for WIZnet w5x00-based MACRAW Mode.");
MODULE_SUPPORTED_DEVICE("WIZnet W5X00 Chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:" DRV_NAME);
MODULE_ALIAS("platform:" DRV_NAME);
