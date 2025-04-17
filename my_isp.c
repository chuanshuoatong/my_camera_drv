#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "my_isp.h"

// 定义 TAG
#define TAG "[my_isp_drv]: "

// 封装打印函数
#define isp_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define isp_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define isp_dbg(fmt, ...) \

#define FRAME_WIDTH			1280
#define FRAME_HEIGHT		720
#define FPS 				30
#define BYTES_PER_PIX_YUYV	2

static struct task_struct *isp_thread = NULL;
static struct my_ring_buffer *isp_rb = NULL;
static wait_queue_head_t consumer_wq;
static struct my_isp *g_myisp = NULL;


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

void my_isp_sync_ring_buffer(struct my_ring_buffer *rb)
{
	isp_rb = rb;
	isp_dbg("isp_rb=%p\n", isp_rb);
}
EXPORT_SYMBOL(my_isp_sync_ring_buffer);

void my_isp_wake_up_consumer(void)
{
	wake_up_interruptible(&consumer_wq);
}
EXPORT_SYMBOL(my_isp_wake_up_consumer);

void my_isp_register_dma_cb(void *cb)
{
	if (!g_myisp || !cb) {
		isp_err("Invlid pointer\n");
	} else {
		g_myisp->post_to_dma_cb = cb;
		isp_info("Registered post_to_dma_cb\n");
	}
}
EXPORT_SYMBOL(my_isp_register_dma_cb);

static int isp_thread_fn(void *data)
{
	struct my_isp *myisp = (struct my_isp *)data;
	void *frame_data;
	int i = 0;

	if (!myisp) {
		isp_err("Invalid pointer\n");
		return -EINVAL;
	}

	isp_info("ISP thread start\n");
	
	while (!kthread_should_stop()) {

		if (!isp_rb) {
			isp_err("isp_rb is null\n");
			msleep(10);
			continue;
		}

		// 阻塞等待，直到环形缓冲区有数据
        wait_event_interruptible(consumer_wq, kthread_should_stop() || !my_ring_buffer_empty_lock(isp_rb));

		if (kthread_should_stop())
			continue;
		
		// 调用 read 函数读取数据
        frame_data = my_ring_buffer_read(isp_rb);
        if (!frame_data) {
            isp_err("Failed to read data from ring buffer.\n");
            continue; // 读取失败，继续下一次循环
        }

        // TODO: 处理数据
        isp_info("Processing frame data...\n");

		// TODO: 处理完成，提交给DMA
		if (!myisp->post_to_dma_cb) {
			isp_err("Invalid callback\n");
		} else {
			myisp->post_to_dma_cb((u8 *)frame_data, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV));
		}

	}

	isp_info("ISP thread exit\n");
	
	return 0;
}

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

	// 将私有数据与subdev关联
	v4l2_set_subdevdata(&myisp->sd, pdev);

	// 启动内核线程
    isp_thread = kthread_run(isp_thread_fn, myisp, "isp_thread");
    if (IS_ERR(isp_thread)) {
        isp_err("Failed to start ISP thread\n");
        return PTR_ERR(isp_thread);
    }

	// 初始化消费者等待队列
	init_waitqueue_head(&consumer_wq);

	g_myisp = myisp;
	
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

	// 停掉内核线程
	if (isp_thread) {
		wake_up_interruptible(&consumer_wq);
        kthread_stop(isp_thread);
    }

	// 清理私有数据
	v4l2_set_subdevdata(&myisp->sd, NULL);

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