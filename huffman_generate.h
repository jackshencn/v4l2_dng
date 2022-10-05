#include <stdlib.h>


// DPCM Huffman Generator
typedef struct {
    unsigned int count;
    unsigned char diff_len;	//255 for node
    unsigned char left;
    unsigned char right;
    unsigned char larger;
} HUFF_NODE;

int huff_node_cmp(const void * a, const void * b) {
    return ((HUFF_NODE*) a)->count - ((HUFF_NODE*) b)->count;
}

void depth_scan(HUFF_NODE * tree, unsigned char idx, unsigned char depth,
        unsigned char * ljpg_huff_depth_def) {
    if (tree[idx].left == 255){
        // The first element in Huffman header is 1-bit long
        ljpg_huff_depth_def[depth - 1]++;
        return;
    }
    depth++;
    depth_scan(tree, tree[idx].left, depth, ljpg_huff_depth_def);
    depth_scan(tree, tree[idx].right, depth, ljpg_huff_depth_def);
}

void huff_code_gen(unsigned char * ljpg_huff_depth_def, unsigned short * huff_codes,
        unsigned char * cumulative_idx) {
    int i, j = 0;
    unsigned short huffman_code = 0;
    for (int d = 0; d < 16; d++){
        cumulative_idx[d] = j;
        for (i = 0; i < ljpg_huff_depth_def[d]; i++){
            huff_codes[j] = huffman_code;
            j++;
            huffman_code++;
        }
        huffman_code <<= 1;
    }
}

void leaf_match_huff(HUFF_NODE * tree, unsigned char idx, unsigned char depth,
        unsigned char * cumulative_idx, unsigned char * ljpg_huff_leaf_def,
        unsigned short * huff_codes, unsigned char * huff_codes_len,
        unsigned short * huffman_code_book) {
    if (tree[idx].left == 255){
        unsigned char leaf_idx = cumulative_idx[depth - 1];
        unsigned char diff_len = tree[idx].diff_len;
        ljpg_huff_leaf_def[leaf_idx] = diff_len;
        huff_codes[diff_len] =  huffman_code_book[leaf_idx];
        huff_codes_len[diff_len] =  depth;
        cumulative_idx[depth - 1]++;
        return;
    }
    depth++;
    leaf_match_huff(tree, tree[idx].left, depth, cumulative_idx, ljpg_huff_leaf_def,
        huff_codes, huff_codes_len, huffman_code_book);
    leaf_match_huff(tree, tree[idx].right, depth, cumulative_idx, ljpg_huff_leaf_def,
        huff_codes, huff_codes_len, huffman_code_book);
}

void update_huffman_tree(unsigned char bitdepth, unsigned char * ljpg_huff_def,
        unsigned int * hist, unsigned short * huff_codes, unsigned char * huff_codes_len) {
    HUFF_NODE huff_tree[33];
    unsigned char i;

    for (i = 0; i <= bitdepth; i++) {
        huff_tree[bitdepth-i].count = hist[bitdepth-i];
        huff_tree[bitdepth-i].diff_len = bitdepth-i;
    }
    qsort(huff_tree, bitdepth + 1, sizeof(HUFF_NODE), huff_node_cmp);
    for (i = 0; i <= bitdepth; i++) {
        huff_tree[bitdepth-i].larger = bitdepth+1-i;
        huff_tree[bitdepth-i].left = 255;
        huff_tree[bitdepth-i].right = 255;
    }
    huff_tree[bitdepth].larger = 255;

    // Make tree
    unsigned char steps = bitdepth;
    unsigned char least, second_least, node_insert, next_larger_idx;
    least = 0;
    second_least = 1;
    node_insert = bitdepth + 1;
    while (steps > 0) {
        unsigned int count = huff_tree[least].count + huff_tree[second_least].count;
        huff_tree[node_insert].count = count;
        huff_tree[node_insert].diff_len = 255;
        huff_tree[node_insert].left = second_least;
        huff_tree[node_insert].right = least;
        huff_tree[node_insert].larger = 255;
        if (steps == 0) break;
        next_larger_idx = huff_tree[second_least].larger;
        //When the larger one has 255 as pointer, the merger has to be larger
        if (next_larger_idx == 255) {
            huff_tree[second_least].larger = node_insert;
        } else {
            unsigned char cur_least = second_least;
            while (1) {
                if (huff_tree[next_larger_idx].count > count){
                    huff_tree[node_insert].larger = next_larger_idx;
                    huff_tree[cur_least].larger = node_insert;
                    break;
                } else {
                    cur_least = next_larger_idx;
                    next_larger_idx = huff_tree[cur_least].larger;
                    if (next_larger_idx == 255){
                        huff_tree[cur_least].larger = node_insert;
                        break;
                    }
                }
            }
        }
        least = huff_tree[second_least].larger;
        second_least = huff_tree[least].larger;
        node_insert++;
        steps--;
    }
    memset(ljpg_huff_def, 0, 17 + bitdepth);

    // Transform the tree to right skewed required by JPEG standard
    depth_scan(huff_tree, bitdepth << 1, 0, ljpg_huff_def);
    unsigned char cumulative_index[17];
    unsigned short huffman_code_book[17];
    huff_code_gen(ljpg_huff_def, huffman_code_book, cumulative_index);

    // Now match the code with the diff_len leafs
    leaf_match_huff(huff_tree, bitdepth << 1, 0, cumulative_index, ljpg_huff_def + 16,
        huff_codes, huff_codes_len, huffman_code_book);
}
