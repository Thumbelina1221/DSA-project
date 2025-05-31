#ifndef OBJECT_DETECTOR_H
#define OBJECT_DETECTOR_H

#include "common.h"
#include "tld_utils.h"
#include "fern_fext.h"
#include "object_classifier.h"
#include "augmentator.h"

#define MAX_SCANNING_GRIDS 16
#define MAX_FEAT_EXTRACTORS 16
#define MAX_CLASSIFIERS 16

typedef struct {
    Image* frame_ptr;
    Size frame_size;
    ScanningGrid* scanning_grids[MAX_SCANNING_GRIDS];
    int scanning_grids_count;

    FernFeatureExtractor feat_extractors[MAX_FEAT_EXTRACTORS];
    int feat_extractors_count;

    ObjectClassifier classifiers[MAX_CLASSIFIERS];
    int classifiers_count;

    Rect designation;
    double designation_stddev;
    DetectorSettings settings;
} ObjectDetector;

void object_detector_init(ObjectDetector* detector);
void object_detector_set_frame(ObjectDetector* detector, Image* img);
void object_detector_set_target(ObjectDetector* detector, Rect strobe);
void object_detector_update_grid(ObjectDetector* detector, const Candidate* reference);
void object_detector_train(ObjectDetector* detector, Candidate prediction);
size_t object_detector_detect(ObjectDetector* detector, Candidate* out_candidates, size_t max_candidates);
double object_detector_ensemble_prediction(ObjectDetector* detector, Image* img);
void object_detector_config(ObjectDetector* detector, DetectorSettings settings);

void object_detector_reset(ObjectDetector* detector);
void object_detector_train_internal(ObjectDetector* detector, Augmentator* aug);
void object_detector_init_train(ObjectDetector* detector, Augmentator* aug);

#endif

