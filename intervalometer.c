
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "lossless_jpeg.h"
#include "v4l2_ops.h"

unsigned char DNG_HEADER[256] = {
    0x49,0x49,0x2A,0x00,0x08,0x00,0x00,0x00,0x04,0x00,0x12,0xC6,0x01,0x00,0x04,0x00,
    0x00,0x00,0x01,0x01,0x00,0x00,0x13,0xC6,0x01,0x00,0x04,0x00,0x00,0x00,0x01,0x01,
    0x00,0x00,0x4A,0x01,0x04,0x00,0x01,0x00,0x00,0x00,0x50,0x00,0x00,0x00,0x14,0xC6,
    0x02,0x00,0x11,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x46,0x65,
    0x69,0x63,0x68,0x65,0x6E,0x20,0x49,0x4D,0x58,0x33,0x33,0x34,0x4C,0x51,0x52,0x00,
    0x0E,0x00,0xFE,0x00,0x04,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
    0x03,0x00,0x01,0x00,0x00,0x00,0x80,0x10,0x00,0x00,0x01,0x01,0x03,0x00,0x01,0x00,
    0x00,0x00,0x78,0x0C,0x00,0x00,0x02,0x01,0x01,0x00,0x01,0x00,0x00,0x00,0x0C,0x00,
    0x00,0x00,0x03,0x01,0x03,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x06,0x01,
    0x03,0x00,0x01,0x00,0x00,0x00,0x23,0x80,0x00,0x00,0x11,0x01,0x04,0x00,0x01,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,0x15,0x01,0x04,0x00,0x01,0x00,0x00,0x00,0x01,0x00,
    0x00,0x00,0x17,0x01,0x04,0x00,0x01,0x00,0x00,0x00,0x00,0x9A,0x34,0x01,0x1C,0x01,
    0x03,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x8D,0x82,0x03,0x00,0x02,0x00,
    0x00,0x00,0x02,0x00,0x02,0x00,0x8E,0x82,0x01,0x00,0x04,0x00,0x00,0x00,0x00,0x01,
    0x01,0x02,0x1A,0xC6,0x03,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x1D,0xC6,
    0x03,0x00,0x01,0x00,0x00,0x00,0xFF,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void update_dng_header(unsigned short width, unsigned short height, unsigned char bitdepth,
    unsigned int compressed_length) {
    *(unsigned short*) (DNG_HEADER + 0x66) = width;
    *(unsigned short*) (DNG_HEADER + 0x72) = height;
    DNG_HEADER[0x7E] = bitdepth;
    DNG_HEADER[0x8A] = 7;
    *(unsigned int*) (DNG_HEADER + 0xBA) = compressed_length;
    // Black level
    *(unsigned short*) (DNG_HEADER + 0xEA) = 15;
    // White level
    *(unsigned short*) (DNG_HEADER + 0xF6) = (1 << bitdepth) - 1;
}

#define TARGET_EXPOSURE 4000
#define LINE_TIME 14.81


int main(int argc, char **argv) {
    int res;
    unsigned char bitdepth = 12;
    unsigned short width, height;
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    float frame_gap = atof(argv[3]);
    unsigned int frac = atoi(argv[4]);
    int v4l2_fd = v4l2_open_device("/dev/video5");
    if (!v4l2_fd) {
        return -1;
    }

    if (v4l2_init_device(v4l2_fd, width, height)) {
        close(v4l2_fd);
        return -1;
    }

    unsigned int max_exposure = (unsigned int)(frame_gap * 2210 * 51);
    unsigned int vblk = max_exposure - height;
    v4l2_set_ctrl(v4l2_fd, V4L2_CID_VBLANK, vblk);

    membuf_t membuf[3];
    if (v4l2_init_mmap(v4l2_fd, membuf)) {
        close(v4l2_fd);
        return -1;
    }

    unsigned short * raw_buf = malloc(width * height * sizeof(unsigned short));
    unsigned char * out_buf = malloc((width * height * bitdepth) >> 3);
    unsigned int hist[4096];
    char dng_fname[64];

    unsigned int exposure_rows;
    v4l2_get_ctrl(v4l2_fd, V4L2_CID_EXPOSURE, &exposure_rows);
    int one_thousandths = width * height / frac;
    bool set_exposure = false;

    v4l2_start_capturing(v4l2_fd);

    // Begin V4L2 operation
    struct v4l2_plane planes[1];
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = 1;
    while (1) {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(v4l2_fd, &fds);

        /* Timeout. */
        tv.tv_sec = (unsigned int) frame_gap;
        tv.tv_usec = (unsigned int) (frame_gap * 1000000) % 1000000;

        r = select(v4l2_fd + 1, &fds, NULL, NULL, &tv);
        if (0 == r) {
            printf("Timeout\n");
        }

        while (v4l2_xioctl(v4l2_fd, VIDIOC_DQBUF, &buf) == -1) {
            switch (errno) {
                case EAGAIN:
                    continue;
                default:
                    printf("VIDIOC_DQBUF error\n");
                return true;
            }
        }

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        int j = 0;
        unsigned char bits = 0;
        unsigned short bitstream = 0;
        memset(hist, 0, 4096 * sizeof(unsigned int));
        unsigned short * memmap_pix = (unsigned short *) membuf[buf.index].start;
        for (int i = 0; i < width * height; i++) {
            // Left justified 16 bit pixel
            unsigned short pix_val = __bswap_16(*memmap_pix++) >> 4;
            hist[pix_val]++;
            raw_buf[i] = pix_val;
        }
        if (-1 == v4l2_xioctl(v4l2_fd, VIDIOC_QBUF, &buf)) {
            printf("VIDIOC_QBUF error\n");
            return -1;
        }
        int jpeg_size = lossless_jpg(raw_buf, out_buf, bitdepth, width, height);

        // Write file
        update_dng_header(width, height, bitdepth, jpeg_size);
        strftime(dng_fname, sizeof(dng_fname), "%Y%m%d_%H%M%S.DNG", tm);
        FILE * outfile = fopen(dng_fname, "wb");
        fwrite(DNG_HEADER, 1, sizeof(DNG_HEADER), outfile);
        fwrite(out_buf, 1, jpeg_size, outfile);
        fclose(outfile);
        printf("Save %s\n", dng_fname);

        if (!set_exposure) {
            // Analyze histogram
            float total_val = 0.0;
            int highlight_count = 0;
            for (int i = 4095; i >= 0; i--) {
                if (hist[i]) {
                    highlight_count += hist[i];
                    total_val += ((float) hist[i]) * i;
                    if (highlight_count > one_thousandths) {
                        total_val /= highlight_count;
                        printf("Highlight @%f\n", total_val);
                        break;
                    }
                }
            }

            // Calculate exposure
            if (total_val > 4090) {
                exposure_rows = exposure_rows >> 1;
            } else {
                exposure_rows = (unsigned int) (exposure_rows * TARGET_EXPOSURE / total_val);
            }
            if (exposure_rows >= max_exposure) {
                exposure_rows = max_exposure - 1;
            }
            set_exposure = true;
            printf("Set exposure %u\n", exposure_rows);
            v4l2_set_ctrl(v4l2_fd, V4L2_CID_EXPOSURE, exposure_rows);

            v4l2_get_ctrl(v4l2_fd, V4L2_CID_EXPOSURE, &exposure_rows);
        } else {
            set_exposure = false;
        }
    }
    free(raw_buf);
    free(out_buf);

    return 0;
}

