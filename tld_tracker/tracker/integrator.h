#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "object_model.h"
#include "tld_utils.h"
#include <stddef.h>
#include <stdbool.h>

#define MAX_CANDIDATES 256
#define MAX_STATUS_MSG 128

typedef struct {
    Candidate candidate;
    bool training_enable;  // bool as int
    bool tracker_relocation_enable; // bool as int
} IntegratorResult;

typedef struct {
    ObjectModel* model;
    IntegratorSettings settings;

    Candidate detector_raw_proposals[MAX_CANDIDATES];
    size_t detector_raw_proposals_count;

    Candidate tracker_raw_proposal;

    Candidate detector_proposal_clusters[MAX_CANDIDATES];
    size_t detector_proposal_clusters_count;

    Candidate dc_more_confident_than_tracker[MAX_CANDIDATES];
    size_t dc_more_confident_than_tracker_count;

    Candidate dc_close_to_tracker[MAX_CANDIDATES];
    size_t dc_close_to_tracker_count;

    char status_message[MAX_STATUS_MSG];

    Candidate final_proposal;
    bool training_enable;
    bool tracker_relocation_enable;
} Integrator;

void integrator_init(Integrator* integrator, ObjectModel* model);
IntegratorResult integrator_integrate(
    Integrator* integrator,
    const Candidate* det_proposals, size_t det_proposals_count,
    const Candidate* tracker_proposal
);
void integrator_get_clusters(const Integrator* integrator, Candidate* out_clusters, size_t* out_count);
void integrator_set_settings(Integrator* integrator, IntegratorSettings settings);
const char* integrator_get_status_message(const Integrator* integrator);

// Internal "private" functions (not exposed in header unless you want to)
void integrator_preprocess_candidates(Integrator* integrator);
IntegratorResult integrator_get_integration_result(const Integrator* integrator);
void integrator_subtree_tracker_result_is_reliable(Integrator* integrator);
void integrator_subtree_detector_clusters_not_reliable(Integrator* integrator);
void integrator_subtree_no_more_confident_det_clusters(Integrator* integrator);
void integrator_subtree_one_more_confident_det_cluster(Integrator* integrator);
void integrator_subtree_few_more_confident_det_clusters(Integrator* integrator);
void integrator_subtree_tracker_result_is_not_reliable(Integrator* integrator);

#endif

