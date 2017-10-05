#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include<unistd.h>
#include<error.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<pthread.h>
#include<linux/videodev2.h>
#include<sys/mman.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<iostream>
#include<iomanip>
#include<string>


#define CLEAR(x)  memset(&(x),0,sizeof(x));

class V4L2Capture
{
    public:
        V4L2Capture(char *devName,int width,int height);
        virtual ~V4L2Capture();

        int openDevice();
        int closeDevice();
        int initDevice();
        int startCapture();
        int stopCapture();
        int freeBuffers();
        int getFrame(void**,size_t *);
        int backFrame();

        void v4l2control(uint32_t id,int32_t value);

        Mat V4l2CaptureRede();

        static void test();

    protected:
    private:
        int initBuffers();

        struct cam_buffers
        {
            void* start;
            unsigned int length;
        };
        char *devName;
        int capW;
        int capH;
        int fd_cam;
        cam_buffers *buffers;
        unsigned int n_buffers;
        int frameIndex;

        struct v4l2_control control_s;
};

void VideoPlayer(void);

#endif // V4L2CAPTURE_H

#include <V4L2Capture.h>
using namespace std;
using namespace cv;

V4L2Capture::V4L2Capture(char *devName,int width,int height)
{
    this->devName = devName;
    this->fd_cam = -1;
    this->buffers = NULL;
    this->n_buffers = 0;
    this->frameIndex = -1;
    this->capW = width;
    this->capH = height;


}

V4L2Capture::~V4L2Capture()
{

}

int V4L2Capture::openDevice()
{
    cout << "Video dev : "<< devName << endl;
    fd_cam = open(devName,O_RDWR);
    if(fd_cam < 0)
    {
        perror("Can't open video device");
    }
    return 0;
}

int V4L2Capture::closeDevice()
{
    if(fd_cam > 0)
    {
        int ret = 0;
        if((ret = close(fd_cam)) < 0)
        {
            perror("Can't close video device");
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

int V4L2Capture::initDevice()
{
    int ret;
    struct v4l2_format cam_format;                //设置摄像头的视频制式，帧格式
    struct v4l2_capability cam_cap;               //显示设备信息
    struct v4l2_cropcap cam_cropcap;         //设置摄像头的捕捉能力
    struct v4l2_fmtdesc cam_fmtdesc;          //查询所有支持的格式
    struct v4l2_crop cam_crop;                        //图像缩放

    /*获取摄像头的基本信息*/
    ret = ioctl(fd_cam,VIDIOC_QUERYCAP,&cam_cap);
    if( ret < 0)
    {
        perror("Can't get device informatiion : VIDIOCGCAP");
    }
    //cout<< " Driver Name: "<< cam_cap.driver << endl << " Card Name: " << cam_cap.card <<endl << " Bus info: "<<cam_cap.bus_info<<endl<<"Driver Version: "<< (cam_cap.version >> 16)&0xFF <<" ,"<< (cam_cap.version >> 8)&0xFF<<","<(cam_cap.version & 0xFF) << endl;
    	printf(
			"Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",
			cam_cap.driver, cam_cap.card, cam_cap.bus_info,
			(cam_cap.version >> 16) & 0XFF, (cam_cap.version >> 8) & 0XFF,
             cam_cap.version & 0XFF);

    /*获取摄像头所有支持的格式*/
    cam_fmtdesc.index = 0;
    cam_fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cout<<  "Support format: "<<endl;
    while(ioctl(fd_cam,VIDIOC_ENUM_FMT,&cam_fmtdesc) != -1)
    {
        cout << endl<< cam_fmtdesc.index + 1 << endl<<cam_fmtdesc.description<<endl;
        cam_fmtdesc.index++;
    }

    /*获取摄像头捕捉能力*/
    cam_cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0 == ioctl(fd_cam,VIDIOC_CROPCAP,&cam_cropcap))
    {
        cout<<"Default rec: "<<cam_cropcap.defrect.left<<" left: "<<cam_cropcap.defrect.top<<"widith: "<<cam_cropcap.defrect.width<<"height:"<<cam_cropcap.defrect.height;
        /*获得摄像头的窗口取景参数*/
        cam_crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam_crop.c = cam_cropcap.defrect;
        if(-1 == ioctl(fd_cam,VIDIOC_S_CROP,&cam_crop))
        {
            cout<< "Can't set crop para"<<endl;
        }
    }
    else
    {
        cout << "Can't set crop para"<<endl;
    }

    /*设置摄像头帧信息*/
    cam_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_format.fmt.pix.width = capW;
    cam_format.fmt.pix.height = capH;
    cam_format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    cam_format.fmt.pix.field = V4L2_FIELD_INTERLACED;
    ret = ioctl(fd_cam,VIDIOC_S_FMT,&cam_format);
    if(ret < 0)
    {
        perror("Can't set frame information");
    }
    cout << "Current data format information: "<<"width"<<cam_format.fmt.pix.width<<"height"<<cam_format.fmt.pix.height<<endl;
    ret = initBuffers();
    if(ret < 0)
    {
        perror("Buffers init error");
    }
    return 0;
}

int V4L2Capture::initBuffers()
{
    int ret;
    /*申请帧缓冲*/
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd_cam,VIDIOC_REQBUFS,&req);
    if(ret < 0)
    {
        perror("Request frame bffers failed");
    }
    if(req.count < 2)
    {
        perror("Request frame buffers while insufficient buffer memory");
    }
    buffers = (struct cam_buffers* )calloc(req.count,sizeof(*buffers));
    if(!buffers)
    {
        perror("Out of memory");
    }
    for(n_buffers = 0;n_buffers<req.count;n_buffers++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        //查询序号为n_buffers 的缓冲，得到其物理地址和大小
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        ret = ioctl(fd_cam,VIDIOC_QUERYBUF,&buf);
        if(ret < 0)
        {
            cout << "VIDIOC_QUERYBUF :  "<<n_buffers<<" failed "<<endl;
            return -1;
        }
        buffers[n_buffers].length = buf.length;
        //映射内存
        buffers[n_buffers].start = mmap(NULL,buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd_cam,buf.m.offset);
        if(MAP_FAILED == buffers[n_buffers].start)
        {
            cout <<"mmap buffer: "<<n_buffers<<"failed"<<endl;
            return -1;
        }
    }
    return 0;
}

int V4L2Capture::startCapture()
{
    unsigned int i;
    for( i = 0;i<n_buffers;i++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == ioctl(fd_cam,VIDIOC_QBUF,&buf))
        {
            cout << "VIDIOC_QBUF: "<<i<<"failed"<<endl;
            return -1;
        }
    }
    enum v4l2_buf_type type;
    type =V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd_cam,VIDIOC_STREAMON,&type))
    {
        cout<<"VIDIOC_QBUF buffer "<< i <<"failed"<<endl;
        return -1;
    }
    return 0;
}

