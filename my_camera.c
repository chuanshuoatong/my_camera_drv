#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include "my_camera.h"
#include "my_isp.h"
#include "my_csi.h"


// 定义 TAG
#define TAG "[my_camera_drv]: "

// 封装打印函数
#define cam_info(fmt, ...) \
    pr_info(TAG "%s: " fmt, __func__, ##__VA_ARGS__)

#define cam_err(fmt, ...) \
    pr_err(TAG "%s: " fmt, __func__, ##__VA_ARGS__)


// 定义图像格式
#define FRAME_WIDTH		1280
#define FRAME_HEIGHT	720
#define FRAME_PS		25


struct my_camera {
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier; // 异步通知链
	struct video_device vdev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct mutex lock;
	v4l2_std_id std;
	struct v4l2_dv_timings timings;
	struct v4l2_pix_format format;
	unsigned input;

	struct vb2_queue queue;

	spinlock_t qlock;
	struct list_head buf_list;
	unsigned field;
	unsigned sequence;

	struct v4l2_subdev *isp_subdev;
	struct v4l2_subdev *csi_subdev;
	struct v4l2_subdev *sensor_subdev;
};


struct mycam_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

static inline struct mycam_buffer *to_mycam_buffer(struct vb2_v4l2_buffer *vbuf)
{
	return container_of(vbuf, struct mycam_buffer, vb);
}


// v4l2_ioctl_ops 的回调函数实现
static int mycam_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	struct my_camera *mycam = video_drvdata(file);
	
	//cam_info("Called by %s\n", current->comm); // 打印调用进程的名字
	cam_info("\n");

	strlcpy(cap->driver, KBUILD_MODNAME, sizeof(cap->driver));
	strlcpy(cap->card, "mipi-csi", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "Platform:%s", mycam->pdev->name);
	
	return 0;
}


static void mycam_fill_pix_format(struct my_camera *mycam,
				     struct v4l2_pix_format *pix)
{
	pix->pixelformat  = V4L2_PIX_FMT_YUYV;
	pix->width        = FRAME_WIDTH;
	pix->height       = FRAME_HEIGHT;
	pix->field        = V4L2_FIELD_NONE;
	pix->colorspace   = V4L2_COLORSPACE_SRGB;
	
	/*
	 * The YUYV format is four bytes for every two pixels, so bytesperline
	 * is width * 2.
	 */
	pix->bytesperline = pix->width * 2;
	pix->sizeimage    = pix->bytesperline * pix->height;
	pix->priv         = 0;
}


// 视频格式相关
static int mycam_try_fmt_vid_cap(struct file *file, void *priv,
				    struct v4l2_format *f)
{
	cam_info("width=%u, height=%u, format=%#x\n",
            f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.pixelformat);
			
	return 0;
}

static int mycam_s_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	cam_info("width=%u, height=%u, format=%#x\n",
            f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.pixelformat);

	// 忽略用户设置
			
	return 0;
}

static int mycam_g_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct my_camera *mycam = video_drvdata(file);

	f->fmt.pix = mycam->format;

	return 0;
}

static int mycam_enum_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	//cam_info("Called by %s\n", current->comm); // 打印调用进程的名字
	cam_info("\n");
	
	if (f->index != 0)
		return -EINVAL;

	f->pixelformat = V4L2_PIX_FMT_YUYV;
	
	return 0;
}



// 模拟电视信号相关的标准，数字摄像头(USB/CSI)不用设置
static int mycam_s_std(struct file *file, void *priv, v4l2_std_id std)
{
	cam_info("\n");
	return -EINVAL;
}

static int mycam_g_std(struct file *file, void *priv, v4l2_std_id *std)
{
	cam_info("\n");
	return -EINVAL;
}

static int mycam_querystd(struct file *file, void *priv, v4l2_std_id *std)
{
	cam_info("\n");
	return -EINVAL;
}




