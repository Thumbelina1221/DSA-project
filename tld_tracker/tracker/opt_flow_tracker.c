#include "opt_flow_tracker.h"
#include <string.h>

void opt_flow_tracker_init(OptFlowTracker* tracker) {
    memset(tracker, 0, sizeof(OptFlowTracker));
}

void opt_flow_tracker_set_frame(OptFlowTracker* tracker, const Image* frame) {
    tracker->prev_frame = tracker->current_frame;
    tracker->current_frame = *frame;
}

void opt_flow_tracker_set_target(OptFlowTracker* tracker, Rect2d strobe) {
    tracker->target = strobe;
    tracker->center.x = strobe.x + 0.5 * strobe.width;
    tracker->center.y = strobe.y + 0.5 * strobe.height;
    tracker->size.width = strobe.width;
    tracker->size.height = strobe.height;
}
#include "opt_flow_tracker.h"
#include <math.h>

Candidate opt_flow_tracker_track(OptFlowTracker* tracker) {
    Candidate out;
    out.src = PROPOSAL_SOURCE_TRACKER; // Use your enum value
    out.valid = 0;

    if ((fabs(rect2d_area(tracker->target)) < 1e-10) ||
        image_empty(&tracker->prev_frame) ||
        image_empty(&tracker->current_frame))
        return out;

    Image mask;
    image_create(&mask, tracker->prev_frame.height, tracker->prev_frame.width, IMAGE_TYPE_U8C1); // Or your image alloc
    image_set_zero(&mask);

    Rect2d target_inside_frame = adjust_rect_to_frame(tracker->target,
                                (Size2d){mask.width, mask.height});

    Image target_subframe = image_subframe(&mask, target_inside_frame);
    image_set_value(&target_subframe, 255);

    Point2f prev_points[100];
    size_t prev_points_count = 0;
    good_features_to_track(&tracker->prev_frame, prev_points, &prev_points_count, 100, 0.01, 10.0, &mask, 7, 0, 0.04);

    if (prev_points_count > 0) {
        corner_sub_pix(&tracker->prev_frame, prev_points, prev_points_count,
                       (Size2d){3,3}, (Size2d){-1,-1}, 100, 0.01);
    }

    Point2f cur_points[100];
    unsigned char cur_status[100];
    Point2f backtrace[100];
    unsigned char backtrace_status[100];
    float err[100];

    size_t cur_points_count = 0, backtrace_count = 0;

    if (prev_points_count > 0) {
        calc_optical_flow_pyr_lk(&tracker->prev_frame, &tracker->current_frame,
                                 prev_points, prev_points_count, cur_points, cur_status, err,
                                 (Size2d){5,5}, 3, 100, 0.01, 0, 0.001);
        calc_optical_flow_pyr_lk(&tracker->current_frame, &tracker->prev_frame,
                                 cur_points, prev_points_count, backtrace, backtrace_status, err,
                                 (Size2d){5,5}, 3, 100, 0.01, 0, 0.001);

        tracker->prev_out_points_count = 0;
        tracker->cur_out_points_count = 0;

        for (size_t i = 0; i < prev_points_count; i++) {
            if (cur_status[i] && backtrace_status[i]) {
                double dx = prev_points[i].x - backtrace[i].x;
                double dy = prev_points[i].y - backtrace[i].y;
                double dist = sqrt(dx*dx + dy*dy);
                if (dist <= 1.0) {
                    tracker->prev_out_points[tracker->prev_out_points_count++] = prev_points[i];
                    tracker->cur_out_points[tracker->cur_out_points_count++] = cur_points[i];
                }
            }
        }
    }

    Point2d mean_shift = {0,0};
    double mean_scale = 1.0;

    if (tracker->prev_out_points_count > 3) {
        mean_shift = get_mean_shift(tracker->prev_out_points, tracker->cur_out_points, tracker->prev_out_points_count);
        mean_scale = get_scale(tracker->prev_out_points, tracker->cur_out_points, tracker->prev_out_points_count);

        tracker->center.x += mean_shift.x;
        tracker->center.y += mean_shift.y;
        tracker->size.width *= mean_scale;
        tracker->size.height *= mean_scale;

        out.prob = (double)tracker->prev_out_points_count / (double)prev_points_count;
        out.valid = (tracker->prev_out_points_count >= 3) && (prev_points_count > 10);
        out.strobe.x = (int)round(tracker->center.x - 0.5 * tracker->size.width);
        out.strobe.y = (int)round(tracker->center.y - 0.5 * tracker->size.height);
        out.strobe.width = (int)round(tracker->size.width);
        out.strobe.height = (int)round(tracker->size.height);
    } else {
        out.valid = 0;
    }

    image_release(&mask);
    image_release(&target_subframe);

    return out;
}

