#include "object_model.h"
#include <math.h>

void object_model_init(ObjectModel* model) {
    model->patch_size.width = 15;
    model->patch_size.height = 15;
    model->scales[0] = 1.0;
    model->scales_count = 1;
    model->init_overlap = 0.5;
    model->overlap = 1.0;
    model->sample_max_depth = 100;
    model->positive_sample_count = 0;
    model->negative_sample_count = 0;
    model->frame = NULL;
}

void object_model_set_frame(ObjectModel* model, Image* frame) {
    model->frame = frame;
}
void object_model_set_target(ObjectModel* model, Rect target) {
    model->target = target;

    Image* frame = model->frame;

    TransformPars aug_pars;

    // Fill angles
    static const double angles[] = {-90, -75, -60, -45, -30, -15, 0, 15, 30, 45, 60, 75, 90};
    memcpy(aug_pars.angles, angles, sizeof(angles));
    aug_pars.angles_count = sizeof(angles)/sizeof(angles[0]);

    // Copy scales from model
    memcpy(aug_pars.scales, model->scales, sizeof(double) * model->scales_count);
    aug_pars.scales_count = model->scales_count;

    // Fill translations
    aug_pars.translation_x[0] = (int)(-0.5 * model->init_overlap * model->target.width);
    aug_pars.translation_x[1] = 0;
    aug_pars.translation_x[2] = (int)(0.5 * model->init_overlap * model->target.width);
    aug_pars.translation_x_count = 3;

    aug_pars.translation_y[0] = (int)(-0.5 * model->init_overlap * model->target.height);
    aug_pars.translation_y[1] = 0;
    aug_pars.translation_y[2] = (int)(0.5 * model->init_overlap * model->target.height);
    aug_pars.translation_y_count = 3;

    aug_pars.overlap = model->init_overlap;
    aug_pars.disp_threshold = 0.1;
    aug_pars.pos_sample_size_limit = 100;
    aug_pars.neg_sample_size_limit = 100;

    // Construct Augmentator
    Augmentator aug;
    Augmentator_init(&aug, frame, target, aug_pars);

    // Generate positive samples
    Augmentator_SetClass(&aug, OBJECT_CLASS_POSITIVE);
    for (int i = 0; i < aug._sample_count; ++i) {
        Image patch = object_model_make_patch(model, &aug._sample[i]);
        object_model_add_new_patch(model, &patch, model->positive_sample, &model->positive_sample_count, model->sample_max_depth);
    }

    // Generate negative samples
    Augmentator_SetClass(&aug, OBJECT_CLASS_NEGATIVE);
    for (int i = 0; i < aug._sample_count; ++i) {
        Image patch = object_model_make_patch(model, &aug._sample[i]);
        object_model_add_new_patch(model, &patch, model->negative_sample, &model->negative_sample_count, model->sample_max_depth);
    }
}
void object_model_train(ObjectModel* model, Candidate candidate) {
    Image* frame = model->frame;

    TransformPars aug_pars;

    // Set angles
    static const double angles[] = { -15, 0, 15 };
    memcpy(aug_pars.angles, angles, sizeof(angles));
    aug_pars.angles_count = sizeof(angles) / sizeof(angles[0]);

    // Copy scales from model
    memcpy(aug_pars.scales, model->scales, sizeof(double) * model->scales_count);
    aug_pars.scales_count = model->scales_count;

    // Set translation_x
    aug_pars.translation_x[0] = (int)(-0.5 * model->init_overlap * model->target.width);
    aug_pars.translation_x[1] = 0;
    aug_pars.translation_x[2] = (int)(0.5 * model->init_overlap * model->target.width);
    aug_pars.translation_x_count = 3;

    // Set translation_y
    aug_pars.translation_y[0] = (int)(-0.5 * model->init_overlap * model->target.height);
    aug_pars.translation_y[1] = 0;
    aug_pars.translation_y[2] = (int)(0.5 * model->init_overlap * model->target.height);
    aug_pars.translation_y_count = 3;

    aug_pars.neg_sample_size_limit = 24;
    aug_pars.overlap = model->overlap;
    aug_pars.disp_threshold = 0.25;
    aug_pars.pos_sample_size_limit = 100;

    Augmentator aug;
    Augmentator_init(&aug, frame, candidate.strobe, aug_pars);

    // Positive samples
    Augmentator_SetClass(&aug, OBJECT_CLASS_POSITIVE);
    for (int i = 0; i < aug._sample_count; ++i) {
        Image patch = object_model_make_patch(model, &aug._sample[i]);
        double prob = object_model_predict(model, &patch);
        if (prob < 0.9)
            object_model_add_new_patch(model, &patch, model->positive_sample, &model->positive_sample_count, model->sample_max_depth);
    }

    // Negative samples
    Augmentator_SetClass(&aug, OBJECT_CLASS_NEGATIVE);
    for (int i = 0; i < aug._sample_count; ++i) {
        Image patch = object_model_make_patch(model, &aug._sample[i]);
        double prob = object_model_predict(model, &patch);
        if (prob > 0.1)
            object_model_add_new_patch(model, &patch, model->negative_sample, &model->negative_sample_count, model->sample_max_depth);
    }
}
double object_model_predict_candidate(const ObjectModel* model, Candidate candidate) {
    Image* src_frame = model->frame;
    // Adjust rectangle to fit inside frame
    candidate.strobe = adjust_rect_to_frame(candidate.strobe, image_size(src_frame));
    // Extract subframe
    Image subframe = image_subframe_clone(src_frame, candidate.strobe);
    // Make patch
    Image patch = object_model_make_patch(model, &subframe);

    double result = 0.0;
    if (!image_empty(&patch))
        result = object_model_predict_patch(model, &patch);

    image_release(&patch);
    image_release(&subframe);
    return result;
}
double object_model_predict_subframe(const ObjectModel* model, const Image* subframe) {
    Image patch = object_model_make_patch(model, subframe);
    double result = 0.0;
    if (!image_empty(&patch))
        result = object_model_predict_patch(model, &patch);
    image_release(&patch);
    return result;
}
void object_model_add_new_patch(ObjectModel* model, Image* patch, Image* sample, int* sample_count) {
    if (model->sample_max_depth) {
        if (*sample_count < model->sample_max_depth) {
            sample[*sample_count] = *patch;
            (*sample_count)++;
        } else {
            int idx_2_replace = get_random_int(*sample_count - 1);
            image_release(&sample[idx_2_replace]);
            sample[idx_2_replace] = *patch;
        }
    }
}
Image object_model_make_patch(const ObjectModel* model, const Image* subframe) {
    Image patch;
    if (!image_empty(subframe)) {
        resize_image(subframe, &patch, model->patch_size);
        return patch;
    } else {
        patch.data = NULL;
        patch.width = 0;
        patch.height = 0;
        // Set other members to 0/null if needed
        return patch;
    }
}

