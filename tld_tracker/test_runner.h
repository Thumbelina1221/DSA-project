#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Print integer array
static inline void print_int_array(const int* arr, size_t n) {
    printf("{");
    for (size_t i = 0; i < n; ++i) {
        if (i) printf(", ");
        printf("%d", arr[i]);
    }
    printf("}");
}

// Print double array
static inline void print_double_array(const double* arr, size_t n) {
    printf("{");
    for (size_t i = 0; i < n; ++i) {
        if (i) printf(", ");
        printf("%.6g", arr[i]);
    }
    printf("}");
}

// Print string array
static inline void print_str_array(const char** arr, size_t n) {
    printf("{");
    for (size_t i = 0; i < n; ++i) {
        if (i) printf(", ");
        printf("\"%s\"", arr[i]);
    }
    printf("}");
}

// Print key/value pair arrays (simulate map)
static inline void print_str_map(const char** keys, const char** vals, size_t n) {
    printf("{");
    for (size_t i = 0; i < n; ++i) {
        if (i) printf(", ");
        printf("\"%s\": \"%s\"", keys[i], vals[i]);
    }
    printf("}");
}

// Print pointer
static inline void print_ptr(const void* p) {
    printf("%p", p);
}

// Assert equal for ints
static inline void assert_equal_int(int a, int b, const char* hint) {
    if (a != b) {
        fprintf(stderr, "Assertion failed: %d != %d hint: %s\n", a, b, hint ? hint : "");
        exit(1);
    }
}

// Assert equal for doubles
static inline void assert_equal_double(double a, double b, const char* hint) {
    if (a != b) {
        fprintf(stderr, "Assertion failed: %.10g != %.10g hint: %s\n", a, b, hint ? hint : "");
        exit(1);
    }
}

// Assert equal with epsilon for doubles
static inline void assert_equal_eps(double a, double b, double eps, const char* hint) {
    if (fabs(a - b) >= eps) {
        fprintf(stderr, "Assertion failed: %.10g != %.10g (eps=%.2g) hint: %s\n", a, b, eps, hint ? hint : "");
        exit(1);
    }
}

// Assert
static inline void assert_true(int cond, const char* hint) {
    if (!cond) {
        fprintf(stderr, "Assertion failed: condition is false hint: %s\n", hint ? hint : "");
        exit(1);
    }
}

typedef void (*TestFunc)();

typedef struct {
    int fail_count;
} TestRunner;

static inline void run_test(TestRunner* tr, TestFunc func, const char* name) {
    if (!tr) return;
    if (!func) return;
    int failed = 0;
    if (setjmp(env) == 0) {
        func();
        fprintf(stderr, "%s OK\n", name);
    } else {
        ++tr->fail_count;
        fprintf(stderr, "%s fail\n", name);
    }
}

static inline void test_runner_init(TestRunner* tr) {
    tr->fail_count = 0;
}

static inline void test_runner_free(TestRunner* tr) {
    if (tr->fail_count > 0) {
        fprintf(stderr, "%d unit tests failed. Terminate\n", tr->fail_count);
        exit(1);
    }
}

// Macros for file/line printing and assertion sugar
#define ASSERT_EQUAL(x, y) \
    do { \
        char _hint[256]; \
        snprintf(_hint, sizeof(_hint), "%s != %s, %s:%d", #x, #y, __FILE__, __LINE__); \
        assert_equal_int((x), (y), _hint); \
    } while(0)

#define ASSERT_EQUAL_DBL(x, y) \
    do { \
        char _hint[256]; \
        snprintf(_hint, sizeof(_hint), "%s != %s, %s:%d", #x, #y, __FILE__, __LINE__); \
        assert_equal_double((x), (y), _hint); \
    } while(0)

#define ASSERT_EQUAL_EPS(x, y, eps) \
    do { \
        char _hint[256]; \
        snprintf(_hint, sizeof(_hint), "%s != %s, %s:%d", #x, #y, __FILE__, __LINE__); \
        assert_equal_eps((x), (y), (eps), _hint); \
    } while(0)

#define ASSERT(cond) \
    do { \
        char _hint[256]; \
        snprintf(_hint, sizeof(_hint), "%s is false, %s:%d", #cond, __FILE__, __LINE__); \
        assert_true((cond), _hint); \
    } while(0)

#define RUN_TEST(tr, func) run_test(&(tr), (func), #func)

#endif

