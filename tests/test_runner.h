/*
  Minimal single-header test framework for FilterFunctions.
  Usage: call SUITE(), CHECK(), CHECK_NEAR() in test functions, then REPORT() at the end of main().
*/
#pragma once
#include <cstdio>
#include <cmath>

static int _tr_run  = 0;
static int _tr_fail = 0;

#define SUITE(name) \
    do { printf("\n[%s]\n", (name)); } while (0)

#define CHECK(expr) \
    do { \
        ++_tr_run; \
        if (!(expr)) { \
            ++_tr_fail; \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        } \
    } while (0)

#define CHECK_NEAR(a, b, tol) \
    do { \
        ++_tr_run; \
        float _a = static_cast<float>(a); \
        float _b = static_cast<float>(b); \
        float _t = static_cast<float>(tol); \
        if (fabsf(_a - _b) > _t) { \
            ++_tr_fail; \
            printf("  FAIL  %s:%d  |%s - %s| = %.6f > %.6f\n", \
                   __FILE__, __LINE__, #a, #b, \
                   static_cast<double>(fabsf(_a - _b)), \
                   static_cast<double>(_t)); \
        } \
    } while (0)

#define REPORT() \
    do { \
        printf("\n--- %d / %d passed", _tr_run - _tr_fail, _tr_run); \
        if (_tr_fail) printf("  (%d FAILED)", _tr_fail); \
        printf(" ---\n"); \
        return (_tr_fail > 0) ? 1 : 0; \
    } while (0)
