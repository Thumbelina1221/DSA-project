#include "object_detector.h"
#include <stdlib.h>
#include <string.h>
#define MAX_DETECTION_CANDIDATES 1024
#define MAX_RETURN_CANDIDATES    16
// ObjectDetector struct definition (example)
typedef struct {
    Image* frame_ptr;
    Size frame_size;
    DetectorSettings settings;
    Rect designation;
    double designation_stddev;
    // ... other necessary fields ...
} ObjectDetector;

// SetFrame: Assign frame pointer and update size
void object_detector_set_frame(ObjectDetector* detector, Image* img) {
    detector->frame_ptr = img;
    detector->frame_size.width = img->width;
    detector->frame_size.height = img->height;
}

// Config: Store the detector settings
void object_detector_config(ObjectDetector* detector, DetectorSettings settings) {
    detector->settings = settings;
}

// SetTarget: Set designation, compute stddev, reset, then run training augmentator
void object_detector_set_target(ObjectDetector* detector, Rect strobe) {
    detector->designation = strobe;
    detector->designation_stddev = get_frame_std_dev(detector->frame_ptr, detector->designation);
    object_detector_reset(detector);

    TransformPars aug_pars;
    aug_pars.pos_sample_size_limit = -1;
    aug_pars.neg_sample_size_limit = -1;
    aug_pars.disp_threshold = detector->settings.stddev_relative_threshold;
    aug_pars.angles = detector->settings.init_training_rotation_angles;
    aug_pars.angles_count = detector->settings.init_training_rotation_angles_count;
    aug_pars.scales = detector->settings.init_training_scales;
    aug_pars.scales_count = detector->settings.init_training_scales_count;
    aug_pars.overlap = detector->settings.scanning_overlap;

    // Translation X (3 values)
    aug_pars.translation_x_count = 3;
    aug_pars.translation_x = malloc(sizeof(double) * 3);
    aug_pars.translation_x[0] = -0.5 * detector->settings.scanning_overlap * detector->designation.width;
    aug_pars.translation_x[1] = 0;
    aug_pars.translation_x[2] = 0.5 * detector->settings.scanning_overlap * detector->designation.width;

    // Translation Y (3 values)
    aug_pars.translation_y_count = 3;
    aug_pars.translation_y = malloc(sizeof(double) * 3);
    aug_pars.translation_y[0] = -0.5 * detector->settings.scanning_overlap * detector->designation.height;
    aug_pars.translation_y[1] = 0;
    aug_pars.translation_y[2] = 0.5 * detector->settings.scanning_overlap * detector->designation.height;

    Augmentator aug;
    Augmentator_init(&aug, detector->frame_ptr, detector->designation, aug_pars);

    object_detector_init_train(detector, &aug);

    // Clean up allocations
    free(aug_pars.translation_x);
    free(aug_pars.translation_y);
}
size_t object_detector_detect(
    ObjectDetector* detector,
    Candidate* out_candidates, // output buffer for top candidates
    size_t out_capacity        // should be at least MAX_RETURN_CANDIDATES
) {
    Candidate temp_candidates[MAX_DETECTION_CANDIDATES];
    size_t temp_count = 0;

    // Get positions_per_scale from the first scanning grid
    Size* positions_per_scale;
    size_t num_scales = 0;
    positions_per_scale = scanning_grid_get_positions_cnt(detector->scanning_grids[0], &num_scales);

    size_t scale_id = 0;
    for (size_t s = 0; s < num_scales; ++s) {
        Size positions = positions_per_scale[s];
        for (int y_i = 0; y_i < positions.height; ++y_i) {
            for (int x_i = 0; x_i < positions.width; ++x_i) {
                double ensemble_prob = 0.0;
                for (size_t i = 0; i < detector->classifiers_count; ++i) {
                    // Extract feature descriptor
                    BinaryDescriptor desc = fern_feature_extractor_call(
                        &detector->feat_extractors[i],
                        detector->frame_ptr,
                        (Size){x_i, y_i},
                        scale_id
                    );
                    // Predict
                    ensemble_prob += object_classifier_predict(&detector->classifiers[i], desc);
                }
                ensemble_prob /= (double)detector->classifiers_count;
                if (ensemble_prob > detector->settings.detection_probability_threshold) {
                    Candidate candidate;
                    candidate.src = PROPOSAL_SOURCE_DETECTOR;
                    candidate.prob = ensemble_prob;
                    candidate.strobe.x = x_i * scanning_grid_get_steps(detector->scanning_grids[0])[scale_id].width;
                    candidate.strobe.y = y_i * scanning_grid_get_steps(detector->scanning_grids[0])[scale_id].height;
                    candidate.strobe.width = scanning_grid_get_bbox_sizes(detector->scanning_grids[0])[scale_id].width;
                    candidate.strobe.height = scanning_grid_get_bbox_sizes(detector->scanning_grids[0])[scale_id].height;
                    if (temp_count < MAX_DETECTION_CANDIDATES) {
                        temp_candidates[temp_count++] = candidate;
                    }
                }
            }
        }
        scale_id++;
    }

    // Sort candidates by probability (descending)
    qsort(temp_candidates, temp_count, sizeof(Candidate), compare_candidate_prob_desc);

    // Output up to MAX_RETURN_CANDIDATES top candidates
    size_t n_ret = temp_count < MAX_RETURN_CANDIDATES ? temp_count : MAX_RETURN_CANDIDATES;
    for (size_t i = 0; i < n_ret; ++i) {
        out_candidates[i] = temp_candidates[i];
    }
    return n_ret;
}

