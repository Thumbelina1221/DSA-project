#ifndef OBJECT_MODEL_H
#define OBJECT_MODEL_H

#include "tld_utils.h"
#include "augmentator.h"

#define MAX_SCALES 16
#define MAX_SAMPLE_DEPTH 128

typedef struct {
    Size patch_size;
    Image* frame;
    Rect target;
    double scales[MAX_SCALES];
    size_t scales_count;
    double init_overlap;
    double overlap;
    size_t sample_max_depth;
    Image positive_sample[MAX_SAMPLE_DEPTH];
    size_t positive_sample_count;
    Image negative_sample[MAX_SAMPLE_DEPTH];
    size_t negative_sample_count;
} ObjectModel;

void object_model_init(ObjectModel* model);
void object_model_set_frame(ObjectModel* model, Image* frame);
void object_model_set_target(ObjectModel* model, Rect target);
void object_model_train(ObjectModel* model, Candidate candidate);
double object_model_predict_candidate(const ObjectModel* model, Candidate candidate);
double object_model_predict_subframe(const ObjectModel* model, const Image* subframe);
size_t object_model_get_positive_sample(const ObjectModel* model, Image* out_samples, size_t max_count);
size_t object_model_get_negative_sample(const ObjectModel* model, Image* out_samples, size_t max_count);

// Internal equivalents
void object_model_make_patch(const ObjectModel* model, const Image* subframe, Image* out_patch);
void object_model_add_new_patch(ObjectModel* model, Image* patch, Image* sample_array, size_t* sample_count);
double object_model_similarity_coeff(const ObjectModel* model, const Image* subframe_0, const Image* subframe_1);
double object_model_sample_dissimilarity(const ObjectModel* model, Image* patch, const Image* sample_array, size_t sample_count);
double object_model_predict_patch(const ObjectModel* model, const Image* patch);

#endif

