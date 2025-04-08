#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include "my_csi.h"

// 定义 TAG
#define TAG "[my_csi_drv]: "

// 封装打印函数
#define csi_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define csi_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)


// CSI 子设备的操作函数
static int csi_s_power(struct v4l2_subdev *sd, int on)
{
    csi_info("CSI: s_power called with on=%d\n", on);
    return 0;
}

static int csi_s_stream(struct v4l2_subdev *sd, int enable)
{
    csi_info("CSI: s_stream called with enable=%d\n", enable);
    return 0;
}

static const struct v4l2_subdev_core_ops csi_core_ops = {
    .s_power = csi_s_power,
};

static const struct v4l2_subdev_video_ops csi_video_ops = {
    .s_stream = csi_s_stream,
};

static const struct v4l2_subdev_ops csi_subdev_ops = {
    .core 	= &csi_core_ops,
    .video 	= &csi_video_ops,
};

static int my_csi_probe(struct platform_device *pdev)
{
	struct my_csi *mycsi;
	
    csi_info("\n");


	// 给私有数据结构分配内存
	mycsi = devm_kzalloc(&pdev->dev, sizeof(*mycsi), GFP_KERNEL);
	if (!mycsi)
		return -ENOMEM;


	// 将私有数据结构与pdev关联
	mycsi->pdev = pdev;
	platform_set_drvdata(pdev, mycsi);


	// 初始化 v4l2_subdev
    v4l2_subdev_init(&mycsi->sd, &csi_subdev_ops);
    mycsi->sd.owner = THIS_MODULE;
    snprintf(mycsi->sd.name, sizeof(mycsi->sd.name), "my_csi_subdev");


	// 将私有数据与subdev关联
	v4l2_set_subdevdata(&mycsi->sd, pdev);
	

	csi_info("ok\n");
	
    return 0;
}

static int my_csi_remove(struct platform_device *pdev)
{
	struct my_csi *mycsi = platform_get_drvdata(pdev);
	
    csi_info("\n");


	if (!mycsi) {
		csi_err("Private data structure is NULL\n");
        return -ENODEV;
	}


	// 清理私有数据
	v4l2_set_subdevdata(&mycsi->sd, NULL);

	
	csi_info("ok\n");
	
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