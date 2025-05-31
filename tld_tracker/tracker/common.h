#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

// Constants
#define BINARY_DESCRIPTOR_WIDTH     (11)
#define BINARY_DESCRIPTOR_CNT       (2048)
#define CLASSIFIERS_CNT             (10)

// BinaryDescriptor type
typedef uint16_t BinaryDescriptor;

// 2D Point (double precision)
typedef struct {
    double x;
    double y;
} Point2d;

// 2D Point (integer)
typedef struct {
    int x;
    int y;
} Point2i;

// Pair of double-precision points (NormFernPair)
typedef struct {
    Point2d first;
    Point2d second;
} NormFernPair;

// Pair of integer points (AbsFernPair)
typedef struct {
    Point2i first;
    Point2i second;
} AbsFernPair;

// Pair of size_t values (PixelIdPair)
typedef struct {
    size_t first;
    size_t second;
} PixelIdPair;

#endif // COMMON_H

