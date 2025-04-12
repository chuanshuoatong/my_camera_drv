#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <sys/stat.h> // 包含 mkdir 函数
#include <errno.h>    // 包含 errno 宏
#include <signal.h>   // 包含信号处理函数
#include <time.h>     // 包含高精度时间函数

#define WIDTH 		1920
#define HEIGHT 		1080
#define NUM_BUFFERS 4
#define CAMERA_DEV	"/dev/video0"

static struct timespec curr_frame_ts = {0};
static struct timespec last_frame_ts = {0};

void save_to_yuv(void *buffer, int len);
void handle_sigint(int sig);          // 信号处理函数

int main() {
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    void *buffers[NUM_BUFFERS];
    __u32 i;
	double frame_period_ms = 0.0;
	
	// 注册信号处理函数以捕获 Ctrl+C
    signal(SIGINT, handle_sigint);

    // 打开设备
    fd = open(CAMERA_DEV, O_RDWR);
    if (fd < 0) {
        perror("无法打开设备");
        return -1;
    }
	printf("\nOpen %s successfully\n\n", CAMERA_DEV);

    // 查询设备能力
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("查询设备能力失败");
        close(fd);
        return -1;
    }
    printf("VIDIOC_QUERYCAP successfully\n");
	printf("    driver: %s\n", cap.driver);
	printf("    card: %s\n", cap.card);
	printf("    bus_info: %s\n", cap.bus_info);
	printf("    capabilities: %#x\n", cap.capabilities);
	if (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
		printf("        V4L2_CAP_DEVICE_CAPS - 支持 device_caps 属性\n");
	if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
		printf("        V4L2_CAP_VIDEO_CAPTURE - 支持视频捕获\n");
	if (cap.capabilities & V4L2_CAP_READWRITE)
		printf("        V4L2_CAP_READWRITE - 支持读写操作\n");
	if (cap.capabilities & V4L2_CAP_STREAMING)
		printf("        V4L2_CAP_STREAMING - 支持流式传输\n");
	if (cap.capabilities & V4L2_CAP_EXT_PIX_FORMAT)
		printf("        V4L2_CAP_EXT_PIX_FORMAT - 支持扩展像素格式\n");
	printf("\n");

    // 设置视频格式
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // YUYV 格式
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("设置视频格式失败");
        close(fd);
        return -1;
    }
	printf("VIDIOC_S_FMT successfully\n\n");

    // 请求缓冲区
    memset(&req, 0, sizeof(req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("请求缓冲区失败");
        close(fd);
        return -1;
    }
	printf("VIDIOC_REQBUFS successfully\n\n");

    // 映射缓冲区
    for (i = 0; i < req.count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("查询缓冲区失败");
            close(fd);
            return -1;
        }
		printf("VIDIOC_QUERYBUF %d successfully\n", i);
		printf("    type: %u\n", buf.type);
		printf("    memory: %u\n", buf.memory);
		printf("    length: %u\n", buf.length);
		printf("    offset: %u\n", buf.m.offset);
		
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i] == MAP_FAILED) {
            perror("映射缓冲区失败");
            close(fd);
            return -1;
        }
		printf("    buffers[%u]: %p\n\n", i, buffers[i]);
    }

    // 入队缓冲区
    for (i = 0; i < req.count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("入队缓冲区失败");
            close(fd);
            return -1;
        }
		printf("VIDIOC_QBUF %d successfully\n", i);
    }
	printf("\n\n");


    // 启动视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("启动视频流失败");
        close(fd);
        return -1;
    }
	printf("VIDIOC_STREAMON\n\n");

dequeue_buf: 
    // 读取一帧数据
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("出队缓冲区失败");
        close(fd);
        return -1;
    }
	clock_gettime(CLOCK_MONOTONIC, &curr_frame_ts);
	if (last_frame_ts.tv_sec != 0) {
		frame_period_ms = (curr_frame_ts.tv_sec - last_frame_ts.tv_sec)*1000 + (curr_frame_ts.tv_nsec - last_frame_ts.tv_nsec)/1e6;
	}
	last_frame_ts.tv_sec = curr_frame_ts.tv_sec;
	last_frame_ts.tv_nsec = curr_frame_ts.tv_nsec;
	printf("VIDIOC_DQBUF: frame_period=%.3fms\n", frame_period_ms);

	save_to_yuv(buffers[buf.index], buf.length);
	
	if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
		perror("入队缓冲区失败");
		close(fd);
		return -1;
	}
	printf("VIDIOC_QBUF\n\n");
	
	goto dequeue_buf;

    // 停止视频流
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("停止视频流失败");
    }

    // 释放缓冲区
    for (i = 0; i < req.count; i++) {
        munmap(buffers[i], buf.length);
    }

    // 关闭设备
    close(fd);

    return 0;
}

void save_to_yuv(void *buffer, int len)
{
	static int i = 0;
	char filename[32] = {0};
	const char *output_dir = "/data/output";
	
	// 确保目录存在
    if (mkdir(output_dir, 0777) == -1 && errno != EEXIST) {
        perror("无法创建目录");
        return;
    }
	
	snprintf(filename, sizeof(filename), "%s/frame_%07d.yuv", output_dir, i);
	
	// 保存为 YUV 文件
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("无法创建文件");
        return ;
    }
	
	// 写入原始 YUV 数据
    fwrite(buffer, 1, len, file);
	
	// 关闭文件
    fclose(file);
	
    printf("SAVED：%s\n", filename);
	
	i++;
}

void handle_sigint(int sig) {

	printf("signal=%d\n", sig);
    // 退出程序
    exit(0);
}