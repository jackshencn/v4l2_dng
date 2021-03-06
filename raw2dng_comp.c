
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lossless_jpeg.h"

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
    0x01,0x02,0x1A,0xC6,0x03,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1D,0xC6,
    0x03,0x00,0x01,0x00,0x00,0x00,0xFF,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void update_dng_header(unsigned short width, unsigned short height, unsigned char bitdepth,
        bool comp, unsigned int compressed_length) {
    *(unsigned short*) (DNG_HEADER + 0x66) = width;
    *(unsigned short*) (DNG_HEADER + 0x72) = height;
    DNG_HEADER[0x7E] = bitdepth;
    // Black level
    *(unsigned short*) (DNG_HEADER + 0xEA) = 15;
    // White level
    *(unsigned short*) (DNG_HEADER + 0xF6) = (1 << bitdepth) - 1;
    if (comp) {
        DNG_HEADER[0x8A] = 7;
        *(unsigned int*) (DNG_HEADER + 0xBA) = compressed_length;
    } else {
        *(unsigned int*) (DNG_HEADER + 0xBA) = (width * height * bitdepth) >> 3;
    }
}

int main(int argc, char **argv) {
    int res;
    unsigned short width, height;
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    unsigned char bitdepth = atoi(argv[3]);
    char * raw_fname = argv[4];
    char * dng_fname = argv[5];
    FILE * raw_file = fopen(raw_fname, "rb");
    unsigned short * raw_buf = malloc(width * height * sizeof(unsigned short));
    fread(raw_buf, sizeof(unsigned short), width * height, raw_file);
    unsigned char * out_buf = malloc((width * height * bitdepth) >> 3);

    for (int i = 0; i < width * height; i++) {
        raw_buf[i] = __bswap_16(raw_buf[i]) >> 4;
    }
    int jpeg_size = lossless_jpg(raw_buf, out_buf, bitdepth, width, height);
    free(raw_buf);

    FILE * outfile = fopen(dng_fname, "wb");
    update_dng_header(width, height, bitdepth, true, jpeg_size);
    fwrite(DNG_HEADER, 1, sizeof(DNG_HEADER), outfile);
    fwrite(out_buf, 1, jpeg_size, outfile);
    free(out_buf);
    fclose(outfile);

    return 0;
}

