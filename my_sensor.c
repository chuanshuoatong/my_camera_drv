#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include "my_sensor.h"

// 定义 TAG
#define TAG "[my_sensor_drv]: "

// 封装打印函数
#define sensor_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define sensor_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

// 定义图像格式
#define FRAME_WIDTH			1280
#define FRAME_HEIGHT		720
#define FPS 				30
#define BYTES_PER_PIX_YUYV	2
#define NSECS_PER_SEC 		1000000000

extern void notify_csi_frame_ready(void);

static void sensor_work_handler(struct work_struct *work)
{
	//struct my_sensor *mysen = container_of(work, struct my_sensor, work);

	//sensor_info("\n");

	// 通知csi
    notify_csi_frame_ready();
}

static enum hrtimer_restart sensor_timer_callback(struct hrtimer *timer)
{
	struct my_sensor *mysen = container_of(timer, struct my_sensor, timer);
	
    //sensor_info("\n");

	// 使用work来处理业务逻辑，避免占用定时器周期 
	schedule_work(&mysen->work);

    // 重新启动定时器以实现周期性触发
    hrtimer_forward_now(timer, ktime_set(0, NSECS_PER_SEC / FPS));
	
    return HRTIMER_RESTART;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct platform_device *pdev = v4l2_get_subdevdata(sd);
	struct my_sensor *mysen = platform_get_drvdata(pdev);
	
    sensor_info("enable=%d\n", enable);

	if (enable) {
		// 启动内核定时器，模拟帧中断
		hrtimer_start(&mysen->timer, ktime_set(0, NSECS_PER_SEC / FPS), HRTIMER_MODE_REL);
	} else {
		// 强制停掉定时器，返回1-当前处于active但是关闭成功；0-当前未active
		hrtimer_cancel(&mysen->timer);
		
		// 确保工作队列中正在被调度的任务完成，挂起未被调度的不受影响
		flush_work(&mysen->work);
	}
	
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

	// 初始化内核定时器
	hrtimer_init(&mysen->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mysen->timer.function = sensor_timer_callback;
	sensor_info("Timer inited\n");

	// 初始化工作队列
	INIT_WORK(&mysen->work, sensor_work_handler);
	sensor_info("Work inited\n");

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

	// 强制停掉定时器，返回1-当前处于active但是关闭成功；0-当前未active
	hrtimer_cancel(&mysen->timer);

	// 确保工作队列中正在被调度的任务完成，取消挂起的
	cancel_work_sync(&mysen->work);


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
