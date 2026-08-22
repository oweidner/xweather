#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

/* Minimal assert-macro test harness -- no framework, no dependencies beyond
 * libc. Each test file is its own standalone `main()`, so these file-scope
 * statics never collide across translation units. */

static int test_count = 0;
static int test_failures = 0;

#define TEST_ASSERT(cond) \
    do { \
        test_count++; \
        if (!(cond)) { \
            test_failures++; \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b) \
    do { \
        test_count++; \
        if (strcmp((a), (b)) != 0) { \
            test_failures++; \
            fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        } \
    } while (0)

#define TEST_ASSERT_INT_EQ(a, b) \
    do { \
        test_count++; \
        if ((a) != (b)) { \
            test_failures++; \
            fprintf(stderr, "FAIL %s:%d: %d != %d\n", __FILE__, __LINE__, (int)(a), (int)(b)); \
        } \
    } while (0)

#define TEST_SUMMARY() \
    do { \
        fprintf(stderr, "%d/%d passed\n", test_count - test_failures, test_count); \
        return test_failures ? 1 : 0; \
    } while (0)

#endif /* TEST_H */
