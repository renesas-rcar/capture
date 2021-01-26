/*
 * Camera test application
 *
 * Copyright (C) 2016-2017 Renesas Electronics Corporation
 * Copyright (C) 2016-2017 Cogent Embedded, Inc. <source@cogentembedded.com>
 *
 * based on:
 *  V4L2 video capture example
 *  This program is provided with the V4L2 API
 *  see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <linux/fb.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

//#define FIELD V4L2_FIELD_INTERLACED
#define FIELD V4L2_FIELD_NONE

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

struct buffer {
        void   *start;
        size_t  length;
};

#define N_DEVS_MAX      12
static char             n_devs = 1;
static char            *dev_name[N_DEVS_MAX] = {"/dev/video0","/dev/video1","/dev/video2","/dev/video3","/dev/video4","/dev/video5","/dev/video6","/dev/video7","/dev/video8","/dev/video9","/dev/video10","/dev/video11"};
static char            *fbdev_name;
static enum io_method   io = IO_METHOD_MMAP;
//static enum io_method   io = IO_METHOD_USERPTR;
static int              fd[N_DEVS_MAX] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int              fbfd = -1;
struct buffer          *buffers[N_DEVS_MAX];
static unsigned int     n_buffers[N_DEVS_MAX];
static int              out_buf, out_fb;
static char             *format_name;
static int              frame_count = 70;
static int              fps_count = 0;
static int              framerate = 0;
static int              timeout = 60; // secs
static int              LEFT = 0;
static int              TOP = 0;
static int              WIDTH = 1920;
static int              HEIGHT = 1080;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static long int screensize = 0;
static char *fbmem = 0;


static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

static inline unsigned long uSecElapsed(struct timeval *t2, struct timeval *t1)
{
        return (t2->tv_sec - t1->tv_sec) * 1000000 + t2->tv_usec - t1->tv_usec;
}

static void fpsCount(int dev)
{
        static unsigned frames[N_DEVS_MAX];
        static struct timeval frame_time[N_DEVS_MAX];
        static unsigned long usec[N_DEVS_MAX];
        struct timeval t;

        gettimeofday(&t, NULL);
        usec[dev] += frames[dev]++ ? uSecElapsed(&t, &frame_time[dev]) : 0;
        frame_time[dev] = t;
        if (usec[dev] >= 1000000) {
                unsigned fps = ((unsigned long long)frames[dev] * 10000000 + usec[dev] - 1) / usec[dev];
                fprintf(stderr, "%s FPS: %3u.%1u\n", dev_name[dev], fps / 10, fps % 10);
                usec[dev] = 0;
                frames[dev] = 0;
        }
}

static inline void yuv_to_rgb32(unsigned short y,
                                unsigned short u,
                                unsigned short v,
                                unsigned char *rgb)
{
    register int r,g,b;

    y = y >> 2;
    u = u >> 2;
    v = v >> 2;

    r = (1192 * (y - 16) + 1634 * (v - 128) ) >> 10;
    g = (1192 * (y - 16) - 833 * (v - 128) - 400 * (u -128) ) >> 10;
    b = (1192 * (y - 16) + 2066 * (u - 128) ) >> 10;

    r = r > 255 ? 255 : r < 0 ? 0 : r;
    g = g > 255 ? 255 : g < 0 ? 0 : g;
    b = b > 255 ? 255 : b < 0 ? 0 : b;

    *rgb++ = b; //B
    *rgb++ = g; //G
    *rgb++ = r; //R
    *rgb   = 0; //A
}

static void process_image(const void *p, int size, int dev)
{
        if (out_buf)
                fwrite(p, size, 1, stdout);

        if (out_fb) {
            if (!strncmp(format_name, "rgb32", 5) | !strncmp(format_name, "raw10", 5)) {
                /* for RGB32 from camera: no need any convertion */
                int i;
                int offset = (WIDTH*4)*(dev%(n_devs > 4 ? 4 : 2)) + (HEIGHT*finfo.line_length)*(dev/(n_devs > 4 ? 4 : 2));
                unsigned char *fbp = (unsigned char *)fbmem + offset;
                char *buf = (char *)p;

                for (i = 0; i < HEIGHT; i++) {
                        memcpy(fbp, buf, WIDTH*4);
                        fbp += finfo.line_length;
                        buf += (WIDTH*4);
                }
            } else if (!strncmp(format_name, "uyvy", 4)) {
                /* for UYVY from camera: covert UYVY to RGB32 */
                int i, j;
                unsigned char *fbp = (unsigned char *)fbmem;
                char *buf = (char *)p;
                unsigned short Y1, Y2, U, V;

                // assume bpp = 32
                for (i = 0; i < HEIGHT; i++) {
                        for (j = 0; j < WIDTH*4; j+=4) {
                                U = (buf[j + 1] << 8) + buf[j + 0];
                                Y1 = (buf[j + 3] << 8) + buf[j + 2];
                                V = (buf[j + 5] << 8) + buf[j + 4];
                                Y2 = (buf[j + 7] << 8) + buf[j + 6];

                                yuv_to_rgb32(Y1, U, V, &fbp[2*j]);
                                yuv_to_rgb32(Y2, U, V, &fbp[2*(j + 2)]);
                        }

                        fbp += finfo.line_length;
                        buf += (WIDTH*2);
                }
            } else if (!strncmp(format_name, "bggr8", 5)) {
            } else if (!strncmp(format_name, "bggr12", 6)) {
                int i, j, k;
                unsigned char *fbp = (unsigned char *)fbmem;
                char *buf = (char *)p;

                for (i = 0; i < HEIGHT; i++) {
                        k = 0;
                        for (j = 0; j < WIDTH; j+=3) {
                                if (i & 1) {
                                        /* GR row */
                                        fbp[k++] = buf[j-WIDTH]; //B
                                        fbp[k++] = buf[j+0]; //G
                                        fbp[k++] = buf[j+1]; //R
                                        fbp[k++] = 0; //A

                                        fbp[k++] = buf[j-WIDTH]; //B
                                        fbp[k++] = buf[j+0]; //G
                                        fbp[k++] = buf[j+1]; //R
                                        fbp[k++] = 0; //A
                                } else {
                                        /* BG row */
                                        fbp[k++] = buf[j+0]; //B
                                        fbp[k++] = buf[j+1]; //G
                                        fbp[k++] = buf[j+WIDTH+1]; //R
                                        fbp[k++] = 0; //A

                                        fbp[k++] = buf[j+0]; //B
                                        fbp[k++] = buf[j+1]; //G
                                        fbp[k++] = buf[j+WIDTH+1]; //R
                                        fbp[k++] = 0; //A
                                }
                        }

                        fbp += finfo.line_length;
                        buf += WIDTH;
                }
            } else {
                fprintf(stderr, "format not supported to stream to Framebuffer\n");
            }
        }
}