int V4L2Capture::stopCapture()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd_cam,VIDIOC_STREAMOFF,&type))
    {
        cout<<"VIDIOC_STREAMOFF error"<<endl;
        return -1;
    }
    return 0;
}

int V4L2Capture::freeBuffers()
{
    unsigned int i;
    for(i = 0;i<n_buffers;++i)
    {
        if(-1 == munmap(buffers[i].start,buffers[i].length))
        {
            cout<<"munmap buffers: "<<i<<"faild"<<endl;
            return -1;
        }
        free(buffers);
        return 0;
    }
}

int V4L2Capture::getFrame(void **frame_buf,size_t* len)
{
    struct v4l2_buffer queue_buf;
    CLEAR(queue_buf);
    queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queue_buf.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(fd_cam,VIDIOC_DQBUF,&queue_buf))
    {
        cout << "VIDIOC_DQBUF error"<<endl;
        return -1;
    }
    *frame_buf = buffers[queue_buf.index].start;
    *len = buffers[queue_buf.index].length;
    frameIndex = queue_buf.index;
    return 0;
}


int V4L2Capture::backFrame()
{
    if(frameIndex != -1)
    {
        struct v4l2_buffer queue_buf;
        CLEAR(queue_buf);
        queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queue_buf.memory = V4L2_MEMORY_MMAP;
        queue_buf.index = frameIndex;
        if(-1 == ioctl(fd_cam,VIDIOC_QBUF,&queue_buf))
        {
            cout<<"VIDIOC_QBUF error"<<endl;
            return -1;
        }
        return 0;
    }
    return -1;
}

void V4L2Capture::test()
{
    unsigned char *yuvframe = NULL;
    unsigned long *yuvframeSize = 0;

    string videoDev = "/dev/video0";
    V4L2Capture *vcap = new V4L2Capture(const_cast<char*>(videoDev.c_str()),1920,1080);
    vcap->openDevice();
    vcap->initDevice();
    vcap->startCapture();
    vcap->getFrame((void** )&yuvframe,(size_t*)&yuvframeSize);

    vcap->backFrame();
    vcap->freeBuffers();
    vcap->closeDevice();
}

void V4L2Capture::v4l2control(uint32_t id,int32_t value)
{
    control_s.value = value;
    control_s.id = id;
    ioctl(fd_cam,VIDIOC_S_CTRL,&control_s);
}

void VideoPlayer()
{
    unsigned char *yuv422frame = NULL;
    unsigned long yuvframeSize = 0;

    string videoDev = "/dev/video0";
    V4L2Capture vcap(const_cast<char*>(videoDev.c_str()),640,480);
    vcap.openDevice();
    vcap.initDevice();
    vcap.startCapture();
    vcap.v4l2control(V4L2_CID_SATURATION,30);                  //设置饱和度
    vcap.v4l2control(V4L2_CID_CONTRAST,30);                      //设置对比度
    vcap.v4l2control(V4L2_CID_SHARPNESS,30);                    //设置清晰度
    vcap.v4l2control(V4L2_CID_GAMMA,30);                           //设置成类
    vcap.v4l2control(V4L2_CID_EXPOSURE_AUTO,V4L2_EXPOSURE_APERTURE_PRIORITY); //设置曝光度
    cvNamedWindow("Capture",CV_WINDOW_AUTOSIZE);
    Mat image;
    double t = 0;
    double fps = 0;
    char fpsCountString[10];
    while(1)
    {
        t = (double)getTickCount();
        vcap.getFrame((void**)&yuv422frame,(size_t*)&yuvframeSize);
        Mat img(640,480,CV_8UC3,yuv422frame);
        image =  imdecode(img,1);
        vcap.backFrame();
        t = (double)(getTickCount() - t)/getTickFrequency();
        fps = 1/t;
        sprintf(fpsCountString,"%.2f",fps);
        string fpsString("FPS: ");
        fpsString += fpsCountString;
        putText(image,fpsString,Point(5,20),FONT_HERSHEY_SIMPLEX,0.5,Scalar(255,125,0));
        if(waitKey(1) == 'q')
        {
            break;
        }
        imshow("Capture",image);
    }
    vcap.stopCapture();
    vcap.freeBuffers();
    vcap.closeDevice();
}

#include<V4L2Capture.h>

using namespace cv;

int main(int argc, char *argv[])
{
    VideoPlayer();
    return 0;
}