// Helper sort function
int compare_candidate_prob_desc(const void* a, const void* b) {
    double pa = ((const Candidate*)a)->prob;
    double pb = ((const Candidate*)b)->prob;
    return (pb > pa) - (pb < pa);
}
void object_detector_train(ObjectDetector* detector, Candidate prediction) {
    TransformPars aug_pars;

    // Fill rotation and scale arrays (assuming the arrays and their counts are set elsewhere)
    memcpy(aug_pars.angles, detector->settings.training_rotation_angles, sizeof(double) * detector->settings.angles_count);
    aug_pars.angles_count = detector->settings.angles_count;
    memcpy(aug_pars.scales, detector->settings.training_scales, sizeof(double) * detector->settings.scales_count);
    aug_pars.scales_count = detector->settings.scales_count;

    // Translation X and Y: just a single 0
    aug_pars.translation_x[0] = 0.0;
    aug_pars.translation_x_count = 1;
    aug_pars.translation_y[0] = 0.0;
    aug_pars.translation_y_count = 1;

    aug_pars.overlap = detector->settings.scanning_overlap;
    aug_pars.disp_threshold = detector->settings.stddev_relative_threshold;
    aug_pars.pos_sample_size_limit = -1;
    aug_pars.neg_sample_size_limit = -1;

    Augmentator aug;
    Augmentator_init(&aug, detector->frame_ptr, prediction.strobe, aug_pars);

    object_detector_train_internal(detector, &aug); // _train(aug) → object_detector_train_internal
}
void object_detector_reset(ObjectDetector* detector) {
    // Clear arrays by resetting their counts to zero
    detector->feat_extractors_count = 0;
    detector->scanning_grids_count = 0;
    detector->classifiers_count = 0;

    for (int i = 0; i < CLASSIFIERS_CNT; i++) {
        // Allocate and initialize ScanningGrid
        ScanningGrid* grid = (ScanningGrid*)malloc(sizeof(ScanningGrid));
        scanning_grid_init(grid, detector->frame_size);

        // Set base for ScanningGrid
        scanning_grid_set_base(grid, 
                               (Size){detector->designation.width, detector->designation.height}, 
                               0.1, 
                               detector->settings.scanning_scales, 
                               detector->settings.scales_count);

        // Store pointer in scanning_grids array
        detector->scanning_grids[i] = grid;
        detector->scanning_grids_count++;

        // Create FernFeatureExtractor, associate with the grid
        FernFeatureExtractor* extractor = (FernFeatureExtractor*)malloc(sizeof(FernFeatureExtractor));
        fern_feature_extractor_init(extractor, grid);
        detector->feat_extractors[i] = extractor;
        detector->feat_extractors_count++;

        // Initialize ObjectClassifier
        ObjectClassifier* classifier = (ObjectClassifier*)malloc(sizeof(ObjectClassifier));
        object_classifier_init(classifier, BINARY_DESCRIPTOR_CNT);
        detector->classifiers[i] = classifier;
        detector->classifiers_count++;
    }
}
void object_detector_update_grid(ObjectDetector* detector, const Candidate* reference) {
    detector->designation = reference->strobe;
    for (int i = 0; i < detector->scanning_grids_count; ++i) {
        scanning_grid_set_base(
            detector->scanning_grids[i],
            (Size){ detector->designation.width, detector->designation.height },
            detector->settings.scanning_overlap,
            detector->settings.scanning_scales,
            detector->settings.scales_count
        );
    }
}