static int read_frame(int dev)
{
        struct v4l2_buffer buf;
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                if (-1 == read(fd[dev], (buffers[dev])[0].start, (buffers[dev])[0].length)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;
                        case EIO:
                                /* Could ignore EIO, see spec. */
                                /* fall through */
                        default:
                                errno_exit("read");
                        }
                }

                process_image((buffers[dev])[0].start, (buffers[dev])[0].length, dev);
                break;

        case IO_METHOD_MMAP:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;

                if (-1 == xioctl(fd[dev], VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;
                        case EIO:
                                /* Could ignore EIO, see spec. */
                                /* fall through */
                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                assert(buf.index < n_buffers[dev]);

                process_image((buffers[dev])[buf.index].start, buf.bytesused, dev);

                if (-1 == xioctl(fd[dev], VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;

        case IO_METHOD_USERPTR:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;

                if (-1 == xioctl(fd[dev], VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;
                        case EIO:
                                /* Could ignore EIO, see spec. */
                                /* fall through */
                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                for (i = 0; i < n_buffers[dev]; ++i)
                        if (buf.m.userptr == (unsigned long)(buffers[dev])[i].start
                            && buf.length == (buffers[dev])[i].length)
                                break;

                assert(i < n_buffers[dev]);

                process_image((void *)buf.m.userptr, buf.bytesused, dev);

                if (-1 == xioctl(fd[dev], VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;
        }

        if (fps_count)
                fpsCount(dev);

        return 1;
}

#define max(a,b) (a>b?a:b)

static void mainloop(void)
{
        unsigned int count = frame_count;
        int dev = 0;
        fd_set fds;
        struct timeval tv;
        int r;

        /* Give time to queue buffers at start streaming by VIN module */
//        usleep(34000*3);

        while (count-- > 0) {
                for (;;) {
                        FD_ZERO(&fds);

                        for (dev = 0; dev < n_devs; dev++)
                                FD_SET(fd[dev], &fds);
                        /* Timeout. */
                        tv.tv_sec = timeout;
                        tv.tv_usec = 0;

                        r = select(max(max(max(max(fd[0],fd[1]),max(fd[2],fd[3])),max(max(fd[4],fd[5]),max(fd[6],fd[7]))),max(max(fd[8],fd[9]),max(fd[10],fd[11]))) + 1, &fds, NULL, NULL, &tv);
//                        r = select((max(max(max(fd[0],fd[1]),max(fd[2],fd[3])),max(max(fd[4],fd[5]),max(fd[6],fd[7]))),max(max(fd[8],fd[9]),max(fd[10],fd[11]))) + 1, &fds, NULL, NULL, &tv);
                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;
                                errno_exit("select");
                        }

                        if (0 == r) {
                                fprintf(stderr, "select timeout\n");
                                exit(EXIT_FAILURE);
                        }

                        r = 0;
                        for (dev = 0; dev < n_devs; dev++) {
                                if (FD_ISSET(fd[dev], &fds))
                                        r += read_frame(dev);
//                                        usleep(30000);
                        }
                        if (r)
                                break;
                        /* EAGAIN - continue select loop. */
                }
        }
}

static void stop_capturing(int dev)
{
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd[dev], VIDIOC_STREAMOFF, &type))
                        errno_exit("VIDIOC_STREAMOFF");
                break;
        }
}

static void start_capturing(int dev)
{
        unsigned int i;
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers[dev]; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (-1 == xioctl(fd[dev], VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd[dev], VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers[dev]; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_USERPTR;
                        buf.index = i;
                        buf.m.userptr = (unsigned long)(buffers[dev])[i].start;
                        buf.length = (buffers[dev])[i].length;

                        if (-1 == xioctl(fd[dev], VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd[dev], VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;
        }
}

static void uninit_device(int dev)
{
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                free((buffers[dev])[0].start);
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers[dev]; ++i)
                        if (-1 == munmap((buffers[dev])[i].start, (buffers[dev])[i].length))
                                errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers[dev]; ++i)
                        free((buffers[dev])[i].start);
                break;
        }

        free(buffers[dev]);
}

static void init_read(unsigned int buffer_size, int dev)
{
        buffers[dev] = calloc(1, sizeof(*buffers[dev]));

        if (!buffers[dev]) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        (buffers[dev])[0].length = buffer_size;
        (buffers[dev])[0].start = malloc(sizeof(unsigned char) * buffer_size);

        if (!(buffers[dev])[0].start) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }
}

static void init_mmap(int dev)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 7;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd[dev], VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name[dev]);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name[dev]);
                exit(EXIT_FAILURE);
        }

        buffers[dev] = calloc(req.count, sizeof(*buffers[dev]));

        if (!buffers[dev]) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers[dev] = 0; n_buffers[dev] < req.count; ++n_buffers[dev]) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers[dev];

                if (-1 == xioctl(fd[dev], VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

                (buffers[dev])[n_buffers[dev]].length = buf.length;
                (buffers[dev])[n_buffers[dev]].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd[dev], buf.m.offset);

                if (MAP_FAILED == (buffers[dev])[n_buffers[dev]].start)
                        errno_exit("mmap");
        }
}

