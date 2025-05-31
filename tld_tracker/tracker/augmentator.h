#ifndef AUGMENTATOR_H
#define AUGMENTATOR_H

#include "common.h"
#include "tld_utils.h"

// ---- ENUMS AND STRUCTS ----

typedef enum {
    OBJECT_CLASS_POSITIVE,
    OBJECT_CLASS_NEGATIVE
} ObjectClass;

typedef struct {
    double* scales;
    int scales_count;
    double* angles;
    int angles_count;
    double* translation_x;
    int translation_x_count;
    double* translation_y;
    int translation_y_count;
    double overlap;
    double disp_threshold;
    int pos_sample_size_limit;
    int neg_sample_size_limit;
} TransformPars;

typedef struct Augmentator {
    Image _frame;
    Rect _target;
    TransformPars _pars;
    double _target_stddev;
    Image* _sample;
    int _sample_count;
} Augmentator;

// ---- FUNCTION PROTOTYPES ----

void Augmentator_init(Augmentator* aug, const Image* frame, Rect target, TransformPars pars);
Augmentator* Augmentator_SetClass(Augmentator* aug, ObjectClass name);
void augmentator_make_positive_sample(Augmentator* aug);
void augmentator_make_negative_sample(Augmentator* aug);
double augmentator_update_target_stddev(Augmentator* aug);
Image* augmentator_sample_begin(Augmentator* aug);
Image* augmentator_sample_end(Augmentator* aug);
void augmentator_free(Augmentator* aug);

#endif

