#include "main.h"

#include "../debug.h"
#include "../macros.h"

#include "../av/video.h" // video super globals

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

int utox_v4l_fd = -1;

#include <sys/mman.h>
#if defined(__linux__) || defined(__FreeBSD__) || defined(__kFreeBSD__) || defined(__DragonFly__) // FreeBSD and DragonFlyBSD will have the proper includes after installing v4l_compat
#include <linux/videodev2.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) // OpenBSD and NetBSD have V4L in base
#include <sys/videoio.h>
#else
#error "Unsupported platform for V4L"
#endif

#ifndef NO_V4LCONVERT
#include <libv4lconvert.h>
#endif

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fh, unsigned long request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

struct buffer {
    void * start;
    size_t length;
};

static struct buffer *buffers;
static uint32_t       n_buffers;

#ifndef NO_V4LCONVERT
static struct v4lconvert_data *v4lconvert_data;
#endif

static struct v4l2_format fmt;
static struct v4l2_format dest_fmt;

bool v4l_init(char *dev_name) {
    // utox_v4l_fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    utox_v4l_fd = open(dev_name, O_RDWR /* required */, 0);

#if 0
    if (dev_name)
    {
        LOG_ERR("v4l", "video device name=%s", dev_name);
    }
#endif

    if (utox_v4l_fd == -1) {
        LOG_ERR("v4l", "Cannot open '%s': %d, %s" , dev_name, errno, strerror(errno));
        return 0;
    }

    struct v4l2_capability cap;
    struct v4l2_cropcap    cropcap;
    struct v4l2_crop       crop;
    unsigned int           min;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            LOG_ERR("v4l", "%s is no V4L2 device" , dev_name);
        } else {
            LOG_ERR("v4l", "VIDIOC_QUERYCAP error %d, %s" , errno, strerror(errno));
        }
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERR("v4l", "%s is no video capture device" , dev_name);
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_ERR("v4l", "%s does not support streaming i/o" , dev_name);
        return 0;
    }

    /* Select video input, video standard and tune here. */
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(utox_v4l_fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c    = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_S_CROP, &crop)) {
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

#ifndef NO_V4LCONVERT
    LOG_ERR("v4l", "V4LCONVERT: calling v4lconvert_create()");
    v4lconvert_data = v4lconvert_create(utox_v4l_fd);
#endif

    CLEAR(fmt);
    CLEAR(dest_fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    // fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;

#if 0
    // --------- 720p camera video ---------
    fmt.fmt.pix.width = 1280;
    fmt.fmt.pix.height = 720;
    // --------- 720p camera video ---------
#endif

    dest_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dest_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    dest_fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_S_FMT, &fmt)) {
        LOG_ERR("v4l", "ERR:VIDIOC_S_FMT error %d, %s" , errno, strerror(errno));
    }

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_G_FMT, &fmt)) {
        LOG_ERR("v4l", "ERR:VIDIOC_G_FMT error %d, %s" , errno, strerror(errno));
        return 0;
    }

    if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
    {
        LOG_ERR("v4l", "Video format(got): V4L2_PIX_FMT_YUV420");
    }
    else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
    {
        LOG_ERR("v4l", "Video format(got): V4L2_PIX_FMT_MJPEG");
    }
    else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_H264)
    {
        LOG_ERR("v4l", "H264 HW encoding would be supported");
    }
    else
    {
        LOG_ERR("v4l", "Video format(got): %u", fmt.fmt.pix.pixelformat);
    }

    LOG_ERR("v4l", "got video format: %u %u %u %u bytesperline=%d sizeimage=%d", fmt.fmt.pix.width, fmt.fmt.pix.height,
                fmt.fmt.pix.pixelformat, fmt.fmt.pix.field,
                (int)fmt.fmt.pix.bytesperline,
                (int)fmt.fmt.pix.sizeimage);

    video_width             = fmt.fmt.pix.width;
    video_height            = fmt.fmt.pix.height;
    dest_fmt.fmt.pix.width  = fmt.fmt.pix.width;
    dest_fmt.fmt.pix.height = fmt.fmt.pix.height;
    LOG_ERR("v4l", "Video size: %u %u" , video_width, video_height);


    /* Buggy driver paranoia. */
    /*
    min = fmt.fmt.pix.width;
    if (fmt.fmt.pix.bytesperline < min)
    {
        LOG_ERR("v4l", "Video format(correcting bytesperline): %d", (int)min);
        fmt.fmt.pix.bytesperline = min;
    }

    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
    {
        LOG_ERR("v4l", "Video format(correcting sizeimage): %d", (int)min);
        fmt.fmt.pix.sizeimage = min;
    }
    */

    // HINT: set camera device fps -----------------------
    struct v4l2_streamparm *setfps;
    setfps = (struct v4l2_streamparm *)calloc(1, sizeof(struct v4l2_streamparm));

    if (setfps)
    {
        LOG_ERR("v4l", "trying to set 25 fps for video capture ...");
        memset(setfps, 0, sizeof(struct v4l2_streamparm));
        setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        setfps->parm.capture.timeperframe.numerator = 1;
        setfps->parm.capture.timeperframe.denominator = 25; // try to set ~25fps

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_S_PARM, setfps))
        {
            LOG_ERR("v4l", "ERR:VIDIOC_S_PARM Error");
        }

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_G_PARM, setfps))
        {
            LOG_ERR("v4l", "ERR:VIDIOC_G_PARM Error");
        }
        LOG_ERR("v4l", "VIDIOC_G_PARM:fps got=%d / %d",
            (int)setfps->parm.capture.timeperframe.numerator,
            (int)setfps->parm.capture.timeperframe.denominator);

        free(setfps);
        setfps = NULL;
    }

    // HINT: set camera device fps -----------------------




    /* part 3*/
    // uint32_t buffer_size = fmt.fmt.pix.sizeimage;
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            LOG_ERR("v4l", "%s does not support x i/o" , dev_name);
        } else {
            LOG_ERR("v4l", "VIDIOC_REQBUFS error %d, %s" , errno, strerror(errno));
        }
        return 0;
    }

    if (req.count < 2) {
        LOG_FATAL_ERR(EXIT_MALLOC, "v4l", "Insufficient buffer memory on %s", dev_name);
    }

    // TODO: this seems to leak sometimes! please fix me!
    buffers = calloc(req.count, sizeof(*buffers));

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers;

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_QUERYBUF, &buf)) {
            LOG_ERR("v4l", "VIDIOC_QUERYBUF error %d, %s" , errno, strerror(errno));
            return 0;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start  = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */,
                                        MAP_SHARED /* recommended */, utox_v4l_fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
            LOG_ERR("v4l", "mmap error %d, %s" , errno, strerror(errno));
            return 0;
        }
    }

    /*buffers = calloc(4, sizeof(*buffers));

    if (!buffers) {
        LOG_TRACE("v4l", "Out of memory" );
        return 0;
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = malloc(buffer_size);

        if (!buffers[n_buffers].start) {
            LOG_TRACE("v4l", "Out of memory" );
            return 0;
        }
    }*/
    LOG_ERR("v4l", "v4l_init:return 1");
    return 1;
}