// 摄像头时序参数，对于只支持一种固定的分辨率和帧率，并且这些参数在启动时已经通过其他方式（如 VIDIOC_S_FMT）设置好了，可不实现 dv_timings 相关的回调函数。
static int mycam_s_dv_timings(struct file *file, void *_fh,
				 struct v4l2_dv_timings *timings)
{
	cam_info("\n");
	return -EINVAL;
}
				 
static int mycam_g_dv_timings(struct file *file, void *_fh,
				 struct v4l2_dv_timings *timings)
{
	cam_info("\n");
	return -EINVAL;
}

static int mycam_enum_dv_timings(struct file *file, void *_fh,
				    struct v4l2_enum_dv_timings *timings)
{
	cam_info("\n");
	return -EINVAL;
}

static int mycam_query_dv_timings(struct file *file, void *_fh,
				     struct v4l2_dv_timings *timings)
{
	cam_info("\n");
	return -EINVAL;
}

static int mycam_dv_timings_cap(struct file *file, void *fh,
				   struct v4l2_dv_timings_cap *cap)
{
	cam_info("\n");
	return -EINVAL;
}



// 摄像头输入源相关参数，MIPI CSI 摄像头只有一个固定的输入源，可固定成某个值
static int mycam_enum_input(struct file *file, void *priv,
			       struct v4l2_input *i)
{
	cam_info("\n");

	if (i->index != 0)
        return -EINVAL;

    i->type = V4L2_INPUT_TYPE_CAMERA;
    strlcpy(i->name, "MIPI CSI Camera", sizeof(i->name));
	
	return 0;
}

static int mycam_s_input(struct file *file, void *priv, unsigned int i)
{
	cam_info("\n");
	return (i == 0) ? 0 : -EINVAL;
}

static int mycam_g_input(struct file *file, void *priv, unsigned int *i)
{
	cam_info("\n");
	*i = 0;
	return 0;
}



//
static int mycam_vb2_ioctl_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *req)
{
    cam_info("type=%u, memory=%u, count=%u\n", req->type, req->memory, req->count);

    if (req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    if (req->memory != V4L2_MEMORY_MMAP)
        return -EINVAL;

    return vb2_ioctl_reqbufs(file, fh, req);
}


static int mycam_vb2_ioctl_querybuf(struct file *file, void *fh, struct v4l2_buffer *p)
{
    cam_info("index=%u, type=%u, memory=%u\n", p->index, p->type, p->memory);

    if (p->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    if (p->memory != V4L2_MEMORY_MMAP)
        return -EINVAL;

    return vb2_ioctl_querybuf(file, fh, p);
}

static int mycam_vb2_ioctl_qbuf(struct file *file, void *fh, struct v4l2_buffer *p)
{
	cam_info("index=%u\n", p->index);
	
	return vb2_ioctl_qbuf(file, fh, p);
}

static int mycam_vb2_ioctl_dqbuf(struct file *file, void *fh, struct v4l2_buffer *p)
{
	cam_info("index=%u\n", p->index);
	
	return vb2_ioctl_dqbuf(file, fh, p);
}

static int mycam_vb2_ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	cam_info("\n");

	return vb2_ioctl_streamon(file, fh, i);
}

static int mycam_vb2_ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	cam_info("\n");

	return vb2_ioctl_streamoff(file, fh, i);
}

#if 1
/*
 * Setup the constraints of the queue: besides setting the number of planes
 * per buffer and the size and allocation context of each plane, it also
 * checks if sufficient buffers have been allocated. Usually 3 is a good
 * minimum number: many DMA engines need a minimum of 2 buffers in the
 * queue and you need to have another available for userspace processing.
 */
