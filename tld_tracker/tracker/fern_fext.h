#ifndef FERN_FEATURE_EXTRACTOR_H
#define FERN_FEATURE_EXTRACTOR_H

#include "common.h"
#include "scanning_grid.h"

// Forward declaration for BinaryDescriptor
typedef uint16_t BinaryDescriptor;

typedef struct FernFeatureExtractor {
    ScanningGrid* grid;
} FernFeatureExtractor;

// Constructor
void fern_feature_extractor_init(FernFeatureExtractor* extractor, ScanningGrid* grid);

// GetDescriptor (frame, position, scale_id)
BinaryDescriptor fern_feature_extractor_get_descriptor_by_position(
    FernFeatureExtractor* extractor,
    Image* frame,
    Size position,
    size_t scale_id
);

// GetDescriptor (frame, bbox)
BinaryDescriptor fern_feature_extractor_get_descriptor_by_bbox(
    FernFeatureExtractor* extractor,
    Image* frame,
    Rect bbox
);

// GetDescriptor (frame)
BinaryDescriptor fern_feature_extractor_get_descriptor(
    FernFeatureExtractor* extractor,
    Image* frame
);

// Operator() overload (alias of get_descriptor_by_position)
#define fern_feature_extractor_call fern_feature_extractor_get_descriptor_by_position

#endif // FERN_FEATURE_EXTRACTOR_H

