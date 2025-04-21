#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <media/v4l2-ctrls.h>
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

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
    struct my_sensor *mysen = ctrl->priv;

	sensor_info("id=%#x\n", ctrl->id);

    if (ctrl->id == V4L2_CID_BASE + 0x1010) {
        if (ctrl->val == true) {
            sensor_info("On\n");
        } else {
            sensor_info("Off\n");
        }
    }

	// TODO: 处理其它ctrl->id

    return 0;
}

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
    .s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_ctrl_config sensor_onoff_cfg = {
    .ops 	= &sensor_ctrl_ops,         // 操作集
    .id 	= V4L2_CID_BASE + 0x1010,   // 自定义控制ID，同一个subdev中不可重复
    .name 	= "sensor_onoff_ctrl",      // 控制项名称
    .type 	= V4L2_CTRL_TYPE_BOOLEAN,   // 控制项类型
    .min 	= 0,                        // 最小值，布尔类型固定为 0
    .max 	= 1,                        // 最大值，布尔类型固定为 1
    .step 	= 1,                        // 步长，布尔类型固定为 1
    .def 	= 1,                        // 默认值，布尔类型必须为 0 或 1
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

	// 初始化控制项处理器，分配1个控制项空间
	v4l2_ctrl_handler_init(&mysen->ctrl_handler, 1);

	// 注册一个自定义控制项到处理器上
	mysen->sensor_onoff_ctrl = v4l2_ctrl_new_custom(&mysen->ctrl_handler, &sensor_onoff_cfg, mysen);
	if (mysen->ctrl_handler.error) {
		sensor_err("Failed to register ctrl, error=%d\n", mysen->ctrl_handler.error);
	}
	
	// 将控制项处理器与子设备关联
	mysen->sd.ctrl_handler = &mysen->ctrl_handler;

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

	// 释放控制器申请的资源
	v4l2_ctrl_handler_free(&mysen->ctrl_handler);

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
