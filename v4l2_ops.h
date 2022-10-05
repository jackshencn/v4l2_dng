#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h> 
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

typedef struct {
    void   *start;
    size_t  length;
} membuf_t;

int v4l2_xioctl(int fh, int request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

int v4l2_set_ctrl(int fd, uint32_t id, uint32_t val) {
    struct v4l2_control v4l2_ctl;
    v4l2_ctl.id = id;
    v4l2_ctl.value = val;
    return v4l2_xioctl(fd, VIDIOC_S_CTRL, &v4l2_ctl);
}

int v4l2_get_ctrl(int fd, uint32_t id, uint32_t * val) {
    struct v4l2_control v4l2_ctl;
    v4l2_ctl.id = id;
    int res = v4l2_xioctl(fd, VIDIOC_G_CTRL, &v4l2_ctl);
    *val = v4l2_ctl.value;
    return res;
}

int v4l2_start_capturing(int fd) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == v4l2_xioctl(fd, VIDIOC_STREAMON, &type)) {
        printf("error VIDIOC_STREAMON");
        return 1;
    } else {
        return 0;
    }
}

int v4l2_init_mmap(int fd, membuf_t * membuf) {
    struct v4l2_requestbuffers req;
    unsigned num_planes = 1;

    CLEAR(req);

    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == v4l2_xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, " not support memory mapping");
        } else {
            printf("errno VIDIOC_REQBUFS");
        }
        return 1;
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\\n");
        return 1;
    }

    for (int buf_idx = 0; buf_idx < req.count; ++buf_idx) {
        struct v4l2_plane planes[VIDEO_MAX_PLANES]={0};
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = buf_idx;
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;

        if (-1 == v4l2_xioctl(fd, VIDIOC_QUERYBUF, &buf))
            printf("VIDIOC_QUERYBUF");

        num_planes = buf.length;
        for(int i = 0 ; i < num_planes; i++) {
            membuf[buf_idx * num_planes + i].length = planes[i].length;
            membuf[buf_idx * num_planes + i].start = mmap(NULL,
                        planes[i].length,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        fd,planes[i].m.mem_offset
                );
            printf("map buf %i plane %i 0x%x @ 0x%lx\n", buf_idx, i,
                (uint32_t) membuf[buf_idx * num_planes + i].length,
                (uint64_t) membuf[buf_idx * num_planes + i].start);
            if (membuf[buf_idx * num_planes + i].start == MAP_FAILED) {
                fprintf(stderr, "mmap failed\n");
                return 1;
            }
        }
        if (-1 == v4l2_xioctl(fd, VIDIOC_QBUF, &buf))
            fprintf(stderr, "VIDIOC_QBUF\n");
    }

    return 0;
}

int v4l2_init_device(int fd, uint16_t width, uint16_t height) {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == v4l2_xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            printf("Not a V4L2 device\\n");
            return 1;
        } else {
            printf("errno VIDIOC_QUERYCAP");
            return 1;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Not support streaming i/o\\n");
        return 1;
    }

    /* Select video input, video standard and tune here. */

    CLEAR(cropcap);

    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (0 == v4l2_xioctl(fd, VIDIOC_G_CROP, &crop)) {
        crop.c.height = height;
        crop.c.width = width;
        crop.c.left = 0;
        crop.c.top = 0;

        if (-1 == v4l2_xioctl(fd, VIDIOC_S_CROP, &crop)) {
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
        printf("errno VIDIOC_CROPCAP");
        return 1;
    }
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    while( 0 == v4l2_xioctl(fd , VIDIOC_ENUM_FMT , &fmtdesc))
    {
        fmtdesc.index++;
    }
    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == v4l2_xioctl(fd, VIDIOC_G_FMT, &fmt))
        printf("errno VIDIOC_G_FMT");
    fmt.fmt.pix_mp.width       = width;
    fmt.fmt.pix_mp.height      = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SRGGB12;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_INTERLACED;

    if (-1 == v4l2_xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        printf("errno VIDIOC_S_FMT");
        return 1;
    }
    return 0;
}

int v4l2_open_device(char * dev_name) {
    return open(dev_name, O_RDWR | O_NONBLOCK, 0);
}
