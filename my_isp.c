#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>

// 定义 TAG
#define TAG "[my_isp_drv]: "

// 封装打印函数
#define isp_info(fmt, ...) \
    pr_info(TAG "%s " fmt, __func__, ##__VA_ARGS__)

#define isp_err(fmt, ...) \
    pr_err(TAG "%s " fmt, __func__, ##__VA_ARGS__)


static int my_isp_probe(struct platform_device *pdev)
{
    isp_info("\n");
    return 0;
}

static int my_isp_remove(struct platform_device *pdev)
{
    isp_info("\n");
    return 0;
}

static const struct of_device_id my_isp_of_match_table[] = {
    {.compatible = "mycompany,my_isp"},
    { },
};

static struct platform_driver my_isp_driver = {
    .driver = {
        .name = "my_isp",
        .owner = THIS_MODULE,
        .of_match_table = my_isp_of_match_table,
    },
    .probe  = my_isp_probe,
    .remove = my_isp_remove,
};

module_platform_driver(my_isp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("ISP Driver");