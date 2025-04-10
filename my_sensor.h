#ifndef __MY_SENSOR_H__
#define __MY_SENSOR_H__


#include <media/v4l2-subdev.h>


// 私有数据结构
struct my_sensor {
    struct platform_device *pdev;
    struct v4l2_subdev sd; 			// 子设备的 v4l2_subdev
    void *priv_data;       			// 其他私有数据（如寄存器基地址、硬件资源等）
    struct hrtimer timer;			// 定时器
    struct work_struct work;		// 工作项
    u8 *fbuffer;					// 存放一帧数据
    dma_addr_t dma_handle;			// 存放DMA物理地址
    
};


#endif /* __MY_SENSOR_H__ */

