#ifndef __MY_CSI_H__
#define __MY_CSI_H__

#include <media/v4l2-subdev.h>
#include "my_ringbuffer.h"

// 私有数据结构
struct my_csi {
    struct platform_device *pdev;
    struct v4l2_subdev sd; 			// 子设备的 v4l2_subdev
    void *priv_data;       			// 其他私有数据（如寄存器基地址、硬件资源等）
    u8 *fbuffer;					// 存放一帧数据
    dma_addr_t dma_handle;			// 存放DMA物理地址
    void (*post_to_dma_cb)(u8 *fbuffer, int len);
	struct my_ring_buffer rb;		// ring buffer
};

#endif /* __MY_CSI_H__ */