static void init_userp(unsigned int buffer_size, int dev)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(fd[dev], VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name[dev]);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        buffers[dev] = calloc(4, sizeof(*buffers[dev]));

        if (!buffers[dev]) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers[dev] = 0; n_buffers[dev] < 4; ++n_buffers[dev]) {
                (buffers[dev])[n_buffers[dev]].length = buffer_size;
                (buffers[dev])[n_buffers[dev]].start = malloc(buffer_size);

                if (!(buffers[dev])[n_buffers[dev]].start) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                }
        }
}

static void init_device(int dev)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;

        if (-1 == xioctl(fd[dev], VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name[dev]);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\n",
                         dev_name[dev]);
                exit(EXIT_FAILURE);
        }

        switch (io) {
        case IO_METHOD_READ:
                if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                        fprintf(stderr, "%s does not support read i/o\n",
                                 dev_name[dev]);
                        exit(EXIT_FAILURE);
                }
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                        fprintf(stderr, "%s does not support streaming i/o\n",
                                 dev_name[dev]);
                        exit(EXIT_FAILURE);
                }
                break;
        }


        /* Select video input, video standard and tune here. */
        CLEAR(cropcap);
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 == xioctl(fd[dev], VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                crop.c.left = LEFT;
                crop.c.top = TOP;
                crop.c.width = WIDTH;
                crop.c.height = HEIGHT;

                if (-1 == xioctl(fd[dev], VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }

        if (framerate) {
            struct v4l2_streamparm parm;

            parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl(fd[dev], VIDIOC_G_PARM, &parm))
                errno_exit("VIDIOC_G_PARM");

            parm.parm.capture.timeperframe.numerator = 1;
            parm.parm.capture.timeperframe.denominator = framerate;
            if (-1 == xioctl(fd[dev], VIDIOC_S_PARM, &parm))
                errno_exit("VIDIOC_S_PARM");
        }

#if 0
    struct v4l2_control control;

    memset(&control, 0, sizeof (control));
    control.id = V4L2_CID_BRIGHTNESS;
//    control.id = V4L2_CID_VFLIP;
//    control.id = V4L2_CID_CONTRAST;
//    control.id = V4L2_CID_SATURATION;
//    control.id = V4L2_CID_GAMMA;
//    control.id = V4L2_CID_GAIN;
//    control.id = V4L2_CID_EXPOSURE;
//    control.id = V4L2_CID_AUTOGAIN;

    if (-1 == xioctl(fd[dev], VIDIOC_G_CTRL, &control))
        errno_exit("VIDIOC_G_CTRL");

    control.value = 0xf0;

    if (-1 == ioctl(fd[dev], VIDIOC_S_CTRL, &control))
        errno_exit("VIDIOC_S_CTRL");
#endif

        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width  = WIDTH;
        fmt.fmt.pix.height = HEIGHT;
        fmt.fmt.pix.field  = FIELD;

        if (!strncmp(format_name, "uyvy", 4))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
        else if (!strncmp(format_name, "yuyv", 4))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        else if (!strncmp(format_name, "rgb565", 6))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
        else if (!strncmp(format_name, "rgb32", 5))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_XBGR32;
        else if (!strncmp(format_name, "raw10", 5))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_Y10;
        else if (!strncmp(format_name, "nv12", 4))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
        else if (!strncmp(format_name, "nv16", 4))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV16;
        else if (!strncmp(format_name, "bggr8", 5))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
        else if (!strncmp(format_name, "bggr12", 6))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR12;
        else if (!strncmp(format_name, "grey", 4))
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        else {
                /* Preserve original settings as set by v4l2-ctl for example */
                if (-1 == xioctl(fd[dev], VIDIOC_G_FMT, &fmt))
                        errno_exit("VIDIOC_G_FMT");
        }

        if (-1 == xioctl(fd[dev], VIDIOC_S_FMT, &fmt))
                errno_exit("VIDIOC_S_FMT");

//        printf("fmt.fmt.pix.bytesperline =%d\n\n\n",fmt.fmt.pix.bytesperline);

        switch (io) {
        case IO_METHOD_READ:
                init_read(fmt.fmt.pix.sizeimage, dev);
                break;

        case IO_METHOD_MMAP:
                init_mmap(dev);
                break;

        case IO_METHOD_USERPTR:
                init_userp(fmt.fmt.pix.sizeimage, dev);
                break;
        }
}

