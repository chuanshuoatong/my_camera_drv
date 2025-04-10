#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include "my_sensor.h"

// 定义 TAG
#define TAG "[my_sensor_drv]: "

// 封装打印函数
#define sensor_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define sensor_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)


// 定义图像格式
#define FRAME_WIDTH		1280
#define FRAME_HEIGHT	720
#define FPS 30
#define NSECS_PER_SEC 1000000000


// 生成一帧 YUV422 数据（YUYV 排布）
static void generate_one_frame_yuyv(uint8_t *buffer, int width, int height, u8 Y, u8 U, u8 V)
{	
	int c, r;
	for (r = 0; r < height; r++) {
        for (c = 0; c < width; c += 2) {
            buffer[r * width * 2 + c * 2] = Y;     // Y0
            buffer[r * width * 2 + c * 2 + 2] = Y; // Y1
            buffer[r * width * 2 + c * 2 + 1] = U;  // U
            buffer[r * width * 2 + c * 2 + 3] = V; // V
        }
    }
}

static void sensor_work_handler(struct work_struct *work)
{
    struct my_sensor *mysen = container_of(work, struct my_sensor, work);
    static u64 i = 0;

	sensor_info("\n");

	
	if (!mysen->fbuffer) {
		sensor_err("Invalid fbuffer\n");
		return;
	}

    // 模拟生成一帧数据
	switch (i % 9) {
		case 0:
		    generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 235, 128, 128); 	// white
		   	break;
		case 1:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 76, 84, 255);	// red
			break;
		case 2:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 168, 102, 221);	// orange
			break;
		case 3:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 210, 16, 146);	// yellow
			break;
		case 4:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 149, 44, 21);	// green
			break;
		case 5:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 41, 240, 110);	// blue
			break;
		case 6:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 72, 187, 155);	// indigo
			break;
		case 7:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 107, 205, 212);	// purple
			break;
		case 8:
			generate_one_frame_yuyv(mysen->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 16, 128, 128);	// black
			break;
	}
	
	i++;

    // 将数据发送到 CSI
    //send_to_csi(sensor->frame_buffer);


	sensor_info("exit\n");
}

static enum hrtimer_restart sensor_timer_callback(struct hrtimer *timer)
{
	struct my_sensor *mysen = container_of(timer, struct my_sensor, timer);
	
    sensor_info("\n");

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
	
    sensor_info("SENSOR: s_stream called with enable=%d\n", enable);

	if (enable) {
		// 启动内核定时器
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

	// 分配fbuffer内存，YUV422，每像素占2Byte
	//mysen->fbuffer = devm_kzalloc(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * 2), GFP_KERNEL);
    //if (!mysen->fbuffer) {
    //    sensor_err("Failed to allocate memory for fbuffer\n");
    //    return -ENOMEM;
    //}

	// 给fbuffer分配内存，使用DMA共享内存，YUV422，每像素占2Byte
	mysen->fbuffer = dma_alloc_coherent(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * 2), &mysen->dma_handle, GFP_KERNEL);
	if (!mysen->fbuffer) {
    	sensor_err("Failed to allocate DMA buffer\n");
    	return -ENOMEM;
	}
	sensor_info("Allocate DMA buffer ok, dma_handle=%#x\n", mysen->dma_handle);


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

	if (mysen->fbuffer) {
        dma_free_coherent(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * 2), mysen->fbuffer, mysen->dma_handle);
        sensor_info("DMA buffer freed\n");
    }

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