double object_model_similarity_coeff(const Image* patch_0, const Image* patch_1) {
    double ncc = images_correlation(patch_0, patch_1);
    double res = 0.5 * (ncc + 1.0);
    return res;
}

double object_model_sample_dissimilarity(const ObjectModel* model, const Image* patch, const Image* sample, int sample_count) {
    double out = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        double sim = object_model_similarity_coeff(&sample[i], patch);
        if (sim > out) out = sim;
    }
    return 1.0 - out;
}

double object_model_predict(const ObjectModel* model, const Image* patch) {
    double out = 0.0;
    double Npm = object_model_sample_dissimilarity(model, patch, model->negative_sample, model->negative_sample_count);
    double Ppm = object_model_sample_dissimilarity(model, patch, model->positive_sample, model->positive_sample_count);
    if (fabs(Npm + Ppm) > 1e-9)
        out = Npm / (Npm + Ppm);
    return out;
}

// Return positive sample array, up to user to manage allocation
void object_model_get_positive_sample(const ObjectModel* model, Image* out_array, int* out_count) {
    for (int i = 0; i < model->positive_sample_count; ++i)
        out_array[i] = model->positive_sample[i];
    *out_count = model->positive_sample_count;
}

// Return negative sample array, up to user to manage allocation
void object_model_get_negative_sample(const ObjectModel* model, Image* out_array, int* out_count) {
    for (int i = 0; i < model->negative_sample_count; ++i)
        out_array[i] = model->negative_sample[i];
    *out_count = model->negative_sample_count;
}

