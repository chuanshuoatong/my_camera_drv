#ifndef __MY_RINGBUFFER_H__
#define __MY_RINGBUFFER_H__

#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/device.h>

// 环形缓冲区的最大帧数
#define MAX_FRAMES 			3

struct my_ring_buffer {
    void *buffers[MAX_FRAMES];      	// 缓冲区数组
    dma_addr_t dma_handles[MAX_FRAMES]; // 每个缓冲区的物理地址
    int write_idx;                  	// 写指针
    int read_idx;                   	// 读指针
    spinlock_t lock;                	// 保护缓冲区的锁
};

// 初始化环形缓冲区
int my_ring_buffer_init(struct device *dev, struct my_ring_buffer *rb, size_t size);

// 释放环形缓冲区
void my_ring_buffer_free(struct device *dev, struct my_ring_buffer *rb, size_t size);

// 判断环形缓冲区是否空
bool my_ring_buffer_empty_lock(struct my_ring_buffer *rb);

// 向环形缓冲区写入数据
int my_ring_buffer_write(struct my_ring_buffer *rb, void *frame_data, size_t size);

// 从环形缓冲区读取数据
void *my_ring_buffer_read(struct my_ring_buffer *rb);

#endif /* __MY_RINGBUFFER_H__ */