static int queue_setup(struct vb2_queue *vq,
		       unsigned int *nbuffers, unsigned int *nplanes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct my_camera *mycam = vb2_get_drv_priv(vq);

	cam_info("num_buffers=%u, nbuffers=%u, nplanes=%u\n", vq->num_buffers, *nbuffers, *nplanes);
	
	/*  vq->num_buffers 是vb2_queue中已分配的buffer个数
		nbuffers 是用户情况的个数，这里可以根据硬件实际情况进行调整
		nplanes 是平面的个数，GPT说像YUV420这种Y/U/V多分量的建议用多个plane分别表示各个分量，
		但是为了简化应用层管理内存，这里仅设置一个平面。
	*/
	if (vq->num_buffers + *nbuffers < 4)	// 这里设置一共最多4个buffer
		*nbuffers = 4 - vq->num_buffers;

	// 设置为单平面
	*nplanes = 1;

	// 但平面的大小设置为当前像素格式的总大小
	sizes[0] = mycam->format.sizeimage;

	cam_info("nbuffers=%u, nplanes=%u, sizes[0]=%u\n", *nbuffers, *nplanes, sizes[0]);
	
	return 0;
}

/*
 * Prepare the buffer for queueing to the DMA engine: check and set the
 * payload size.
 */
static int buffer_prepare(struct vb2_buffer *vb)
{
	struct my_camera *mycam = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size = mycam->format.sizeimage;

	cam_info("index=%u\n", vb->index);

	if (vb2_plane_size(vb, 0) < size) {
		cam_err("buffer too small (%lu < %lu)\n", vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);
	return 0;
}

/*
 * Queue this buffer to the DMA engine.
 */
static void buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct my_camera *mycam = vb2_get_drv_priv(vb->vb2_queue);
	struct mycam_buffer *buf = to_mycam_buffer(vbuf);
	unsigned long flags = 0;

	// vb/vbuf/buf 三者地址应该一样
	cam_info("index=%u, vb2_buffer=%p, vbuf=%p, buf=%p\n", vb->index, vb, vbuf, buf);

	spin_lock_irqsave(&mycam->qlock, flags);
	list_add_tail(&buf->list, &mycam->buf_list);

	/* TODO: Update any DMA pointers if necessary */
	// TODO: 把缓冲区的物理地址告诉DMA，以便后续接收传感器数据时直接写入这些缓冲区。

	spin_unlock_irqrestore(&mycam->qlock, flags);
}

static void return_all_buffers(struct my_camera *mycam,
			       enum vb2_buffer_state state)
{
	struct mycam_buffer *buf, *node;
	unsigned long flags;

	cam_info("\n");

	spin_lock_irqsave(&mycam->qlock, flags);
	list_for_each_entry_safe(buf, node, &mycam->buf_list, list) {
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&mycam->qlock, flags);
}

/*
 * Start streaming. First check if the minimum number of buffers have been
 * queued. If not, then return -ENOBUFS and the vb2 framework will call
 * this function again the next time a buffer has been queued until enough
 * buffers are available to actually start the DMA engine.
 */
static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct my_camera *mycam = vb2_get_drv_priv(vq);
	int ret = 0;

	mycam->sequence = 0;

	cam_info("\n");
	
	/* TODO: start DMA */
	// 主设备通过 v4l2_subdev_call 调用 ISP 子设备的 s_stream 操作。

	if (mycam->sensor_subdev) {
		ret = v4l2_subdev_call(mycam->sensor_subdev, video, s_stream, 1);
		if (ret && ret != -ENOIOCTLCMD) {
            cam_err("Failed to start sensor streaming, ret=%d\n", ret);
        }
	}

	if (ret) {
		/*
		 * In case of an error, return all active buffers to the
		 * QUEUED state
		 */
		return_all_buffers(mycam, VB2_BUF_STATE_QUEUED);
	}
	return ret;
}

/*
 * Stop the DMA engine. Any remaining buffers in the DMA queue are dequeued
 * and passed on to the vb2 framework marked as STATE_ERROR.
 */