void object_detector_train(ObjectDetector* detector, Augmentator* aug) {
    // POSITIVE
    aug->SetClass(aug, OBJECT_CLASS_POSITIVE);
    for (int s = 0; s < aug->_sample_count; ++s) {
        Image* augm_subframe = &aug->_sample[s];
        double ensemble_prob = object_detector_ensemble_prediction(detector, augm_subframe);
        if (ensemble_prob >= detector->settings.training_pos_min_prob &&
            ensemble_prob <= detector->settings.training_pos_max_prob) {
            for (int i = 0; i < detector->feat_extractors_count; ++i) {
                BinaryDescriptor descriptor =
                    feat_extractor_get_descriptor(&detector->feat_extractors[i], augm_subframe);
                object_classifier_train_positive(&detector->classifiers[i], descriptor);
            }
        }
    }
    // NEGATIVE
    aug->SetClass(aug, OBJECT_CLASS_NEGATIVE);
    for (int s = 0; s < aug->_sample_count; ++s) {
        Image* augm_subframe = &aug->_sample[s];
        double ensemble_prob = object_detector_ensemble_prediction(detector, augm_subframe);
        if (ensemble_prob >= detector->settings.training_neg_min_prob &&
            ensemble_prob <= detector->settings.training_neg_max_prob) {
            for (int i = 0; i < detector->feat_extractors_count; ++i) {
                BinaryDescriptor descriptor =
                    feat_extractor_get_descriptor(&detector->feat_extractors[i], augm_subframe);
                object_classifier_train_negative(&detector->classifiers[i], descriptor);
            }
        }
    }
}
void object_detector_init_train(ObjectDetector* detector, Augmentator* aug) {
    // Positive samples
    aug->SetClass(aug, OBJECT_CLASS_POSITIVE);
    for (int s = 0; s < aug->_sample_count; ++s) {
        Image* augm_subframe = &aug->_sample[s];
        for (int i = 0; i < detector->feat_extractors_count; ++i) {
            BinaryDescriptor descriptor = feat_extractor_get_descriptor(&detector->feat_extractors[i], augm_subframe);
            object_classifier_train_positive(&detector->classifiers[i], descriptor);
        }
    }
    // Negative samples
    aug->SetClass(aug, OBJECT_CLASS_NEGATIVE);
    for (int s = 0; s < aug->_sample_count; ++s) {
        Image* augm_subframe = &aug->_sample[s];
        for (int i = 0; i < detector->feat_extractors_count; ++i) {
            BinaryDescriptor descriptor = feat_extractor_get_descriptor(&detector->feat_extractors[i], augm_subframe);
            size_t pos_max = object_classifier_get_max_positive(&detector->classifiers[i]);
            if (object_classifier_get_negative_distr(&detector->classifiers[i], descriptor) <
                (size_t)(pos_max * detector->settings.training_init_saturation)) {
                object_classifier_train_negative(&detector->classifiers[i], descriptor);
            }
        }
    }
}

double object_detector_ensemble_prediction(ObjectDetector* detector, Image* img) {
    double accum = 0.0;
    for (int i = 0; i < detector->feat_extractors_count; ++i) {
        BinaryDescriptor descriptor = feat_extractor_get_descriptor(&detector->feat_extractors[i], img);
        accum += object_classifier_predict(&detector->classifiers[i], descriptor);
    }
    if (detector->feat_extractors_count > 0)
        accum /= detector->feat_extractors_count;
    return accum;
}


