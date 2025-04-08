#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include "my_sensor.h"

// 定义 TAG
#define TAG "[my_sensor_drv]: "

// 封装打印函数
#define sensor_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define sensor_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)


static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
    sensor_info("SENSOR: s_stream called with enable=%d\n", enable);
    return 0;
}

static const struct v4l2_subdev_video_ops sensor_video_ops = {
    .s_stream = sensor_s_stream,
};

static const struct v4l2_subdev_ops sensor_subdev_ops = {
    .video 	= &sensor_video_ops,
};

static int my_sensor_probe(struct platform_device *pdev)
{
	struct my_sensor *mysen;
	int ret = 0;
	
    sensor_info("\n");
	

	// 给私有数据结构分配内存
	mysen = devm_kzalloc(&pdev->dev, sizeof(*mysen), GFP_KERNEL);
	if (!mysen)	
		return -ENOMEM;


	// 将私有数据结构与pdev关联
	mysen->pdev = pdev;
	platform_set_drvdata(pdev, mysen);


	// 初始化 v4l2_subdev
	v4l2_subdev_init(&mysen->sd, &sensor_subdev_ops);
	mysen->sd.owner = THIS_MODULE;
	mysen->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	mysen->sd.dev = &pdev->dev; // 非常重要，否则不会触发match
	snprintf(mysen->sd.name, sizeof(mysen->sd.name), "my_sensor_subdev");


	// 将私有数据与subdev关联
	v4l2_set_subdevdata(&mysen->sd, pdev);


	// 注册为异步子设备
	ret = v4l2_async_register_subdev(&mysen->sd);
    if (ret) {
        sensor_err("Failed to register async subdev, ret=%d\n", ret);
        return ret;
    }
    sensor_info("Async subdev registered\n");


	sensor_info("ok\n");
	
    return 0;
}

static int my_sensor_remove(struct platform_device *pdev)
{
    struct my_sensor *mysen = platform_get_drvdata(pdev);

    sensor_info("\n");


    // 注销异步子设备
    v4l2_async_unregister_subdev(&mysen->sd);
	v4l2_set_subdevdata(&mysen->sd, NULL); // 清除私有数据
    sensor_info("Async subdev unregistered\n");


	sensor_info("ok\n");

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