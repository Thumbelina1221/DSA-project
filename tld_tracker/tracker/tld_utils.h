#ifndef TLD_UTILS_H
#define TLD_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Simple 2D size and point types
typedef struct { int width, height; } Size;
typedef struct { int x, y, width, height; } Rect;
typedef struct { float x, y; } Point2f;

// Proposal source enum (int)
#define PROPOSAL_SOURCE_TRACKER 0
#define PROPOSAL_SOURCE_DETECTOR 1
#define PROPOSAL_SOURCE_MIXED 2
#define PROPOSAL_SOURCE_FINAL 3

typedef struct {
    Rect strobe;
    double prob;
    double aux_prob;
    int valid;
    int src;
} Candidate;

// Raw grayscale image
typedef struct {
    int width, height;
    uint8_t* data;
} Image;

// Print a Rect
void print_rect(FILE* out, const Rect* rect);

// Candidate comparison
int candidate_less(const Candidate* a, const Candidate* b);

// Random utilities
double get_normalized_random();
int get_random_int(int maxint);

// Random image generation
Image generate_random_image();
Image generate_random_image_with_size(int width, int height);

// Image memory
void image_free(Image* img);

// Rect utilities
Rect get_extended_rect_for_rotation(Rect base_rect, double angle_degrees);
Rect adjust_rect_to_frame(Rect rect, Size sz);
int strobe_is_outside(Rect rect, Size sz);
int rect_area(Rect r);

// Subframe and transform
Image get_rotated_subframe(const Image* frame, Rect subframe_rect, double angle_deg);
void rotate_subframe(Image* frame, Rect subframe_rect, double angle_deg);
void subframe_linear_transform(const Image* frame, Rect in_strobe, double angle,
                              double scale, int offset_x, int offset_y, Image* out);

// Interpolation
uint8_t bilinear_interp_for_point(double x, double y, const Image* img);

// Angle conversions
double degree2rad(double angle);
double rad2degree(double angle);

// IOU, shifts, scale
double compute_iou(Rect a, Rect b);
Point2f get_mean_shift(const Point2f* start, const Point2f* stop, size_t count);
double get_scale(const Point2f* start, const Point2f* stop, size_t count);

// Draw rectangles
void draw_candidate(Image* img, Candidate c);
void draw_candidates(Image* img, Candidate* candidates, size_t num_candidates);

// Grid scan calculation
void get_scan_position_cnt(Size frame_size, Size box, const double* scales, const Size* steps, size_t scales_count, Size* out);

// Correlation
double images_correlation(const Image* image_1, const Image* image_2);

// Non-max suppression and clustering (return number of outputs)
int non_max_suppression(const Candidate* in, int in_count, double threshold_iou, Candidate* out);
int clusterize_candidates(const Candidate* in, int in_count, double threshold_iou, Candidate* out);
Candidate aggregate_candidates(const Candidate* sample, int sample_count);

// Std dev in ROI
double get_frame_std_dev(const Image* frame, Rect roi);

// ----------- ADD THESE MISSING HELPERS FOR AUGMENTATOR.C -----------

// Return the size of an image as a Size struct
static inline Size image_size(const Image* img) {
    Size sz;
    sz.width = img->width;
    sz.height = img->height;
    return sz;
}

// Clone a subframe (rectangular region) from an image, returned as new Image
Image image_subframe_clone(const Image* src, Rect roi);

#endif

