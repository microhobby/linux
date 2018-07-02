#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct clap_sensor {
	struct input_dev *idev;
	struct device *dev;
};

static irqreturn_t clap_sensor_irq(int irq, void *_clap)
{
	struct clap_sensor *clap = _clap;
	int val;

	kobject_uevent(&clap->dev->kobj, KOBJ_CHANGE);
	dev_info(clap->dev, "CLAPED\n");

	return IRQ_HANDLED;
}

static int clap_sensor_probe(struct platform_device *pdev)
{
	struct clap_sensor *clap;
	int irq;
	int err;

	/* memory allocation */
	clap = devm_kmalloc(&pdev->dev, sizeof(*clap), GFP_KERNEL);
	if (!clap) {
		dev_err(&pdev->dev, "CLAP malloc error %d\n", -ENOMEM);
		return -ENOMEM;
	}

	clap->idev = devm_input_allocate_device(&pdev->dev);
	if (!clap->idev) {
		dev_err(&pdev->dev, "CLAP clap->idev malloc error %d\n",
				-ENOMEM);
		return -ENOMEM;
	}

	/* initializations */
	clap->dev = &pdev->dev;
	clap->idev->name = "clap-sensor";
	clap->idev->phys = "clap-sensor/input0";
	clap->idev->dev.parent = clap->dev;
	input_set_capability(clap->idev, EV_SND, SND_CLICK);

	dev_info(clap->dev, "initializing CLAP\n");

	/* interrupt */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform IRQ request failed: %d\n", irq);
		return irq;
	}

	err = devm_request_threaded_irq(&pdev->dev, irq, NULL,
		clap_sensor_irq, IRQF_ONESHOT, "clap-sensor", clap);
	if (err < 0) {
		dev_err(&pdev->dev, "IRQ request failed: %d\n", err);
		return err;
	}

	/* put dev in input subsystem */
	err = input_register_device(clap->idev);
	if (err) {
		dev_err(&pdev->dev, "Input register failed: %d\n", err);
		return err;
	}

	device_init_wakeup(&pdev->dev, true);

	dev_info(clap->dev, "CLAP Probed\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id clap_sensor_dt_match_table[] = {
	{ .compatible = "texugo,clap-sensor" },
	{ },
};
MODULE_DEVICE_TABLE(of, clap_sensor_dt_match_table);
#endif

static struct platform_driver clap_sensor_driver = {
	.probe = clap_sensor_probe,
	.driver = {
		.name = "clap-sensor",
		.of_match_table = of_match_ptr(clap_sensor_dt_match_table),
	},
};
module_platform_driver(clap_sensor_driver);

MODULE_AUTHOR("Matheus Castello <matheus@castello.eng.br>");
MODULE_DESCRIPTION("Driver for generic Clap Sensor from an op amp output");
MODULE_LICENSE("GPL v2");