void v4l_close(void) {
    size_t i;
    for (i = 0; i < n_buffers; ++i) {
        if (-1 == munmap(buffers[i].start, buffers[i].length)) {
            LOG_TRACE("v4l", "munmap error" );
        }
    }

    free(buffers);
    buffers = NULL;

    close(utox_v4l_fd);
}

bool v4l_startread(void) {
    LOG_TRACE("v4l", "start webcam" );
    size_t i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;
        buf.index  = i;
        // buf.m.userptr = (unsigned long)buffers[i].start;
        // buf.length = buffers[i].length;

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_QBUF, &buf)) {
            LOG_TRACE("v4l", "VIDIOC_QBUF error %d, %s" , errno, strerror(errno));
            return 0;
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(utox_v4l_fd, VIDIOC_STREAMON, &type)) {
        LOG_TRACE("v4l", "VIDIOC_STREAMON error %d, %s" , errno, strerror(errno));
        return 0;
    }

    return 1;
}

bool v4l_endread(void) {
    LOG_TRACE("v4l", "stop webcam" );
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(utox_v4l_fd, VIDIOC_STREAMOFF, &type)) {
        LOG_TRACE("v4l", "VIDIOC_STREAMOFF error %d, %s" , errno, strerror(errno));
        return 0;
    }

    return 1;
}

int v4l_getframe(uint8_t *y, uint8_t *UNUSED(u), uint8_t *UNUSED(v), uint16_t width, uint16_t height) {
    if (width != video_width || height != video_height) {
        LOG_ERR("V4L", "width/height mismatch %u %u != %u %u" , width, height, video_width, video_height);
        return 0;
    }

    struct v4l2_buffer buf;
    // unsigned int i;

    CLEAR(buf);

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;

    if (-1 == ioctl(utox_v4l_fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EINTR:
            case EAGAIN:
                LOG_ERR("v4l", "VIDIOC_DQBUF [1] error %d, %s" , errno, strerror(errno));
                return 0;

            case EIO:
            /* Could ignore EIO, see spec. */

            default:
                LOG_ERR("v4l", "VIDIOC_DQBUF [2] error %d, %s" , errno, strerror(errno));
                return -1;
        }
    }

    /*for (i = 0; i < n_buffers; ++i)
        if (buf.m.userptr == (unsigned long)buffers[i].start
                && buf.length == buffers[i].length)
            break;

    if(i >= n_buffers) {
        LOG_TRACE("v4l", "fatal error" );
        return 0;
    }*/

    void *data = (void *)buffers[buf.index].start; // length = buf.bytesused //(void*)buf.m.userptr

/* assumes planes are continuous memory */
#ifndef NO_V4LCONVERT
    int result = v4lconvert_convert(v4lconvert_data, &fmt, &dest_fmt, data, buf.bytesused, y,
                                    (video_width * video_height * 3) / 2);
    if (result == -1) {
        LOG_ERR("v4l", "v4lconvert_convert error %s" , v4lconvert_get_error_message(v4lconvert_data));
    }
#else
    if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
        yuv422to420(y, u, v, data, video_width, video_height);
    } else {
    }
#endif

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_QBUF, &buf)) {
        LOG_ERR("v4l", "VIDIOC_QBUF error %d, %s" , errno, strerror(errno));
    }

#ifndef NO_V4LCONVERT
    int res = (result == -1 ? 0 : 1);
    // LOG_ERR("v4l", "res = %d result = %d" , (int)res, (int) result);
    return res;
#else
    return 1;
#endif
}
