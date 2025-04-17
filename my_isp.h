#ifndef __MY_ISP_H__
#define __MY_ISP_H__

#include <media/v4l2-subdev.h>
#include "my_ringbuffer.h"

// 私有数据结构
struct my_isp {
    struct platform_device *pdev;
    struct v4l2_subdev sd; 			// 子设备的 v4l2_subdev
    void *priv_data;       			// 其他私有数据（如寄存器基地址、硬件资源等）
    void (*post_to_dma_cb)(u8 *fbuffer, int len);
};

#endif /* __MY_ISP_H__ */

