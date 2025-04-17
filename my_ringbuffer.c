#include <linux/module.h>
#include <linux/dma-mapping.h>
#include "my_ringbuffer.h"

// 定义 TAG
#define TAG "[my_ring_buf]: "

// 封装打印函数
#define rbuf_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define rbuf_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

// 定义图像格式
#define FRAME_WIDTH			1280
#define FRAME_HEIGHT		720
#define BYTES_PER_PIX_YUYV	2

// 初始化环形缓冲区
int my_ring_buffer_init(struct device *dev, struct my_ring_buffer *rb, size_t size)
{
    int i;

    if (!dev || !rb) {
		rbuf_err("Invalid pointer\n");
        return -EINVAL;
    }

    rb->write_idx = 0;
    rb->read_idx = 0;

    // 分配 DMA 缓冲区
    for (i = 0; i < MAX_FRAMES; i++) {
        rb->buffers[i] = dma_alloc_coherent(dev,
                                            size,
                                            &rb->dma_handles[i],
                                            GFP_KERNEL);
        if (!rb->buffers[i]) {
            rbuf_err("Failed to allocate DMA buffer %d\n", i);
            goto err_alloc;
        }
    }

    spin_lock_init(&rb->lock);

    return 0;

err_alloc:
    for (i--; i >= 0; i--) {
        dma_free_coherent(dev, size, rb->buffers[i], rb->dma_handles[i]);
		rbuf_err("Free DMA buffer %d\n", i);
    }
    return -ENOMEM;
}
EXPORT_SYMBOL(my_ring_buffer_init);

// 释放环形缓冲区
void my_ring_buffer_free(struct device *dev, struct my_ring_buffer *rb, size_t size)
{
    int i;

    if (!dev || !rb) {
		rbuf_err("Invalid pointer\n");
        return;
    }

    for (i = 0; i < MAX_FRAMES; i++) {
        if (rb->buffers[i]) {
            dma_free_coherent(dev, size, rb->buffers[i], rb->dma_handles[i]);
			rbuf_info("Free DMA buffer %d\n", i);
        }
    }
}
EXPORT_SYMBOL(my_ring_buffer_free);

// 带锁的，给外部用
bool my_ring_buffer_empty_lock(struct my_ring_buffer *rb)
{
    bool is_empty;

	if (!rb) {
		rbuf_err("Invalid pointer\n");
		return true;
	}

	spin_lock(&rb->lock);

    is_empty = (rb->read_idx == rb->write_idx);

	spin_unlock(&rb->lock);

    return is_empty;
}
EXPORT_SYMBOL(my_ring_buffer_empty_lock);


// 检查环形缓冲区是否为空
static bool my_ring_buffer_empty(struct my_ring_buffer *rb)
{
    bool is_empty;

    is_empty = (rb->read_idx == rb->write_idx);

    return is_empty;
}

// 检查环形缓冲区是否已满
static bool my_ring_buffer_full(struct my_ring_buffer *rb)
{
    bool is_full;

    is_full = ((rb->write_idx + 1) % MAX_FRAMES == rb->read_idx);

    return is_full;
}

// 生成一帧 YUV422 数据（YUYV 排布）
static void generate_one_frame_yuyv(uint8_t *buffer)
{	
	int c, r;
	u8 Y, U, V;
	static u64 i = 0;

	switch (i++ % 9) {
		case 0:
			Y=235; U=128; V=128; break;	// white
		case 1:
			Y=76; U=84; V=255; break;	// red
		case 2:
			Y=168; U=102; V=221; break;	// orange
		case 3:
			Y=210; U=16; V=146; break;	// yellow
		case 4:
			Y=149; U=44; V=21; break;	// green
		case 5:
			Y=41; U=240; V=110; break;	// blue
		case 6:
			Y=72; U=187; V=155; break;	// indigo
		case 7:
			Y=107; U=205; V=212; break;	// purple
		case 8:
			Y=16; U=128; V=128; break;	// black
	}

	for (r = 0; r < FRAME_HEIGHT; r++) {
        for (c = 0; c < FRAME_WIDTH; c += 2) {
            buffer[r * FRAME_WIDTH * 2 + c * 2] = Y;     // Y0
            buffer[r * FRAME_WIDTH * 2 + c * 2 + 2] = Y; // Y1
            buffer[r * FRAME_WIDTH * 2 + c * 2 + 1] = U; // U
            buffer[r * FRAME_WIDTH * 2 + c * 2 + 3] = V; // V
        }
    }

}

// 向环形缓冲区写入数据
int my_ring_buffer_write(struct my_ring_buffer *rb, void *frame_data, size_t size)
{
    if (!rb) {
        rbuf_err("Invalid pointer\n");
        return -EINVAL;
    }

	spin_lock(&rb->lock);
	
    if (my_ring_buffer_full(rb)) {
		// 缓冲区已满，不等空buffer，会阻塞生产者线程，直接丢弃当前帧
		rbuf_err("Ring buffer is full, dropping frame...\n");
        spin_unlock(&rb->lock);
        return -ENOBUFS;
	}

    rbuf_info("write_idx=%d\n", rb->write_idx);

    // TODO: 使用DMA将CSI输出的数据传输到缓冲区
    
    // 没有实际硬件，使用模拟的数据填充缓冲区
    generate_one_frame_yuyv(rb->buffers[rb->write_idx]);
	
    rb->write_idx = (rb->write_idx + 1) % MAX_FRAMES;

    // 手动释放锁
    spin_unlock(&rb->lock);

    return 0;
}
EXPORT_SYMBOL(my_ring_buffer_write);

// 从环形缓冲区读取数据
void *my_ring_buffer_read(struct my_ring_buffer *rb)
{
    void *frame_data;

    if (!rb) {
        rbuf_err("Invalid pointer\n");
        return NULL;
    }

	spin_lock(&rb->lock);

    if (my_ring_buffer_empty(rb)) {
		// 缓冲区空
		rbuf_err("Ring buffer is empty\n");
        spin_unlock(&rb->lock);
        return NULL;
	}

    rbuf_info("read_idx=%d\n", rb->read_idx);

    // 从缓冲区中读取数据
    frame_data = rb->buffers[rb->read_idx];
    rb->read_idx = (rb->read_idx + 1) % MAX_FRAMES;

    // 手动释放锁
    spin_unlock(&rb->lock);

    return frame_data;
}
EXPORT_SYMBOL(my_ring_buffer_read);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My Ringbuffer Module");
MODULE_VERSION("1.0");

