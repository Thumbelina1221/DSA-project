#include <tracker/scanning_grid.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define AREA_ERROR         "ScanningGrid has received zero area base bbox!"
#define OVERLAP_ERROR      "ScanningGrid has received zero or negative overlap!"
#define SCALE_ERROR        "ScanningGrid has received zero or negative base scale!"
#define LARGE_SCALE_ERROR  "ScanningGrid scale larger than image!"
void scanning_grid_init(ScanningGrid* grid, Size frame_size) {
    grid->frame_size = frame_size;
    fern_init(&grid->fern, BINARY_DESCRIPTOR_WIDTH); // BINARY_DESCRIPTOR_WIDTH is a macro or constant
    grid->base_bbox.width = 0;
    grid->base_bbox.height = 0;
    grid->scales = NULL;
    grid->scales_count = 0;
    grid->overlap = 0.0;
    grid->zero_shifted = NULL;
    grid->zero_shifted_count = 0;
}
void scanning_grid_copy(ScanningGrid* dest, const ScanningGrid* src) {
    dest->frame_size = src->frame_size;
    fern_copy(&dest->fern, &src->fern); // Assume fern_copy is defined

    dest->base_bbox = src->base_bbox;

    dest->scales_count = src->scales_count;
    if (src->scales_count > 0) {
        dest->scales = (double*)malloc(sizeof(double) * src->scales_count);
        memcpy(dest->scales, src->scales, sizeof(double) * src->scales_count);
    } else {
        dest->scales = NULL;
    }

    dest->overlap = src->overlap;

    dest->zero_shifted_count = src->zero_shifted_count;
    if (src->zero_shifted_count > 0) {
        dest->zero_shifted = (PixelIdPair**)malloc(sizeof(PixelIdPair*) * src->zero_shifted_count);
        for (size_t i = 0; i < src->zero_shifted_count; ++i) {
            // You need to know the length of each row in zero_shifted (e.g. zero_shifted_len[i])
            // Here we assume a function or field is available for that length, or all rows are same size
            size_t row_len = /* get row length for i */;
            dest->zero_shifted[i] = (PixelIdPair*)malloc(sizeof(PixelIdPair) * row_len);
            memcpy(dest->zero_shifted[i], src->zero_shifted[i], sizeof(PixelIdPair) * row_len);
        }
    } else {
        dest->zero_shifted = NULL;
    }
}
void scanning_grid_set_base(ScanningGrid* grid, Size bbox, double overlap, const double* scales, size_t scales_count) {
    grid->base_bbox = bbox;
    grid->overlap = overlap;

    // Free old scales and arrays if present
    if (grid->scales) free(grid->scales);
    if (grid->steps) free(grid->steps);
    if (grid->bbox_sizes_w) free(grid->bbox_sizes_w);
    if (grid->bbox_sizes_h) free(grid->bbox_sizes_h);

    // Free previous zero_shifted arrays
    if (grid->zero_shifted) {
        for (size_t i = 0; i < grid->zero_shifted_scales_count; ++i)
            if (grid->zero_shifted[i]) free(grid->zero_shifted[i]);
        free(grid->zero_shifted);
    }
    if (grid->zero_shifted_counts) free(grid->zero_shifted_counts);

    grid->scales = (double*)malloc(sizeof(double) * scales_count);
    memcpy(grid->scales, scales, sizeof(double) * scales_count);
    grid->scales_count = scales_count;

    grid->steps = (Size*)malloc(sizeof(Size) * scales_count);
    grid->bbox_sizes_w = (size_t*)malloc(sizeof(size_t) * scales_count);
    grid->bbox_sizes_h = (size_t*)malloc(sizeof(size_t) * scales_count);
    grid->bbox_sizes_count = scales_count;

    grid->zero_shifted = (PixelIdPair**)malloc(sizeof(PixelIdPair*) * scales_count);
    grid->zero_shifted_counts = (size_t*)malloc(sizeof(size_t) * scales_count);
    grid->zero_shifted_scales_count = scales_count;

    // Area/bounds checks
    if (bbox.width * bbox.height <= 0) {
        fprintf(stderr, "%s\n", AREA_ERROR);
        exit(1);
    }
    if (overlap <= 0.0) {
        fprintf(stderr, "%s\n", OVERLAP_ERROR);
        exit(1);
    }

    for (size_t i = 0; i < scales_count; ++i) {
        double scale = scales[i];
        if (scale <= 0.0) {
            fprintf(stderr, "%s\n", SCALE_ERROR);
            exit(1);
        }

        Size scaled_bbox;
        scaled_bbox.width = (int)(bbox.width * scale);
        scaled_bbox.height = (int)(bbox.height * scale);

        grid->bbox_sizes_w[i] = scaled_bbox.width;
        grid->bbox_sizes_h[i] = scaled_bbox.height;

        if (scaled_bbox.width > grid->frame_size.width ||
            scaled_bbox.height > grid->frame_size.height) {
            fprintf(stderr, "%s\n", LARGE_SCALE_ERROR);
            exit(1);
        }

        // 1. Fern transform
        size_t fern_pair_count;
        AbsFernPair* fern_base = fern_transform(&grid->fern, scaled_bbox.width, scaled_bbox.height, &fern_pair_count);

        // 2. For each fern pair, compute PixelIdPair offsets
        grid->zero_shifted_counts[i] = fern_pair_count;
        grid->zero_shifted[i] = (PixelIdPair*)malloc(sizeof(PixelIdPair) * fern_pair_count);
        for (size_t j = 0; j < fern_pair_count; ++j) {
            size_t offset0 = (size_t)(fern_base[j].first.x + fern_base[j].first.y * grid->frame_size.width);
            size_t offset1 = (size_t)(fern_base[j].second.x + fern_base[j].second.y * grid->frame_size.width);
            grid->zero_shifted[i][j].p1 = offset0;
            grid->zero_shifted[i][j].p2 = offset1;
        }
        free(fern_base);

        // 3. Step sizes
        int step_x = (int)(scaled_bbox.width * overlap);
        int step_y = (int)(scaled_bbox.height * overlap);
        if (step_x < 4) step_x = 4;
        if (step_y < 4) step_y = 4;
        grid->steps[i].width = step_x;
        grid->steps[i].height = step_y;
    }
}
// You must implement get_scan_position_cnt_c similar to your get_scan_position_cnt C++ function.
void scanning_grid_get_positions_cnt(
    const ScanningGrid* grid, Size* out_sizes, size_t* out_count)
{
    get_scan_position_cnt_c(
        grid->frame_size,
        grid->base_bbox,
        grid->scales, grid->scales_count,
        grid->steps,
        out_sizes,
        out_count
    );
}
// Returns a pointer to a newly allocated array of PixelIdPair, with count in *out_count
PixelIdPair* scanning_grid_get_pixel_pairs(
    const ScanningGrid* grid,
    const Image* frame,
    Size position,
    size_t scale_idx,
    size_t* out_count)
{
    if (grid->zero_shifted_scales_count == 0)
        exit(1); // Or handle error

    // Copy the base pixel pairs for this scale
    size_t base_count = grid->zero_shifted_counts[scale_idx];
    PixelIdPair* out_pairs = (PixelIdPair*)malloc(sizeof(PixelIdPair) * base_count);

    // Calculate stride, step sizes, and offset
    double scale = grid->scales[scale_idx];
    Size scaled_bbox;
    scaled_bbox.width = (int)(grid->base_bbox.width * scale);
    scaled_bbox.height = (int)(grid->base_bbox.height * scale);

    int step_x = grid->steps[scale_idx].width;
    int step_y = grid->steps[scale_idx].height;
    int x_offset = position.width * step_x;
    int y_offset = position.height * step_y;

    // Assuming row stride (step[0]) in bytes, but with PixelIdPair, use frame->width (or as appropriate)
    int linear_offset = x_offset + y_offset * frame->width;

    for (size_t i = 0; i < base_count; ++i) {
        out_pairs[i].p1 = grid->zero_shifted[scale_idx][i].p1 + linear_offset;
        out_pairs[i].p2 = grid->zero_shifted[scale_idx][i].p2 + linear_offset;
    }
    *out_count = base_count;
    return out_pairs;
}
// Returns out_pairs with out_count (must be freed by caller)
PixelIdPair* scanning_grid_get_pixel_pairs_bbox(
    const ScanningGrid* grid,
    const Image* frame,
    Rect bbox,
    size_t* out_count
) {
    Size rect_size = {bbox.width, bbox.height};
    size_t n_fern;
    AbsFernPair* local_coords = fern_transform(&grid->fern, rect_size.width, rect_size.height, &n_fern);

    PixelIdPair* out_pairs = (PixelIdPair*)malloc(sizeof(PixelIdPair) * n_fern);

    for (size_t i = 0; i < n_fern; ++i) {
        // frame->stride should be the number of pixels per row (not bytes)
        size_t p1_offset = (local_coords[i].first.x + bbox.x) + (local_coords[i].first.y + bbox.y) * frame->stride;
        size_t p2_offset = (local_coords[i].second.x + bbox.x) + (local_coords[i].second.y + bbox.y) * frame->stride;
        out_pairs[i].p1 = p1_offset;
        out_pairs[i].p2 = p2_offset;
    }
    *out_count = n_fern;
    free(local_coords);
    return out_pairs;
}
PixelIdPair* scanning_grid_get_pixel_pairs_full(
    const ScanningGrid* grid,
    const Image* frame,
    size_t* out_count
) {
    Size rect_size = {frame->width, frame->height};
    size_t n_fern;
    AbsFernPair* local_coords = fern_transform(&grid->fern, rect_size.width, rect_size.height, &n_fern);

    PixelIdPair* out_pairs = (PixelIdPair*)malloc(sizeof(PixelIdPair) * n_fern);

    for (size_t i = 0; i < n_fern; ++i) {
        size_t p1_offset = local_coords[i].first.x + local_coords[i].first.y * frame->stride;
        size_t p2_offset = local_coords[i].second.x + local_coords[i].second.y * frame->stride;
        out_pairs[i].p1 = p1_offset;
        out_pairs[i].p2 = p2_offset;
    }
    *out_count = n_fern;
    free(local_coords);
    return out_pairs;
}

const Fern* scanning_grid_get_fern(const ScanningGrid* grid) {
    return &grid->fern;
}

Size scanning_grid_get_overlap(const ScanningGrid* grid) {
    Size out;
    out.width = (int)(grid->overlap * grid->base_bbox.width);
    out.height = (int)(grid->overlap * grid->base_bbox.height);
    return out;
}

Size scanning_grid_get_frame_size(const ScanningGrid* grid) {
    return grid->frame_size;
}

const double* scanning_grid_get_scales(const ScanningGrid* grid, size_t* out_count) {
    *out_count = grid->scales_count;
    return grid->scales;
}

const Size* scanning_grid_get_steps(const ScanningGrid* grid, size_t* out_count) {
    *out_count = grid->scales_count;
    return grid->steps;
}

const Size* scanning_grid_get_bbox_sizes(const ScanningGrid* grid, size_t* out_count) {
    *out_count = grid->scales_count;
    return grid->bbox_sizes;
}