static void stop_streaming(struct vb2_queue *vq)
{
	struct my_camera *mycam = vb2_get_drv_priv(vq);
	int ret = 0;

	cam_info("\n");
	
	/* TODO: stop DMA */
	// 关闭sensor
	if (mycam->sensor_subdev) {
		ret = v4l2_subdev_call(mycam->sensor_subdev, video, s_stream, 0);
		if (ret && ret != -ENOIOCTLCMD) {
            cam_err("Failed to stop sensor streaming, ret=%d\n", ret);
        }
	}

	/* Release all active buffers */
	return_all_buffers(mycam, VB2_BUF_STATE_ERROR);
}
#endif

/*
 * The vb2 queue ops. Note that since q->lock is set we can use the standard
 * vb2_ops_wait_prepare/finish helper functions. If q->lock would be NULL,
 * then this driver would have to provide these ops.
 */
static const struct vb2_ops mycam_qops = {
	.queue_setup		= queue_setup,			// 会被 vb2_ioctl_reqbufs 调用
	.buf_prepare		= buffer_prepare,		// 会被 vb2_ioctl_qbuf 调用
	.buf_queue			= buffer_queue,			// 会被 vb2_ioctl_streamon 调用
	.start_streaming	= start_streaming,		// 会被 vb2_ioctl_streamon 调用
	.stop_streaming		= stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};


static const struct v4l2_ioctl_ops my_v4l2_ioctl_ops = {
	.vidioc_querycap = mycam_querycap,
	.vidioc_try_fmt_vid_cap = mycam_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = mycam_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = mycam_g_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap = mycam_enum_fmt_vid_cap,

	.vidioc_g_std = mycam_g_std,
	.vidioc_s_std = mycam_s_std,
	.vidioc_querystd = mycam_querystd,

	.vidioc_s_dv_timings = mycam_s_dv_timings,
	.vidioc_g_dv_timings = mycam_g_dv_timings,
	.vidioc_enum_dv_timings = mycam_enum_dv_timings,
	.vidioc_query_dv_timings = mycam_query_dv_timings,
	.vidioc_dv_timings_cap = mycam_dv_timings_cap,

	.vidioc_enum_input = mycam_enum_input,
	.vidioc_g_input = mycam_g_input,
	.vidioc_s_input = mycam_s_input,

	.vidioc_reqbufs 	= mycam_vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_querybuf 	= mycam_vb2_ioctl_querybuf,
	.vidioc_qbuf 		= mycam_vb2_ioctl_qbuf,
	.vidioc_dqbuf 		= mycam_vb2_ioctl_dqbuf,
	.vidioc_expbuf 		= vb2_ioctl_expbuf,
	.vidioc_streamon 	= mycam_vb2_ioctl_streamon,
	.vidioc_streamoff 	= mycam_vb2_ioctl_streamoff,

	.vidioc_log_status = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

// video_device 的 v4l2_file_operations 函数集全部都用 vb2 的
static const struct v4l2_file_operations my_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.read = vb2_fop_read,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};

void video_device_release(struct video_device *vdev)
{
	/* Do nothing */
	/* Only valid when the video_device struct is a static. */
}


