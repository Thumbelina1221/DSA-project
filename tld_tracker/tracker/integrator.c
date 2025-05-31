#include "integrator.h"
#include <stdlib.h>
#include <string.h>

void integrator_init(Integrator* integrator, ObjectModel* model) {
    integrator->model = model;
}
void integrator_preprocess_candidates(Integrator* integrator) {
    // Clusterize detector proposals
    integrator->detector_proposal_clusters_count =
        clusterize_candidates(
            integrator->detector_raw_proposals, integrator->detector_raw_proposals_count,
            integrator->settings.clusterization_iou_threshold,
            integrator->detector_proposal_clusters
        );

    // Remove clusters invalidated by model
    size_t valid_count = 0;
    for (size_t i = 0; i < integrator->detector_proposal_clusters_count; ++i) {
        Candidate* item = &integrator->detector_proposal_clusters[i];
        item->aux_prob = object_model_predict(integrator->model, item);
        if (item->aux_prob >= integrator->settings.model_prob_threshold) {
            if (valid_count != i) {
                integrator->detector_proposal_clusters[valid_count] = *item;
            }
            ++valid_count;
        }
    }
    integrator->detector_proposal_clusters_count = valid_count;

    // Sort clusters by aux_prob descending
    qsort(
        integrator->detector_proposal_clusters,
        integrator->detector_proposal_clusters_count,
        sizeof(Candidate),
        compare_candidate_aux_prob_desc
    );

    // Select clusters with aux_prob > tracker_raw_proposal.aux_prob
    integrator->dc_more_confident_than_tracker_count = 0;
    for (size_t i = 0; i < integrator->detector_proposal_clusters_count; ++i) {
        if (integrator->detector_proposal_clusters[i].aux_prob > integrator->tracker_raw_proposal.aux_prob) {
            integrator->dc_more_confident_than_tracker[integrator->dc_more_confident_than_tracker_count++] =
                integrator->detector_proposal_clusters[i];
        }
    }

    // Select clusters close to tracker (IOU > 0.75)
    integrator->dc_close_to_tracker_count = 0;
    for (size_t i = 0; i < integrator->dc_more_confident_than_tracker_count; ++i) {
        if (compute_iou(
                integrator->dc_more_confident_than_tracker[i].strobe,
                integrator->tracker_raw_proposal.strobe) > 0.75) {
            integrator->dc_close_to_tracker[integrator->dc_close_to_tracker_count++] =
                integrator->dc_more_confident_than_tracker[i];
        }
    }

    // Update tracker proposal aux_prob
    integrator->tracker_raw_proposal.aux_prob = object_model_predict(
        integrator->model, &integrator->tracker_raw_proposal
    );
}

