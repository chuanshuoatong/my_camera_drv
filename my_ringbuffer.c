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
    init_waitqueue_head(&rb->wq);

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

// 检查环形缓冲区是否为空
bool my_ring_buffer_empty(struct my_ring_buffer *rb)
{
    bool is_empty;

    spin_lock(&rb->lock);
    is_empty = (rb->read_idx == rb->write_idx);
    spin_unlock(&rb->lock);

    return is_empty;
}
EXPORT_SYMBOL(my_ring_buffer_empty);

// 检查环形缓冲区是否已满
bool my_ring_buffer_full(struct my_ring_buffer *rb)
{
    bool is_full;

    spin_lock(&rb->lock);
    is_full = ((rb->write_idx + 1) % MAX_FRAMES == rb->read_idx);
    spin_unlock(&rb->lock);

    return is_full;
}
EXPORT_SYMBOL(my_ring_buffer_full);

// 向环形缓冲区写入数据
int my_ring_buffer_write(struct my_ring_buffer *rb, void *frame_data, size_t size)
{
    if (!rb || !frame_data) {
		rbuf_err("Invalid pointer\n");
        return -EINVAL;
    }

    spin_lock(&rb->lock);

    // 如果缓冲区已满，等待
    while (my_ring_buffer_full(rb)) {
        spin_unlock(&rb->lock);
        wait_event(rb->wq, !my_ring_buffer_full(rb));
        spin_lock(&rb->lock);
    }

    // 将数据复制到当前缓冲区
    //memcpy(rb->buffers[rb->write_idx], frame_data, size);
    rb->write_idx = (rb->write_idx + 1) % MAX_FRAMES;

    spin_unlock(&rb->lock);

    // 唤醒消费者
    wake_up_interruptible(&rb->wq);

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

    // 如果缓冲区为空，等待
    while (my_ring_buffer_empty(rb)) {
        spin_unlock(&rb->lock);
        wait_event(rb->wq, !my_ring_buffer_empty(rb));
        spin_lock(&rb->lock);
    }

    // 从缓冲区中读取数据
    frame_data = rb->buffers[rb->read_idx];
    rb->read_idx = (rb->read_idx + 1) % MAX_FRAMES;

    spin_unlock(&rb->lock);

    // 唤醒生产者
    wake_up_interruptible(&rb->wq);

    return frame_data;
}
EXPORT_SYMBOL(my_ring_buffer_read);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My Ringbuffer Module");
MODULE_VERSION("1.0");