static int mycam_register_subdevs(struct platform_device *pdev)
{
	struct my_camera *mycam = platform_get_drvdata(pdev);
	struct device_node *isp_node = NULL, *csi_node = NULL;
	struct platform_device *isp_pdev = NULL, *csi_pdev = NULL;
	struct my_isp *myisp = NULL;
	struct my_csi *mycsi = NULL;
	int ret = 0;

	cam_info("\n");


	// 解析并绑定 ISP subdev
	isp_node = of_parse_phandle(pdev->dev.of_node, "isp-subdev", 0);
	if (!isp_node) {
		cam_err("Failed to find the device_node by isp-subdev\n");
		return -ENODEV;
	}
	
	isp_pdev = of_find_device_by_node(isp_node);
	if (!isp_pdev) {
		cam_err("Failed to find the platform_device by isp-subdev\n");
		of_node_put(isp_node);
		return -ENODEV;
	}
	
	myisp = platform_get_drvdata(isp_pdev);
	if (!myisp || !myisp->sd.name[0]) {
		cam_err("Failed to get ISP subdev\n");
		of_node_put(isp_node);
		return -ENODEV;
	}
	
	ret = v4l2_device_register_subdev(&mycam->v4l2_dev, &myisp->sd);
	if (ret) {
		cam_err("Failed to register ISP subdev\n");
		of_node_put(isp_node);
		return -ENODEV;
	}
	cam_info("ISP subdev bound and registered: %s\n", myisp->sd.name);
	mycam->isp_subdev = &myisp->sd;



	// 解析并绑定 CSI subdev
	csi_node = of_parse_phandle(pdev->dev.of_node, "csi-subdev", 0);
	if (!csi_node) {
		cam_err("Failed to find the device_node by csi-subdev\n");
		return -ENODEV;
	}
	
	csi_pdev = of_find_device_by_node(csi_node);
	if (!csi_pdev) {
		cam_err("Failed to find the platform_device by csi-subdev\n");
		of_node_put(csi_node);
		return -ENODEV;
	}
	
	mycsi = platform_get_drvdata(csi_pdev);
	if (!mycsi || !mycsi->sd.name[0]) {
		cam_err("Failed to get CSI subdev\n");
		of_node_put(csi_node);
		return -ENODEV;
	}
	
	ret = v4l2_device_register_subdev(&mycam->v4l2_dev, &mycsi->sd);
	if (ret) {
		cam_err("Failed to register CSI subdev\n");
		of_node_put(csi_node);
		return -ENODEV;
	}
	cam_info("CSI subdev bound and registered: %s\n", mycsi->sd.name);
	mycam->csi_subdev = &mycsi->sd;

	cam_info("ok\n");

	return 0;
}


static int mycam_notifier_bound(struct v4l2_async_notifier *notifier,
                                struct v4l2_subdev *subdev,
                                struct v4l2_async_subdev *asd)
{
    struct my_camera *mycam = container_of(notifier, struct my_camera, notifier);

    cam_info("Subdevice '%s' bound to main device\n", subdev->name);


	// 这里只需要保存异步子设备的subdev，其它的已经在mycam_register_subdevs中保存了
    if (!strcmp(subdev->name, "my_sensor_subdev")) {
        mycam->sensor_subdev = subdev;
    }

    return 0;
}

static int mycam_notifier_complete(struct v4l2_async_notifier *notifier)
{
    //struct my_camera *mycam = container_of(notifier, struct my_camera, notifier);
    struct v4l2_device *v4l2_dev = notifier->v4l2_dev;
    struct v4l2_subdev *sd;
	int ret = 0;

    cam_info("All subdevices have been bound\n");
	

    // 遍历 v4l2_device 的子设备链表
    list_for_each_entry(sd, &v4l2_dev->subdevs, list) {
        cam_info("Found subdevice: %s\n", sd->name);
    }

	// 为所有子设备创建devnode，前提是子设备的sd.flags中设置了V4L2_SUBDEV_FL_HAS_DEVNODE
	ret = v4l2_device_register_subdev_nodes(v4l2_dev);
	if (ret) {
		cam_err("Failed to register device node for subdevs\n");
	}

    // 在这里可以执行一些后续操作，比如启动流媒体或初始化硬件
    return 0;
}


static const struct v4l2_async_notifier_operations mycam_notifier_ops = {
    .bound = mycam_notifier_bound,
    .complete = mycam_notifier_complete,
};