static void close_device(int dev)
{
        if (-1 == close(fd[dev]))
                errno_exit("close");

        fd[dev] = -1;
}

static void open_device(int dev)
{
        struct stat st;

        if (-1 == stat(dev_name[dev], &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name[dev], errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name[dev]);
                exit(EXIT_FAILURE);
        }

        fd[dev] = open(dev_name[dev], O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd[dev]) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name[dev], errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}

static void open_fb(void)
{
        if (out_fb) {
                fbfd = open(fbdev_name, O_RDWR, 0);
                if (-1 == fbfd) {
                        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                                 fbdev_name, errno, strerror(errno));
                        exit(EXIT_FAILURE);
                }

                // Get fixed screen information
                if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
                        perror("Error reading fixed information");
                        exit(EXIT_FAILURE);
                }
                // Get variable screen information
                if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
                        perror("Error reading variable information");
                        exit(EXIT_FAILURE);
                }
//                printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

                printf("fb0 Fixed Info:\n"
                                "   %s  @ 0x%lx, len=%d, line=%d bytes,\n",
                        finfo.id,
                        finfo.smem_start,
                        finfo.smem_len,
                        finfo.line_length);

                printf("   Geometry - %u x %u, %u bpp%s\n",
                        vinfo.xres,
                        vinfo.yres,
                        vinfo.bits_per_pixel,
                        vinfo.grayscale ? ", greyscale" : "");


                // Figure out the size of the screen in bytes
                screensize = finfo.smem_len; //vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

                // Map the device to memory
                fbmem = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
                if (!fbmem) {
                        perror("Error: failed to map framebuffer device to memory");
                        exit(EXIT_FAILURE);
                }
        }
}

