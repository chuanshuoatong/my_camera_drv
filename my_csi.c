#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>

// 定义 TAG
#define TAG "[my_csi_drv]: "

// 封装打印函数
#define csi_info(fmt, ...) \
    pr_info(TAG "%s " fmt, __func__, ##__VA_ARGS__)

#define csi_err(fmt, ...) \
    pr_err(TAG "%s " fmt, __func__, ##__VA_ARGS__)


static int my_csi_probe(struct platform_device *pdev)
{
    csi_info("\n");
    return 0;
}

static int my_csi_remove(struct platform_device *pdev)
{
    csi_info("\n");
    return 0;
}

static const struct of_device_id my_csi_of_match_table[] = {
    {.compatible = "mycompany,my_csi"},
    { },
};

static struct platform_driver my_csi_driver = {
    .driver = {
        .name = "my_csi",
        .owner = THIS_MODULE,
        .of_match_table = my_csi_of_match_table,
    },
    .probe  = my_csi_probe,
    .remove = my_csi_remove,
};

module_platform_driver(my_csi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("CSI Driver");