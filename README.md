# my_camera_drv
依照内核/samples/v4l/v4l2-pci-skeleton.c实现一个camera驱动，掌握v4l2框架的基本使用方法。

参考 dtsi.patch，在设备树中增加设备节点；

编译设备驱动：make

编译测试应用程序：make test_app

2025/04/14
	1）注意模块加载的依赖顺序： sensor依赖csi，camera依赖isp和csi，camera和sensor没有先后
		isp >> csi >> camera >> sensor
		isp >> csi >> sensor >> camera
		模块卸载顺序：必须先卸载camera
		camera >> sensor >> csi >> isp
	
	2）目前测试应用程序可以DQ到YUYV图像数据，保存在/data/output目录下，是白、红、橙、黄、绿、蓝、靛、紫、黑九种颜色的纯色图像。
	3）目前csi驱动将自己的帧缓冲区直接调用camera驱动的DMA传输接口（使用memcpy模拟），将数据写入vb2_buffer的帧缓冲区中。
	   当初的设计是csi->isp，isp中处理完再提交给camera的DMA传输接口，由于是模拟的数据就不做处理了，csi绕过isp直接提交至camera的v4l2框架。
	   其次，整个驱动中数据流转基本完整，还存在以下不足：
	   第一，csi驱动使用的DMA缓冲区只有一份，再高帧率环境下，考虑使用多缓冲区的ring buffer来管理；
	   第二，一旦启用isp，csi与isp之间的缓冲区存在同步问题（生产者与消费者的问题），考虑在csi与isp的共享ring buffer上加wait queue来实现同步；

2025/04/17
	1）加载/卸载模块：
		insmod /data/my_ringbuffer.ko;insmod /data/my_isp.ko;insmod /data/my_csi.ko;insmod /data/my_sensor.ko;insmod /data/my_camera.ko
		rmmod my_camera;rmmod my_sensor;rmmod my_csi;rmmod my_isp;rmmod my_ringbuffer
	2）运行测试程序：
		将 test_my_camera push 到 /data/
		cd /data;./test_my_camera
		获取到的帧会保存在 /data/output 文件夹中。

