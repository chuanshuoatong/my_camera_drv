#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <media/videobuf2-core.h>
#include "my_csi.h"

// 定义 TAG
#define TAG "[my_csi_drv]: "

// 封装打印函数
#define csi_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define csi_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define FRAME_WIDTH			1280
#define FRAME_HEIGHT		720
#define FPS 				30
#define BYTES_PER_PIX_YUYV	2

static struct wait_queue_head csi_wait_queue;
static struct task_struct *csi_thread = NULL;
static bool frame_ready = false;
static struct my_csi *g_mycsi = NULL;

extern void my_isp_sync_ring_buffer(struct my_ring_buffer *rb);
extern void my_isp_wake_up_consumer(void);

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

void notify_csi_frame_ready(void)
{
	frame_ready = true;
	wake_up_interruptible(&csi_wait_queue);
}
EXPORT_SYMBOL(notify_csi_frame_ready);

void my_csi_register_dma_cb(void *cb)
{
	if (!g_mycsi || !cb) {
		csi_err("Invlid pointer\n");
	} else {
		g_mycsi->post_to_dma_cb = cb;
		csi_info("Registered post_to_dma_cb\n");
	}
}
EXPORT_SYMBOL(my_csi_register_dma_cb);

static int csi_thread_fn(void *data)
{
	struct my_csi *mycsi = (struct my_csi *)data;
	static u64 i = 0;
	struct vb2_buffer *vb = NULL;
	void *vaddr = NULL;

	if (!mycsi || !mycsi->fbuffer) {
		csi_err("Invalid pointer\n");
		return -EINVAL;
	}
	
	while (!kthread_should_stop()) {

		wait_event_interruptible_timeout(csi_wait_queue, 
										 frame_ready || kthread_should_stop(), 
										 msecs_to_jiffies(1000));
		if (frame_ready) {
			
			csi_info("Frame is ready, id=%d\n", i);
			
			frame_ready = false;

			my_ring_buffer_write(&mycsi->rb, NULL, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV));

			my_isp_wake_up_consumer();
			
		}
	}

	return 0;
}

static int my_csi_probe(struct platform_device *pdev)
{
	struct my_csi *mycsi;
	int ret = 0;
	
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

	// 初始化等待队列
    init_waitqueue_head(&csi_wait_queue);
	
	// 给fbuffer分配内存，使用DMA共享内存，YUV422，每像素占2Byte
	mycsi->fbuffer = dma_alloc_coherent(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV), &mycsi->dma_handle, GFP_KERNEL);
	if (!mycsi->fbuffer) {
    	csi_err("Failed to allocate DMA buffer\n");
    	return -ENOMEM;
	}
	csi_info("Allocate DMA buffer ok\n");

	// ring buffer 初始化
	ret = my_ring_buffer_init(&pdev->dev, &mycsi->rb, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV));
	if (ret) {
    	csi_err("Failed to init ring buffer\n");
    	return -ENOMEM;
	}
	csi_info("Inited ring buffer ok\n");
	// 将 ring buffer 地址告诉 ISP
	my_isp_sync_ring_buffer(&mycsi->rb);

	// 启动内核线程
    csi_thread = kthread_run(csi_thread_fn, mycsi, "csi_thread");
    if (IS_ERR(csi_thread)) {
        csi_err("Failed to start CSI thread\n");
        return PTR_ERR(csi_thread);
    }

	g_mycsi = mycsi;
	
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

	// 停掉内核线程
	if (csi_thread) {
        kthread_stop(csi_thread);
        csi_info("CSI thread stopped\n");
    }
	
	// 手动释放dma内存
	if (mycsi->fbuffer) {
        dma_free_coherent(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV), mycsi->fbuffer, mycsi->dma_handle);
        csi_info("DMA buffer freed\n");
    }

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
