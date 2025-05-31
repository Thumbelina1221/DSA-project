#ifndef FERN_H
#define FERN_H

#include "common.h"
#include <stddef.h>
#include <stdlib.h>

// Fern data structure in C
typedef struct {
    NormFernPair* pairs;   // dynamic array of pairs
    size_t pairs_count;    // number of pairs
} Fern;

// Constructor: create Fern with given number of random pairs (you'll need a random init)
Fern* fern_create(size_t pairs_count);

// Copy constructor: deep copy another Fern
Fern* fern_copy(const Fern* other);

// Destroy/free Fern
void fern_free(Fern* fern);

// Get number of pairs
size_t fern_get_pairs_count(const Fern* fern);

// Transform: create AbsFernPair array for a given base bounding box size
// Returns dynamically allocated array, out_count will be set to array size
AbsFernPair* fern_transform(const Fern* fern, int bbox_width, int bbox_height, size_t* out_count);

// Alternative "init from array": initialize a Fern from an array of NormFernPair
Fern* fern_create_from_array(const NormFernPair* arr, size_t count);

// Example of iterator: use fern->pairs and fern->pairs_count directly

#endif // FERN_H