// Helper: Descending sort by aux_prob
int compare_candidate_aux_prob_desc(const void* a, const void* b) {
    double pa = ((const Candidate*)a)->aux_prob;
    double pb = ((const Candidate*)b)->aux_prob;
    return (pb > pa) - (pb < pa);
}
IntegratorResult integrator_get_integration_result(const Integrator* integrator) {
    IntegratorResult result;
    result.candidate = integrator->final_proposal;
    result.training_enable = integrator->training_enable;
    result.tracker_relocation_enable = integrator->tracker_relocation_enable;
    return result;
}
IntegratorResult integrator_integrate(
    Integrator* integrator,
    const Candidate* det_proposals, size_t det_proposals_count,
    const Candidate* tracker_proposal
) {
    // Copy detector proposals
    for (size_t i = 0; i < det_proposals_count; ++i)
        integrator->detector_raw_proposals[i] = det_proposals[i];
    integrator->detector_raw_proposals_count = det_proposals_count;

    // Copy tracker proposal
    integrator->tracker_raw_proposal = *tracker_proposal;

    // Preprocess
    integrator_preprocess_candidates(integrator);

    int tracker_result_is_stable = tracker_proposal->valid;
    int tracker_result_confident = (integrator->tracker_raw_proposal.aux_prob > integrator->settings.model_prob_threshold);

    if (tracker_result_is_stable && tracker_result_confident) {
        integrator_subtree_tracker_result_is_reliable(integrator);
    } else {
        integrator_subtree_tracker_result_is_not_reliable(integrator);
    }
    return integrator_get_integration_result(integrator);
}
void integrator_get_clusters(const Integrator* integrator, Candidate* out_clusters, size_t* out_count) {
    for (size_t i = 0; i < integrator->detector_proposal_clusters_count; ++i)
        out_clusters[i] = integrator->detector_proposal_clusters[i];
    *out_count = integrator->detector_proposal_clusters_count;
}
void integrator_set_settings(Integrator* integrator, IntegratorSettings settings) {
    integrator->settings = settings;
}
void integrator_subtree_detector_clusters_not_reliable(Integrator* integrator) {
    if (integrator->tracker_raw_proposal.aux_prob > 0.4) {
        integrator->final_proposal.strobe = integrator->tracker_raw_proposal.strobe;
        integrator->final_proposal.src = PROPOSAL_SOURCE_TRACKER;
        integrator->final_proposal.valid = true;
        integrator->final_proposal.prob = integrator->tracker_raw_proposal.aux_prob;
        integrator->training_enable = false;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "Only tracker's unreliable result", MAX_STATUS_MSG-1);
        if (integrator->tracker_raw_proposal.aux_prob > 0.7) {
            integrator->training_enable = true;
            integrator->tracker_relocation_enable = false;
            strncpy(integrator->status_message, "Only tracker's reliable result", MAX_STATUS_MSG-1);
        }
    } else {
        integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
        integrator->final_proposal.valid = false;
        integrator->final_proposal.prob = 0.0;
        integrator->training_enable = false;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "No reliable proposals", MAX_STATUS_MSG-1);
    }
}
void integrator_subtree_no_more_confident_det_clusters(Integrator* integrator) {
    integrator_subtree_detector_clusters_not_reliable(integrator);
}
void integrator_subtree_one_more_confident_det_cluster(Integrator* integrator) {
    double iou = compute_iou(
        integrator->dc_more_confident_than_tracker[0].strobe,
        integrator->tracker_raw_proposal.strobe
    );
    if (iou < 0.75) {
        integrator->final_proposal.strobe = integrator->dc_more_confident_than_tracker[0].strobe;
        integrator->final_proposal.src = PROPOSAL_SOURCE_DETECTOR;
        integrator->final_proposal.valid = true;
        integrator->final_proposal.prob = integrator->dc_more_confident_than_tracker[0].aux_prob;
        integrator->training_enable = false;
        integrator->tracker_relocation_enable = (integrator->dc_more_confident_than_tracker[0].aux_prob > 0.65);
        strncpy(integrator->status_message, "One detectors cluster better than tracker", MAX_STATUS_MSG-1);
    } else {
        // aggregate_candidates: must be implemented in C, here is an example:
        Candidate all_candidates[2];
        all_candidates[0] = integrator->dc_more_confident_than_tracker[0];
        all_candidates[1] = integrator->tracker_raw_proposal;
        integrator->final_proposal = aggregate_candidates(all_candidates, 2);
        integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
        integrator->final_proposal.valid = true;
        integrator->final_proposal.prob = integrator->final_proposal.aux_prob;
        integrator->training_enable = true;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "One Detector & Tracker are close", MAX_STATUS_MSG-1);
    }
}
void integrator_subtree_few_more_confident_det_clusters(Integrator* integrator) {
    if (integrator->dc_close_to_tracker_count == 0) {
        integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
        integrator->final_proposal.valid = false;
        integrator->final_proposal.prob = 0.0;
        integrator->training_enable = false;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "Few detectors clusters far from tracker", MAX_STATUS_MSG-1);
    } else {
        Candidate all_candidates[MAX_CANDIDATES];
        size_t count = 0;
        for (size_t i = 0; i < integrator->dc_close_to_tracker_count; ++i)
            all_candidates[count++] = integrator->dc_close_to_tracker[i];
        all_candidates[count++] = integrator->tracker_raw_proposal;
        integrator->final_proposal = aggregate_candidates(all_candidates, count);
        integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
        integrator->final_proposal.valid = true;
        integrator->final_proposal.prob = integrator->final_proposal.aux_prob;
        integrator->training_enable = true;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "Few Detector & Tracker are close", MAX_STATUS_MSG-1);
    }
}
void integrator_subtree_tracker_result_is_reliable(Integrator* integrator) {
    if (integrator->detector_proposal_clusters_count == 0) {
        integrator_subtree_detector_clusters_not_reliable(integrator);
    } else {
        if (integrator->dc_more_confident_than_tracker_count == 0) {
            integrator_subtree_no_more_confident_det_clusters(integrator);
        } else if (integrator->dc_more_confident_than_tracker_count == 1) {
            integrator_subtree_one_more_confident_det_cluster(integrator);
        } else {
            integrator_subtree_few_more_confident_det_clusters(integrator);
        }
    }
}
void integrator_subtree_tracker_result_is_not_reliable(Integrator* integrator) {
    if (integrator->detector_proposal_clusters_count == 0) {
        integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
        integrator->final_proposal.valid = false;
        integrator->final_proposal.prob = 0.0;
        integrator->training_enable = false;
        integrator->tracker_relocation_enable = false;
        strncpy(integrator->status_message, "No reliable results", MAX_STATUS_MSG-1);
    } else if (integrator->detector_proposal_clusters_count == 1) {
        int detector_stable =
            (integrator->detector_proposal_clusters[0].prob > 0.7) ||
            (integrator->detector_proposal_clusters[0].aux_prob > 0.8);
        integrator->final_proposal.strobe = integrator->detector_proposal_clusters[0].strobe;
        integrator->final_proposal.prob = integrator->detector_proposal_clusters[0].aux_prob;
        integrator->final_proposal.valid = true;
        integrator->final_proposal.src = PROPOSAL_SOURCE_DETECTOR;
        integrator->training_enable = true;
        integrator->tracker_relocation_enable = detector_stable;
        strncpy(integrator->status_message, "One detector cluster", MAX_STATUS_MSG-1);
    } else {
        if (integrator->detector_proposal_clusters[0].aux_prob > 0.95) {
            integrator->final_proposal.strobe = integrator->detector_proposal_clusters[0].strobe;
            integrator->final_proposal.src = PROPOSAL_SOURCE_DETECTOR;
            integrator->final_proposal.valid = true;
            integrator->final_proposal.prob = integrator->detector_proposal_clusters[0].aux_prob;
            integrator->training_enable = false;
            integrator->tracker_relocation_enable = false;
            strncpy(integrator->status_message, "Most confident Detector cluster", MAX_STATUS_MSG-1);
        } else {
            integrator->final_proposal.src = PROPOSAL_SOURCE_MIXED;
            integrator->final_proposal.valid = false;
            integrator->final_proposal.prob = 0.0;
            integrator->training_enable = false;
            integrator->tracker_relocation_enable = false;
            strncpy(integrator->status_message, "No reliable results", MAX_STATUS_MSG-1);
        }
    }
}
const char* integrator_get_status_message(const Integrator* integrator) {
    return integrator->status_message;
}





