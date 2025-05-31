#ifndef TLD_TRACKER_H
#define TLD_TRACKER_H

#include "common.h"
#include "object_detector.h"
#include "object_model.h"
#include "opt_flow_tracker.h"
#include "integrator.h"

// Example struct for TldStatus
typedef struct {
    const char* message;
    int training;
    int processing;
    int valid_object;
    int tracker_relocation;
    int detector_candidates_cnt;
    int detector_clusters_cnt;
} TldStatus;

// Example: dynamic array for Candidate
typedef struct {
    Candidate* data;
    int count;
    int capacity;
} CandidateArray;

typedef struct {
    Settings _settings;
    int _processing_en;
    Candidate _prediction;
    int _training_en;
    int _tracker_relocate;
    CandidateArray _detector_proposals;
    Candidate _tracker_proposal;
    ObjectDetector _detector;
    ObjectModel _model;
    OptFlowTracker _tracker;
    Integrator _integrator;
    Image _src_frame;
    Image _lf_frame;
} TldTracker;

void tld_tracker_init(TldTracker* tracker, Settings settings);
void tld_tracker_free(TldTracker* tracker);
Candidate tld_tracker_process_frame(TldTracker* tracker, const Image* input_frame);
void tld_tracker_start_tracking(TldTracker* tracker, Rect target);
void tld_tracker_stop_tracking(TldTracker* tracker);
TldStatus tld_tracker_get_status(const TldTracker* tracker);
CandidateArray tld_tracker_get_detector_proposals(const TldTracker* tracker);
CandidateArray tld_tracker_get_clusters(const TldTracker* tracker);
Candidate tld_tracker_get_tracker_proposal(const TldTracker* tracker);
Candidate tld_tracker_get_current_prediction(const TldTracker* tracker);

#endif

