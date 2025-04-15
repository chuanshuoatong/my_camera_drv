#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <media/videobuf2-core.h>
#include "my_csi.h"
#include "my_ringbuffer.h"

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

// 生成一帧 YUV422 数据（YUYV 排布）
static void generate_one_frame_yuyv(uint8_t *buffer, int width, int height, u8 Y, u8 U, u8 V)
{	
	int c, r;
	for (r = 0; r < height; r++) {
        for (c = 0; c < width; c += 2) {
            buffer[r * width * 2 + c * 2] = Y;     // Y0
            buffer[r * width * 2 + c * 2 + 2] = Y; // Y1
            buffer[r * width * 2 + c * 2 + 1] = U; // U
            buffer[r * width * 2 + c * 2 + 3] = V; // V
        }
    }
}

static int csi_thread_fn(void *data)
{
	struct my_csi *mycsi = (struct my_csi *)data;
	static u64 i = 0;
	struct vb2_buffer *vb = NULL;
	void *vaddr = NULL;

	if (!mycsi || !mycsi->fbuffer) {
		csi_info("Invalid pointer\n");
		return -EINVAL;
	}
	
	while (!kthread_should_stop()) {

		wait_event_interruptible_timeout(csi_wait_queue, 
										 frame_ready || kthread_should_stop(), 
										 msecs_to_jiffies(1000));
		if (frame_ready) {
			
			csi_info("Frame is ready, id=%d\n", i);
			
			frame_ready = false;			
		
			// 获取一帧模拟数据，每次更新一种颜色
			switch (i++ % 9) {
				case 0:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 235, 128, 128); 	// white
					break;
				case 1:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 76, 84, 255);	// red
					break;
				case 2:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 168, 102, 221);	// orange
					break;
				case 3:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 210, 16, 146);	// yellow
					break;
				case 4:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 149, 44, 21);	// green
					break;
				case 5:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 41, 240, 110);	// blue
					break;
				case 6:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 72, 187, 155);	// indigo
					break;
				case 7:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 107, 205, 212);	// purple
					break;
				case 8:
					generate_one_frame_yuyv(mycsi->fbuffer, FRAME_WIDTH, FRAME_HEIGHT, 16, 128, 128);	// black
					break;
			}

			// 将帧buffer地址提交给DMA
			if (!mycsi->post_to_dma_cb) {
				csi_err("Invalid callback\n");
			} else {
				mycsi->post_to_dma_cb(mycsi->fbuffer, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV));
			}			
		}
	}

	return 0;
}

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

	// 初始化等待队列
    init_waitqueue_head(&csi_wait_queue);
	
	// 给fbuffer分配内存，使用DMA共享内存，YUV422，每像素占2Byte
	mycsi->fbuffer = dma_alloc_coherent(&pdev->dev, (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIX_YUYV), &mycsi->dma_handle, GFP_KERNEL);
	if (!mycsi->fbuffer) {
    	csi_err("Failed to allocate DMA buffer\n");
    	return -ENOMEM;
	}
	csi_info("Allocate DMA buffer ok\n");

	my_ring_buffer_init(&pdev->dev, NULL, 1024);

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
