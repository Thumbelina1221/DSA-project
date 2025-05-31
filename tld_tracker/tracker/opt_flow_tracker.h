#ifndef OPT_FLOW_TRACKER_H
#define OPT_FLOW_TRACKER_H

#include "tld_utils.h"
#include <stddef.h>

#define MAX_POINTS 1024

typedef struct {
    double x, y;
} Point2d;

typedef struct {
    double width, height;
} Size2d;

typedef struct {
    double x, y, width, height;
} Rect2d;

// Replace Image with your image/matrix struct (for cv::Mat)
typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
    // add fields as needed
} Image;

typedef struct {
    Rect2d target;
    Point2d center;
    Size2d size;
    Image current_frame;
    Image prev_frame;
    Point2d prev_points[MAX_POINTS];
    size_t prev_points_count;
    Point2d cur_points[MAX_POINTS];
    size_t cur_points_count;
    Point2d backtrace[MAX_POINTS];
    size_t backtrace_count;
    Point2d prev_out_points[MAX_POINTS];
    size_t prev_out_points_count;
    Point2d cur_out_points[MAX_POINTS];
    size_t cur_out_points_count;
    unsigned char cur_status[MAX_POINTS];
    size_t cur_status_count;
    unsigned char backtrace_status[MAX_POINTS];
    size_t backtrace_status_count;
} OptFlowTracker;

// Constructors and API
void opt_flow_tracker_init(OptFlowTracker* tracker);
void opt_flow_tracker_set_frame(OptFlowTracker* tracker, const Image* frame);
void opt_flow_tracker_set_target(OptFlowTracker* tracker, Rect2d strobe);
Candidate opt_flow_tracker_track(OptFlowTracker* tracker);

#endif

