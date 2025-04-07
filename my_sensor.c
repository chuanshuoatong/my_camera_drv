#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>

// 定义 TAG
#define TAG "[my_sensor_drv]: "

// 封装打印函数
#define sensor_info(fmt, ...) \
    pr_info(TAG "%s " fmt, __func__, ##__VA_ARGS__)

#define sensor_err(fmt, ...) \
    pr_err(TAG "%s " fmt, __func__, ##__VA_ARGS__)


static int my_sensor_probe(struct platform_device *pdev)
{
    sensor_info("\n");
    return 0;
}

static int my_sensor_remove(struct platform_device *pdev)
{
    sensor_info("\n");
    return 0;
}

static const struct of_device_id my_sensor_of_match_table[] = {
    {.compatible = "mycompany,my_sensor"},
    { },
};

static struct platform_driver my_sensor_driver = {
    .driver = {
        .name = "my_sensor",
        .owner = THIS_MODULE,
        .of_match_table = my_sensor_of_match_table,
    },
    .probe  = my_sensor_probe,
    .remove = my_sensor_remove,
};

module_platform_driver(my_sensor_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SENSOR Driver");