#include "fern.h"
#include "tld_utils.h"
#include <stdlib.h>
#include <string.h>

Fern* fern_create(size_t pairs_count) {
    Fern* fern = (Fern*)malloc(sizeof(Fern));
    fern->pairs = (NormFernPair*)malloc(sizeof(NormFernPair) * pairs_count);
    fern->pairs_count = pairs_count;
    for (size_t i = 0; i < pairs_count; ++i) {
        NormFernPoint p1, p2;
        p1.x = get_normalized_random();
        p1.y = get_normalized_random();
        double orientation = get_normalized_random();
        double aux = get_normalized_random();
        if (orientation > 0.5) {
            p2.x = p1.x;
            p2.y = aux;
        } else {
            p2.y = p1.y;
            p2.x = aux;
        }
        fern->pairs[i].p1 = p1;
        fern->pairs[i].p2 = p2;
    }
    return fern;
}

Fern* fern_copy(const Fern* other) {
    Fern* fern = (Fern*)malloc(sizeof(Fern));
    fern->pairs_count = other->pairs_count;
    fern->pairs = (NormFernPair*)malloc(sizeof(NormFernPair) * fern->pairs_count);
    memcpy(fern->pairs, other->pairs, sizeof(NormFernPair) * fern->pairs_count);
    return fern;
}

AbsFernPair* fern_transform(const Fern* fern, int bbox_width, int bbox_height, size_t* out_count) {
    size_t n = fern->pairs_count;
    AbsFernPair* out = (AbsFernPair*)malloc(sizeof(AbsFernPair) * n);
    for (size_t i = 0; i < n; ++i) {
        const NormFernPoint p1 = fern->pairs[i].p1;
        const NormFernPoint p2 = fern->pairs[i].p2;
        out[i].first.x = (int)(p1.x * bbox_width);
        out[i].first.y = (int)(p1.y * bbox_height);
        out[i].second.x = (int)(p2.x * bbox_width);
        out[i].second.y = (int)(p2.y * bbox_height);
    }
    if (out_count) *out_count = n;
    return out;
}

size_t fern_get_pairs_count(const Fern* fern) {
    return fern->pairs_count;
}

void fern_free(Fern* fern) {
    if (!fern) return;
    free(fern->pairs);
    free(fern);
}

