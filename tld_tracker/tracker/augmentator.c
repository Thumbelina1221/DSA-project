#include "augmentator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assumes TransformPars/ObjectClass are defined in "common.h"

void augmentator_make_positive_sample(Augmentator* aug);
void augmentator_make_negative_sample(Augmentator* aug);
double augmentator_update_target_stddev(Augmentator* aug);
Image* augmentator_sample_begin(Augmentator* aug);
Image* augmentator_sample_end(Augmentator* aug);

void Augmentator_init(Augmentator* aug, const Image* frame, Rect target, TransformPars pars) {
    aug->_frame = *frame;
    aug->_target = target;
    aug->_pars = pars;
    aug->_sample = NULL;
    aug->_sample_count = 0;
    aug->_target_stddev = 0.0;

    aug->SetClass = Augmentator_SetClass;
    aug->make_positive_sample = augmentator_make_positive_sample;
    aug->make_negative_sample = augmentator_make_negative_sample;
    aug->update_target_stddev = augmentator_update_target_stddev;
    aug->begin = augmentator_sample_begin;
    aug->end = augmentator_sample_end;
}

Augmentator* Augmentator_SetClass(Augmentator* aug, ObjectClass name) {
    if (name == OBJECT_CLASS_POSITIVE) {
        augmentator_make_positive_sample(aug);
    } else if (name == OBJECT_CLASS_NEGATIVE) {
        augmentator_make_negative_sample(aug);
    } else {
        fprintf(stderr, "Unexpected object class in Augmentator\n");
        exit(1);
    }
    return aug;
}

void augmentator_make_positive_sample(Augmentator* aug) {
    for (int i = 0; i < aug->_sample_count; ++i) {
        image_free(&aug->_sample[i]);
    }
    free(aug->_sample);
    aug->_sample = NULL;
    aug->_sample_count = 0;

    int samples_count = 0;
    int stop = 0;
    for (int i = 0; i < aug->_pars.angles_count && !stop; ++i) {
        double angle = aug->_pars.angles[i];
        for (int j = 0; j < aug->_pars.scales_count && !stop; ++j) {
            double scale = aug->_pars.scales[j];
            for (int kx = 0; kx < aug->_pars.translation_x_count && !stop; ++kx) {
                double transl_x = aug->_pars.translation_x[kx];
                for (int ky = 0; ky < aug->_pars.translation_y_count && !stop; ++ky) {
                    double transl_y = aug->_pars.translation_y[ky];
                    // Create the transformed sample
                    Image transformed;
                    subframe_linear_transform(&aug->_frame, aug->_target, angle, scale, transl_x, transl_y, &transformed);
                    aug->_sample = realloc(aug->_sample, sizeof(Image) * (aug->_sample_count + 1));
                    aug->_sample[aug->_sample_count++] = transformed;
                    samples_count++;
                    if ((samples_count >= aug->_pars.pos_sample_size_limit) && (aug->_pars.pos_sample_size_limit > 0)) {
                        stop = 1;
                    }
                }
                if (stop) break;
            }
            if (stop) break;
        }
        if (stop) break;
    }
}

void augmentator_make_negative_sample(Augmentator* aug) {
    for (int i = 0; i < aug->_sample_count; ++i) {
        image_free(&aug->_sample[i]);
    }
    free(aug->_sample);
    aug->_sample = NULL;
    aug->_sample_count = 0;

    double target_stddev = augmentator_update_target_stddev(aug);

    Size* steps = malloc(sizeof(Size) * aug->_pars.scales_count);
    for (int i = 0; i < aug->_pars.scales_count; ++i) {
        int scaled_w = (int)(aug->_target.width * aug->_pars.scales[i]);
        int scaled_h = (int)(aug->_target.height * aug->_pars.scales[i]);
        int step_x = (int)(scaled_w * aug->_pars.overlap);
        int step_y = (int)(scaled_h * aug->_pars.overlap);
        if (step_x < 4) step_x = 4;
        if (step_y < 4) step_y = 4;
        steps[i].width = step_x;
        steps[i].height = step_y;
    }

    Size* scan_positions = malloc(sizeof(Size) * aug->_pars.scales_count);
    get_scan_position_cnt(
        image_size(&aug->_frame),
        (Size){aug->_target.width, aug->_target.height},
        aug->_pars.scales,
        steps,
        aug->_pars.scales_count,
        scan_positions
    );

    int samples_count = 0;
    int stop = 0;
    for (int scale_id = 0; scale_id < aug->_pars.scales_count && !stop; ++scale_id) {
        Rect current_rect;
        current_rect.x = 0;
        current_rect.y = 0;
        current_rect.width = (int)(aug->_target.width * aug->_pars.scales[scale_id]);
        current_rect.height = (int)(aug->_target.height * aug->_pars.scales[scale_id]);

        int step_x = steps[scale_id].width;
        int step_y = steps[scale_id].height;
        Size positions = scan_positions[scale_id];

        for (int x_org = 0; x_org < positions.width && !stop; ++x_org) {
            for (int y_org = 0; y_org < positions.height && !stop; ++y_org) {
                current_rect.x = x_org * step_x;
                current_rect.y = y_org * step_y;
                double iou = compute_iou(current_rect, aug->_target);
                double stddev = get_frame_std_dev(&aug->_frame, current_rect);
                if ((iou < 0.1) && (stddev > target_stddev * aug->_pars.disp_threshold)) {
                    Image neg = image_subframe_clone(&aug->_frame, current_rect);
                    aug->_sample = realloc(aug->_sample, sizeof(Image) * (aug->_sample_count + 1));
                    aug->_sample[aug->_sample_count++] = neg;
                    samples_count++;
                }
                if ((samples_count >= aug->_pars.neg_sample_size_limit) && (aug->_pars.neg_sample_size_limit > 0)) {
                    stop = 1;
                }
            }
        }
    }
    free(steps);
    free(scan_positions);
}

double augmentator_update_target_stddev(Augmentator* aug) {
    int target_outside_frame = strobe_is_outside(aug->_target, image_size(&aug->_frame));
    if (target_outside_frame)
        return aug->_target_stddev;
    aug->_target_stddev = get_frame_std_dev(&aug->_frame, aug->_target);
    return aug->_target_stddev;
}

Image* augmentator_sample_begin(Augmentator* aug) {
    return aug->_sample;
}

Image* augmentator_sample_end(Augmentator* aug) {
    return aug->_sample + aug->_sample_count;
}

