#include "fern_feature_extractor.h"
#include <stddef.h>

void fern_feature_extractor_init(FernFeatureExtractor* extractor, ScanningGrid* grid) {
    extractor->grid = grid;
}
BinaryDescriptor fern_feature_extractor_get_descriptor_by_position(
    FernFeatureExtractor* extractor,
    Image* frame,
    Size position,
    size_t scale_id
) {
    BinaryDescriptor desc = 0x0;
    unsigned char* frame_data = frame->data;

    // Assume maximum number of pairs (based on BINARY_DESCRIPTOR_WIDTH)
    PixelIdPair pairs[BINARY_DESCRIPTOR_WIDTH];
    size_t n_pairs = scanning_grid_get_pixel_pairs(
        extractor->grid, frame, position, scale_id, pairs, BINARY_DESCRIPTOR_WIDTH
    );

    size_t mask = 0x1;
    for (size_t i = 0; i < n_pairs; ++i) {
        size_t p1 = pairs[i].p1;
        size_t p2 = pairs[i].p2;
        if (frame_data[p1] > frame_data[p2]) {
            desc |= mask;
        }
        mask <<= 1;
    }
    return desc;
}
BinaryDescriptor fern_feature_extractor_get_descriptor_by_bbox(
    FernFeatureExtractor* extractor,
    Image* frame,
    Rect bbox
) {
    BinaryDescriptor desc = 0x0;
    unsigned char* frame_data = frame->data;

    PixelIdPair pairs[BINARY_DESCRIPTOR_WIDTH];
    size_t n_pairs = scanning_grid_get_pixel_pairs_bbox(
        extractor->grid, frame, bbox, pairs, BINARY_DESCRIPTOR_WIDTH
    );

    size_t mask = 0x1;
    for (size_t i = 0; i < n_pairs; ++i) {
        size_t p1 = pairs[i].p1;
        size_t p2 = pairs[i].p2;
        if (frame_data[p1] > frame_data[p2]) {
            desc |= mask;
        }
        mask <<= 1;
    }
    return desc;
}
BinaryDescriptor fern_feature_extractor_get_descriptor(FernFeatureExtractor* extractor, Image* frame) {
    BinaryDescriptor desc = 0x0;
    unsigned char* frame_data = frame->data;

    PixelIdPair pairs[BINARY_DESCRIPTOR_WIDTH];
    size_t n_pairs = scanning_grid_get_pixel_pairs(
        extractor->grid, frame, pairs, BINARY_DESCRIPTOR_WIDTH
    );

    size_t mask = 0x1;
    for (size_t i = 0; i < n_pairs; ++i) {
        size_t p1 = pairs[i].p1;
        size_t p2 = pairs[i].p2;
        if (frame_data[p1] > frame_data[p2])
            desc |= mask;
        mask <<= 1;
    }
    return desc;
}
BinaryDescriptor fern_feature_extractor_call(
    FernFeatureExtractor* extractor,
    Image* frame,
    Size position,
    size_t scale_id
) {
    return fern_feature_extractor_get_descriptor_by_position(extractor, frame, position, scale_id);
}