static void close_fb(void)
{
        if (out_fb) {
                munmap(fbmem, screensize);
                close(fbfd);
                fbfd = -1;
        }
}

static void usage(FILE *fp, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Version 1.3\n"
                 "Options:\n"
                 "-d | --device name   Video device name [%s]\n"
                 "-D | --ndev          Number of devices to capture simultaneously [%d]\n"
                 "-h | --help          Print this message\n"
                 "-m | --mmap          Use memory mapped buffers [default]\n"
                 "-r | --read          Use read() calls\n"
                 "-u | --userp         Use application allocated buffers\n"
                 "-o | --output        Outputs stream to stdout\n"
                 "-F | --output_fb     Outputs stream to framebuffer\n"
                 "-f | --format        Set pixel format: uyvy, yuyv, rgb565, rgb32, nv12, nv16, bggr8, grey [%s]\n"
                 "-c | --count         Number of frames to grab [%i]\n"
                 "-z | --fps_count     Enable fps show\n"
                 "-s | --framerate     Set framerate\n"
                 "-L | --left          Video left crop [%i]\n"
                 "-T | --top           Video top crop [%i]\n"
                 "-W | --width         Video width [%i]\n"
                 "-H | --height        Video height [%i]\n"
                 "-t | --timeout       Select timeout [%i]sec\n"
                 "",
                 argv[0], dev_name[0], n_devs, format_name, frame_count, LEFT, TOP, WIDTH, HEIGHT, timeout);
}

static const char short_options[] = "d:D:hmruoFf:c:zs:L:T:W:H:t:";

static const struct option
long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "ndev",   required_argument, NULL, 'D' },
        { "help",   no_argument,       NULL, 'h' },
        { "mmap",   no_argument,       NULL, 'm' },
        { "read",   no_argument,       NULL, 'r' },
        { "userp",  no_argument,       NULL, 'u' },
        { "output", no_argument,       NULL, 'o' },
        { "output_fb", no_argument,    NULL, 'F' },
        { "format", required_argument, NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { "fps_count",  required_argument, NULL, 'z' },
        { "framerate",  required_argument, NULL, 's' },
        { "left",  required_argument, NULL, 'L' },
        { "top",  required_argument, NULL, 'T' },
        { "width",  required_argument, NULL, 'W' },
        { "height",  required_argument, NULL, 'H' },
        { "timeout",  required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
        int dev;
        dev_name[0] = "/dev/video0";
        fbdev_name = "/dev/fb0";
        format_name = "uyvy";

        for (;;) {
                int idx;
                int c;

                c = getopt_long(argc, argv,
                                short_options, long_options, &idx);

                if (-1 == c)
                        break;

                switch (c) {
                case 0: /* getopt_long() flag */
                        break;

                case 'd':
                        dev_name[0] = optarg;
                        break;

                case 'D':
                        errno = 0;
                        n_devs = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'h':
                        usage(stdout, argv);
                        exit(EXIT_SUCCESS);

                case 'm':
                        io = IO_METHOD_MMAP;
                        break;

                case 'r':
                        io = IO_METHOD_READ;
                        break;

                case 'u':
                        io = IO_METHOD_USERPTR;
                        break;

                case 'o':
                        out_buf++;
                        break;

                case 'F':
                        out_fb++;
                        break;

                case 'f':
                        format_name = optarg;
                        break;

                case 'c':
                        errno = 0;
                        frame_count = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'z':
                        fps_count = 1;
                        break;

                case 's':
                        errno = 0;
                        framerate = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'L':
                        errno = 0;
                        LEFT = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'T':
                        errno = 0;
                        TOP = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'W':
                        errno = 0;
                        WIDTH = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 'H':
                        errno = 0;
                        HEIGHT = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                case 't':
                        errno = 0;
                        timeout = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                default:
                        usage(stderr, argv);
                        exit(EXIT_FAILURE);
                }
        }

        for (dev = 0; dev < n_devs; dev++) {
                open_device(dev);
                init_device(dev);
                start_capturing(dev);
        }
        open_fb();
        mainloop();
        close_fb();
        for (dev = 0; dev < n_devs; dev++) {
                stop_capturing(dev);
                uninit_device(dev);
                close_device(dev);
        }

        return 0;
}