static int mycam_parse_dts_remote_node(struct platform_device *pdev, struct device_node **remote_node)
{
	struct device_node *node = pdev->dev.of_node;
    struct device_node *port, *endpoint;

	// 获取本节点中的port节点
	port = of_get_child_by_name(node, "port");
	if (!port) {
		cam_err("Failed to find 'port' node in device tree\n");
        return -ENODEV;
	}

	// 获取port节点中的endpoint节点
	endpoint = of_get_child_by_name(port, "endpoint");
	if (!endpoint) {
		cam_err("Failed to find 'endpoint' node in device tree\n");
		of_node_put(port);
		return -ENODEV;
	}

	// 获取endpoint节点中remote-endpoint所指向的endpoint所在的port的父节点
	*remote_node = of_graph_get_remote_port_parent(endpoint);
	if (!*remote_node) {
        cam_err("Failed to get remote node from endpoint\n");
        of_node_put(endpoint);
		of_node_put(port);
        return -ENODEV;
    }
	
	cam_info("Found remote_node: %s\n", (*remote_node)->name);

	// 释放中间节点的引用
	of_node_put(endpoint);
	of_node_put(port);

	return 0;
}

static int mycam_register_async_notifier(struct platform_device *pdev)
{
	struct my_camera *mycam  = platform_get_drvdata(pdev);
    struct device_node *remote_node = NULL;
    struct v4l2_async_subdev *asd = NULL;
	int ret = 0;

	cam_info("\n");
	

	// 1. 初始化异步通知链
	mycam->notifier.ops = &mycam_notifier_ops;
    v4l2_async_notifier_init(&mycam->notifier);
	

	// 解析设备树，获取远端节点
	ret = mycam_parse_dts_remote_node(pdev, &remote_node);
	if (ret) {
		cam_err("Failed to parse remote node, ret=%d\n", ret);
		goto err_cleanup_notifier;
	}


	// 2. 向通知链中添加需要监听的异步子设备
	asd = v4l2_async_notifier_add_fwnode_subdev(&mycam->notifier, 
												of_fwnode_handle(remote_node), 
												sizeof(struct v4l2_async_subdev));
	of_node_put(remote_node);
	
	if (IS_ERR(asd)) {
		cam_err("Failed to add async subdev, ret=%ld\n", PTR_ERR(asd));
        ret = PTR_ERR(asd);
		goto err_cleanup_notifier;
	}
	

	// 3. 向内核注册通知链，内核开始监听
	ret = v4l2_async_notifier_register(&mycam->v4l2_dev, &mycam->notifier);
	if (ret) {
    	cam_err("Failed to register async notifier, ret=%d\n", ret);
    	goto err_cleanup_notifier;
	}

	return 0;

err_cleanup_notifier:
	v4l2_async_notifier_cleanup(&mycam->notifier);
	return ret;
}

