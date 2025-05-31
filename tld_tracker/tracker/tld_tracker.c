#include "tld_tracker.h"
#include <stdio.h>

void tld_tracker_print(FILE* out, const TldTracker* tracker) {
    TldStatus status = tld_tracker_get_status(tracker);
    Candidate pred = tld_tracker_get_current_prediction(tracker);

    fprintf(out, "\n");
    fprintf(out, "<TLD tracker object>\n");
    fprintf(out, "Processing:\t%s\n", status.processing ? "enable" : "disable");
    fprintf(out, "Target:\t%s\n", status.valid_object ? "valid" : "invalid");
    fprintf(out, "Probability:\t%.6f\n", pred.prob);
    fprintf(out, "Size (W/H):\t%d/%d\n", pred.strobe.width, pred.strobe.height);
    fprintf(out, "Center (X/Y):\t%d/%d\n", pred.strobe.x, pred.strobe.y);
    fprintf(out, "Training status:\t%s\n", status.training ? "enable" : "disable");
    fprintf(out, "Relocation flag:\t%s\n", status.tracker_relocation ? "enable" : "disable");
    fprintf(out, "Detector proposals:\t%d\n", status.detector_candidates_cnt);
    fprintf(out, "Detector clusters:\t%d\n", status.detector_clusters_cnt);
    fprintf(out, "Status:\t\t%s\n", status.message ? status.message : "");
    fprintf(out, "\n");
}

void default_settings(Settings* s);

TldTracker make_tld_tracker() {
    Settings s;
    default_settings(&s);
    return make_tld_tracker_with_settings(&s);
}

TldTracker make_tld_tracker_with_settings(const Settings* s) {
    TldTracker tracker;
    tld_tracker_init(&tracker, *s);
    return tracker;
}

// Constructor logic, sets fields. Add any extra zeroing or initialization needed.
void tld_tracker_init(TldTracker* tracker, Settings settings) {
    tracker->_settings = settings;
    object_model_init(&tracker->_model);
    integrator_init(&tracker->_integrator, &tracker->_model);
    tracker->_processing_en = 0;
    // Add zeroing/init for the rest as needed
}

Candidate tld_tracker_process_frame(TldTracker* tracker, const Image* input_frame) {
    // Clone source frame
    image_clone(input_frame, &tracker->_src_frame);

    // Blur for low-frequency frame
    blur_image(&tracker->_src_frame, &tracker->_lf_frame, 7); // 7x7 kernel

    // Set frames for detector/model/tracker
    object_detector_set_frame(&tracker->_detector, &tracker->_lf_frame);
    object_model_set_frame(&tracker->_model, &tracker->_src_frame);
    opt_flow_tracker_set_frame(&tracker->_tracker, &tracker->_src_frame);

    if (tracker->_processing_en) {
        // Detect
        CandidateArray detector_proposals = object_detector_detect(&tracker->_detector);
        candidate_array_free(&tracker->_detector_proposals);
        tracker->_detector_proposals = candidate_array_clone(&detector_proposals);

        // Track
        tracker->_tracker_proposal = opt_flow_tracker_track(&tracker->_tracker);

        // Integrate (returns IntegratorResult)
        IntegratorResult result = integrator_integrate(
            &tracker->_integrator,
            tracker->_detector_proposals.data, tracker->_detector_proposals.count,
            &tracker->_tracker_proposal
        );
        tracker->_prediction = result.candidate;
        tracker->_training_en = result.training_enable;
        tracker->_tracker_relocate = result.tracker_relocation_enable;

        // Training and relocation
        if (tracker->_training_en) {
            object_detector_train(&tracker->_detector, tracker->_prediction);
            object_detector_update_grid(&tracker->_detector, tracker->_prediction);
            object_model_train(&tracker->_model, tracker->_prediction);
        }
        if (tracker->_tracker_relocate)
            opt_flow_tracker_set_target(&tracker->_tracker, tracker->_prediction.strobe);
        else
            opt_flow_tracker_set_target(&tracker->_tracker, tracker->_tracker_proposal.strobe);
    }

    tracker->_prediction.src = PROPOSAL_SOURCE_FINAL;
    return tracker->_prediction;
}

Candidate tld_tracker_get_current_prediction(const TldTracker* tracker) {
    return tracker->_prediction;
}
Candidate tld_tracker_process_frame(TldTracker* tracker, const Image* input_frame);

void tld_tracker_start_tracking(TldTracker* tracker, Rect target) {
    object_detector_set_target(&tracker->_detector, target);
    opt_flow_tracker_set_target(&tracker->_tracker, target);
    object_model_set_target(&tracker->_model, target);
    tracker->_processing_en = 1;
}
void tld_tracker_stop_tracking(TldTracker* tracker) {
    tracker->_processing_en = 0;
}
void tld_tracker_update_settings(TldTracker* tracker) {
    // No-op
}
int tld_tracker_is_processing(const TldTracker* tracker) {
    return tracker->_processing_en;
}

void tld_tracker_get_proposals(const TldTracker* tracker, CandidateArray* detector_proposals, CandidateArray* clusters, Candidate* tracker_proposal) {
    // Copy detector proposals
    *detector_proposals = tracker->_detector_proposals;
    // Get clusters from integrator
    *clusters = integrator_get_clusters(&(tracker->_integrator));
    // Return tracker proposal
    *tracker_proposal = tracker->_tracker_proposal;
}
TldStatus tld_tracker_get_status(const TldTracker* tracker) {
    TldStatus out;
    out.message = integrator_get_status_message(&(tracker->_integrator));
    out.training = tracker->_training_en;
    out.processing = tracker->_processing_en;
    out.valid_object = tracker->_prediction.valid;
    out.tracker_relocation = tracker->_tracker_relocate;
    out.detector_candidates_cnt = tracker->_detector_proposals.count;
    out.detector_clusters_cnt = integrator_get_clusters(&(tracker->_integrator)).count;
    return out;
}
void tld_tracker_get_models_positive(const TldTracker* tracker, Image* out_array, int* out_count) {
    object_model_get_positive_sample(&(tracker->_model), out_array, out_count);
}
void tld_tracker_get_models_negative(const TldTracker* tracker, Image* out_array, int* out_count) {
    object_model_get_negative_sample(&(tracker->_model), out_array, out_count);
}


