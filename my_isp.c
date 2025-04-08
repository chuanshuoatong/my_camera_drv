#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include "my_isp.h"

// 定义 TAG
#define TAG "[my_isp_drv]: "

// 封装打印函数
#define isp_info(fmt, ...) \
    pr_info(TAG "%s " fmt, __func__, ##__VA_ARGS__)

#define isp_err(fmt, ...) \
    pr_err(TAG "%s " fmt, __func__, ##__VA_ARGS__)


// ISP 子设备的操作函数
static int isp_s_power(struct v4l2_subdev *sd, int on)
{
    isp_info("ISP: s_power called with on=%d\n", on);
    return 0;
}

static int isp_s_stream(struct v4l2_subdev *sd, int enable)
{
    isp_info("ISP: s_stream called with enable=%d\n", enable);
    return 0;
}

static int isp_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
                       struct v4l2_subdev_format *format)
{
    isp_info("ISP: set_fmt called\n");
    return 0;
}

static const struct v4l2_subdev_core_ops isp_core_ops = {
    .s_power = isp_s_power,
};

static const struct v4l2_subdev_video_ops isp_video_ops = {
    .s_stream = isp_s_stream,
};

static const struct v4l2_subdev_pad_ops isp_pad_ops = {
    .set_fmt = isp_set_fmt,
};

static const struct v4l2_subdev_ops isp_subdev_ops = {
    .core 	= &isp_core_ops,
    .video 	= &isp_video_ops,
    .pad 	= &isp_pad_ops,
};


static int my_isp_probe(struct platform_device *pdev)
{
	struct my_isp *myisp;
	
    isp_info("\n");

	// 给私有数据结构分配内存
	myisp = devm_kzalloc(&pdev->dev, sizeof(*myisp), GFP_KERNEL);
	if (!myisp)	
		return -ENOMEM;


	// 将私有数据结构与pdev关联
	myisp->pdev = pdev;
	platform_set_drvdata(pdev, myisp);


	// 初始化 v4l2_subdev
	v4l2_subdev_init(&myisp->sd, &isp_subdev_ops);
	myisp->sd.owner = THIS_MODULE;
	snprintf(myisp->sd.name, sizeof(myisp->sd.name), "my_isp_subdev");

	isp_info("ok\n");
	
    return 0;
}

static int my_isp_remove(struct platform_device *pdev)
{
	struct my_isp *myisp = platform_get_drvdata(pdev);
	
    isp_info("\n");

	if (!myisp) {
		isp_err("Private data structure is NULL\n");
        return -ENODEV;
	}

	isp_info("ok\n");
	
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