static int my_camera_probe(struct platform_device *pdev)
{
    int ret;
	struct my_camera *mycam;
	struct video_device *vdev;
	struct vb2_queue *q;

	cam_info("\n");

	// 初始化私有数据结构
	mycam = devm_kzalloc(&pdev->dev, sizeof(struct my_camera), GFP_KERNEL);
	if (!mycam) {
		return -ENOMEM;
	}
	mycam->pdev = pdev;
	platform_set_drvdata(pdev, mycam);

	// 初始化锁
	mutex_init(&mycam->lock);

	// 填充初始格式相关设置
	mycam_fill_pix_format(mycam, &mycam->format);
	
    // 注册 v4l2_device
    strscpy(mycam->v4l2_dev.name, "my_v4l2_device", sizeof(mycam->v4l2_dev.name));
    ret = v4l2_device_register(&pdev->dev, &mycam->v4l2_dev);
    if (ret) {
        cam_err("Failed to register v4l2_device, ret=%d\n", ret);
        goto err_exit;
    }
    cam_info("v4l2_device registered: %s\n", mycam->v4l2_dev.name);

	// 解析绑定 subdev
	ret = mycam_register_subdevs(pdev);
	if (ret) {
		cam_err("Failed to register subdevs, ret=%d\n", ret);
		goto err_unregister_v4l2_dev;
	}

	// 初始化 vb2_queue
	q = &mycam->queue;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ;
	q->dev = &pdev->dev;
	q->drv_priv = mycam;
	q->buf_struct_size = sizeof(struct mycam_buffer); // 很重要，__vb2_queue_alloc 中实际会按此大小分配内存
	q->ops = &mycam_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 2;
	q->lock = &mycam->lock;
	q->gfp_flags = GFP_DMA32;
	ret = vb2_queue_init(q);
	if (ret) {
		cam_err("Failed to init vb2_queue, ret=%d\n", ret);
		goto err_unregister_subdevs;
	}

	INIT_LIST_HEAD(&mycam->buf_list);
	spin_lock_init(&mycam->qlock);

    // 初始化 video_device 节点
    vdev = &mycam->vdev;
    snprintf(vdev->name, sizeof(vdev->name), "my_video_device");
	vdev->release = video_device_release_empty;
    vdev->fops = &my_v4l2_fops;
	vdev->ioctl_ops = &my_v4l2_ioctl_ops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
    vdev->lock = &mycam->lock;	
	vdev->queue = q;
	vdev->v4l2_dev = &mycam->v4l2_dev;
	video_set_drvdata(vdev, mycam);
    ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
    if (ret) {
        cam_err("Failed to register video_device, ret=%d\n", ret);
        goto err_release_vb2_queue;
    }
    cam_info("video_device registered: /dev/video%d\n", vdev->num);

	// 初始化异步通知链
	ret = mycam_register_async_notifier(pdev);
	if (ret) {
		cam_err("Failed to register async notifier, ret=%d\n", ret);
		goto err_cleanup_notifier;
	}

	cam_info("ok\n");

    return 0;

err_cleanup_notifier:
	v4l2_async_notifier_cleanup(&mycam->notifier);
err_release_vb2_queue:
	vb2_queue_release(q);
err_unregister_subdevs:
	v4l2_device_unregister_subdev(mycam->isp_subdev);
	v4l2_device_unregister_subdev(mycam->csi_subdev);
err_unregister_v4l2_dev:
	v4l2_device_unregister(&mycam->v4l2_dev);
err_exit:
	return ret;
}

static int my_camera_remove(struct platform_device *pdev)
{
	struct my_camera *mycam = platform_get_drvdata(pdev);

	cam_info("\n");
	
	if (!mycam) {
        cam_err("Private data structure is NULL\n");
        return -ENODEV;
	}

	// 注销通知链，清空通知链
	v4l2_async_notifier_unregister(&mycam->notifier);
	v4l2_async_notifier_cleanup(&mycam->notifier);
	cam_info("Unregistered and cleanup notifier\n");

	// 注销 video_device
	if (video_is_registered(&mycam->vdev)) {
		video_unregister_device(&mycam->vdev);
		cam_info("Unregistered video_device: /dev/video%d\n", mycam->vdev.num);
	}

	// 释放 VB2 资源
	vb2_queue_release(&mycam->queue);
	cam_info("Released vb2_queue\n");


	if (mycam->isp_subdev) {
		v4l2_device_unregister_subdev(mycam->isp_subdev);
		cam_info("Unregistered isp_subdev\n");
	}

	if (mycam->csi_subdev) {
		v4l2_device_unregister_subdev(mycam->csi_subdev);
		cam_info("Unregistered csi_subdev\n");
	}

    // 注销 v4l2_device
    v4l2_device_unregister(&mycam->v4l2_dev);
    cam_info("Unregistered v4l2_device: %s\n", mycam->v4l2_dev.name);

	cam_info("ok\n");
	
    return 0;
}


static const struct of_device_id my_camera_of_match_table[] = {
	{.compatible = "mycompany,my_camera"},
	{ },
};

static struct platform_driver my_camera_driver = {
    .driver = {
        .name = "my_camera",
        .owner = THIS_MODULE,
        .of_match_table = my_camera_of_match_table,
    },
    .probe  = my_camera_probe,
    .remove = my_camera_remove,
};


module_platform_driver(my_camera_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("CAMERA Driver");
