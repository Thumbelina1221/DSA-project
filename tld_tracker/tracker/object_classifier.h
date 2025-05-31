#ifndef OBJECT_CLASSIFIER_H
#define OBJECT_CLASSIFIER_H

#include <stddef.h>

typedef struct {
    size_t descriptors_cnt;
    size_t* positive_distribution;
    size_t* negative_distribution;
    double* posterior_prob_distribution;
    char* updated; // boolean array: 0 = false, 1 = true
    size_t positive_distr_max;
} ObjectClassifier;

void object_classifier_init(ObjectClassifier* clf, size_t descriptors_cnt) {
    clf->descriptors_cnt = descriptors_cnt;
    clf->positive_distribution = (size_t*)calloc(descriptors_cnt, sizeof(size_t));
    clf->negative_distribution = (size_t*)calloc(descriptors_cnt, sizeof(size_t));
    clf->posterior_prob_distribution = (double*)calloc(descriptors_cnt, sizeof(double));
    clf->updated = (char*)calloc(descriptors_cnt, sizeof(char)); // All false
    clf->positive_distr_max = 0;
}

void object_classifier_free(ObjectClassifier* clf);
void object_classifier_reset(ObjectClassifier* clf) {
    for (size_t i = 0; i < clf->descriptors_cnt; ++i) {
        clf->positive_distribution[i] = 0;
        clf->negative_distribution[i] = 0;
        clf->posterior_prob_distribution[i] = 0.0;
        clf->updated[i] = 0; // false
    }
    clf->positive_distr_max = 0;
}


size_t object_classifier_train_positive(ObjectClassifier* clf, size_t x) {
    clf->updated[x] = 0; // false
    size_t res = ++clf->positive_distribution[x];
    if (res > clf->positive_distr_max)
        clf->positive_distr_max = res;
    return res;
}
size_t object_classifier_train_negative(ObjectClassifier* clf, size_t x) {
    clf->updated[x] = 0; // false
    return ++clf->negative_distribution[x];
}

size_t object_classifier_get_max_positive(const ObjectClassifier* clf) {
    return clf->positive_distr_max;
}


double object_classifier_predict(ObjectClassifier* clf, size_t x) {
    if (clf->updated[x])
        return clf->posterior_prob_distribution[x];
    else
        return object_classifier_update_prob(clf, x);
}


size_t object_classifier_get_positive_distr(const ObjectClassifier* clf, size_t x) {
    return clf->positive_distribution[x];
}

size_t object_classifier_get_negative_distr(const ObjectClassifier* clf, size_t x) {
    return clf->negative_distribution[x];
}
double object_classifier_update_prob(ObjectClassifier* clf, size_t x) {
    size_t p_cnt = clf->positive_distribution[x];
    size_t n_cnt = clf->negative_distribution[x];
    if (p_cnt == 0) {
        clf->posterior_prob_distribution[x] = 0.0;
    } else if (n_cnt != 0) {
        clf->posterior_prob_distribution[x] = (double)p_cnt / (p_cnt + n_cnt);
    } else {
        clf->posterior_prob_distribution[x] = 1.0;
    }
    clf->updated[x] = 1;
    return clf->posterior_prob_distribution[x];
}


#endif

