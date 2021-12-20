
#include "huffman_generate.h"

unsigned char JPG_HEADER[56] = {
    // Start of Image
    0xff,0xd8,

    // Start of Frame
    0xff,0xc3,0x00,0x0E,
    0x0C, // Bitdepth
    0x0c,0x7a, // Height
    0x10,0x80, // Width
    0x02, // No. of Components
    0x00,0x11,0x00,
    0x01,0x11,0x00,

    // Define Huffman
    0xff,0xc4,0x00,0x24,
    0x00, // Table index 0
    // Node counts at each depth, start at 1-bit
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // Leaf array
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00
};

unsigned char JPG_SOS[12] = {
    // Start of Scan
    0xFF,0xDA,0x00,0x0A,
    0x02,
    0x00,0x00,
    0x01,0x00,
    // Predictor
    0x01,0x00,0x00
};

void update_jpg_header(unsigned short width, unsigned short height, unsigned char bitdepth) {
    JPG_HEADER[6] = bitdepth;
    JPG_HEADER[7] = height >> 8;
    JPG_HEADER[8] = height & 0xFF;
    JPG_HEADER[9] = (width >> 1) >> 8;
    JPG_HEADER[10] = (width >> 1) & 0xFF;
    // Huffman table length = 2 + Num_tables + Num_tables * (bitdepth + 1 + 16)
    JPG_HEADER[21] = 20 + bitdepth;
}

unsigned char nums_bits(unsigned short * val) {
    bool neg = *val >> 15;
    unsigned short test = *val;
    if (neg) {
        (*val)--;
        test = ~(*val);
    }
    unsigned char res = 0;
    while (test) {
        res++;
        test >>= 1;
    }
    return res;
}

int lossless_jpg(unsigned short * raw_buf, unsigned char * out_buf, unsigned short bitdepth,
        unsigned short width, unsigned short height) {
    unsigned int dpcm_hist[17] = {0}; // Reserved for 16 bit
    unsigned short prev_line[2];
    unsigned short default_predictor = 1 << (bitdepth - 1);
    prev_line[0] = default_predictor;
    prev_line[1] = default_predictor;
    unsigned short predictor[2];
    unsigned char * dpcm_len = malloc(width * height);
    for (int y = 0; y < height; y++) {
        memcpy(predictor, prev_line, 2 * 2);
        for (int x = 0; x < width; x++) {
            // Left justified 16 bit pixel
            unsigned short pix_val = raw_buf[width * y + x];
            short diff = pix_val - predictor[x & 1];
            unsigned char bits = nums_bits(&diff);
            predictor[x & 1] = pix_val;
            raw_buf[width * y + x] = diff;
            dpcm_len[width * y + x] = bits;
            dpcm_hist[bits]++;
            if (x < 2) {
                prev_line[x & 1] = predictor[x & 1];
            }
        }
    }

    unsigned char huff_lens[17];
    unsigned short huff_codes[17];
    update_huffman_tree(bitdepth, JPG_HEADER + 23, dpcm_hist, huff_codes, huff_lens);

#ifdef HUFFMAN_DEBUG
    puts("Diff\tCounts\tHuf_len\tHuffman Code");
    unsigned int total_bits = 0;
    for (int i = 0; i <= bitdepth; i++) {
        total_bits += (huff_lens[i] + i) * dpcm_hist[i];
        printf("%i\t%i\t%i\t", i, dpcm_hist[i], huff_lens[i]);
        for (int j = huff_lens[i] - 1; j >= 0; j--) {
            printf("%u", (huff_codes[i] >> j) & 1);
        }
        puts("");
    }
    for (int i = 0; i <= bitdepth; i++) {
        printf("%i ", JPG_HEADER[i + 23]);
    }
    puts("");
    for (int i = 0; i <= bitdepth; i++) {
        printf("%X ", JPG_HEADER[i + 23 + 16]);
    }

    printf("\nSize: %i Ratio: %f%%\n", total_bits >> 3,
        total_bits * 100.0/(width * height * bitdepth));
#endif

    update_jpg_header(width, height, bitdepth);
    int stream_idx = sizeof(JPG_HEADER) + bitdepth - 16;
    memcpy(out_buf, JPG_HEADER, stream_idx);
    memcpy(out_buf + stream_idx, JPG_SOS, sizeof(JPG_SOS));
    stream_idx += sizeof(JPG_SOS);

    unsigned char bits = 0;
    unsigned char leftover = 0;
    for (int pix = 0; pix < width * height; pix++) {
        unsigned char dpcm_bits = dpcm_len[pix];
        unsigned int pix_bits = raw_buf[pix] & ((1 << dpcm_bits) - 1);
        pix_bits |= huff_codes[dpcm_bits] << dpcm_bits;
        unsigned char pix_bitlen = dpcm_bits + huff_lens[dpcm_bits];

        do {
            unsigned char required_bits = 8 - bits;
            if (pix_bitlen >= required_bits) {
                unsigned char out_byte = leftover | (pix_bits >> (pix_bitlen - required_bits)) &
                    ((1 << required_bits) - 1);
                bits = 0;
                pix_bitlen -= required_bits;
                leftover = 0;
                out_buf[stream_idx] = out_byte;
                stream_idx++;
                if (out_byte == 0xFF) {
                    out_buf[stream_idx] = 0x00;
                    stream_idx++;
                }
            } else {
                leftover |= (pix_bits << (required_bits - pix_bitlen)) &
                    ((1 << required_bits) - 1);
                bits += pix_bitlen;
                pix_bitlen = 0;
            }
        } while (pix_bitlen);
    }
    out_buf[stream_idx] = leftover;
    out_buf[stream_idx + 1] = 0xFF;
    out_buf[stream_idx + 2] = 0xD9;
    free(dpcm_len);

    return stream_idx + 3;
}

