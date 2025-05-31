#ifndef SCANNING_GRID_H
#define SCANNING_GRID_H

#include "common.h"
#include "fern.h"
#include "tld_utils.h"

#define MAX_SCALES 16
#define MAX_POSITIONS 128
#define MAX_ZERO_SHIFTED 16
#define MAX_PIXEL_PAIRS 32

typedef struct {
    int width;
    int height;
} Size;

typedef struct {
    size_t p1;
    size_t p2;
} PixelIdPair;

typedef struct {
    Size frame_size;
    Fern fern;
    Size base_bbox;
    double scales[MAX_SCALES];
    size_t scales_count;
    Size steps[MAX_SCALES];
    Size bbox_sizes[MAX_SCALES];
    double overlap;
    PixelIdPair zero_shifted[MAX_ZERO_SHIFTED][MAX_PIXEL_PAIRS];
    size_t zero_shifted_count[MAX_ZERO_SHIFTED]; // count of pairs for each scale/shift
} ScanningGrid;

void scanning_grid_init(ScanningGrid* grid, Size frame_size);
void scanning_grid_copy(ScanningGrid* dst, const ScanningGrid* src);
void scanning_grid_set_base(ScanningGrid* grid, Size bbox, double overlap, const double* scales, size_t scales_count);

void scanning_grid_get_positions_cnt(const ScanningGrid* grid, Size* out_positions, size_t* out_count);

size_t scanning_grid_get_pixel_pairs(const ScanningGrid* grid, const Image* frame, Size position, size_t scale_idx, PixelIdPair* out_pairs, size_t max_pairs);
size_t scanning_grid_get_pixel_pairs_bbox(const ScanningGrid* grid, const Image* frame, Rect bbox, PixelIdPair* out_pairs, size_t max_pairs);
size_t scanning_grid_get_pixel_pairs_default(const ScanningGrid* grid, const Image* frame, PixelIdPair* out_pairs, size_t max_pairs);

size_t scanning_grid_get_scales(const ScanningGrid* grid, double* out_scales, size_t max_scales);
size_t scanning_grid_get_steps(const ScanningGrid* grid, Size* out_steps, size_t max_steps);
size_t scanning_grid_get_bbox_sizes(const ScanningGrid* grid, Size* out_bbox_sizes, size_t max_sizes);

const Fern* scanning_grid_get_fern(const ScanningGrid* grid);
Size scanning_grid_get_overlap(const ScanningGrid* grid);
Size scanning_grid_get_frame_size(const ScanningGrid* grid);

#endif // SCANNING_GRID_H

