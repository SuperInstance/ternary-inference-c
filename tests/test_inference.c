#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../src/ternary_inference.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-50s", name);
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ----------------------------------------------------------------
   Test 1: AvoidanceMap init/add/get
   ---------------------------------------------------------------- */
static void test_avoidance_map_basic(void) {
    TEST("AvoidanceMap basic add/get");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 3);
    avoidance_map_add(&map, 20, 5);
    avoidance_map_add(&map, 5, 1);

    ASSERT(avoidance_map_size(&map) == 3, "size should be 3");
    ASSERT(avoidance_map_get(&map, 10) == 3, "pos 10 = 3");
    ASSERT(avoidance_map_get(&map, 20) == 5, "pos 20 = 5");
    ASSERT(avoidance_map_get(&map, 5) == 1,  "pos 5 = 1");
    ASSERT(avoidance_map_get(&map, 99) == 0,  "unknown pos = 0");

    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 2: AvoidanceMap duplicate add accumulates
   ---------------------------------------------------------------- */
static void test_avoidance_map_accumulate(void) {
    TEST("AvoidanceMap duplicate positions accumulate");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 2);
    avoidance_map_add(&map, 10, 3);

    ASSERT(avoidance_map_size(&map) == 1, "size should still be 1");
    ASSERT(avoidance_map_get(&map, 10) == 5, "accumulated count = 5");

    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 3: AvoidanceMap clear
   ---------------------------------------------------------------- */
static void test_avoidance_map_clear(void) {
    TEST("AvoidanceMap clear resets size");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 1, 1);
    avoidance_map_add(&map, 2, 2);
    ASSERT(avoidance_map_size(&map) == 2, "size before clear");

    avoidance_map_clear(&map);
    ASSERT(avoidance_map_size(&map) == 0, "size after clear");

    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 4: GapFinder with no avoidance = full domain gap
   ---------------------------------------------------------------- */
static void test_gap_finder_empty_map(void) {
    TEST("GapFinder empty map = full domain gap");
    AvoidanceMap map;
    avoidance_map_init(&map);

    GapResult gaps = gap_finder_find(&map, 0, 100);
    ASSERT(gap_result_count(&gaps) == 1, "one gap for entire domain");
    ASSERT(gaps.gaps[0].start == 0,   "gap starts at 0");
    ASSERT(gaps.gaps[0].end == 100,   "gap ends at 100");

    gap_result_free(&gaps);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 5: GapFinder finds gaps between avoidance positions
   ---------------------------------------------------------------- */
static void test_gap_finder_between_avoidance(void) {
    TEST("GapFinder gaps between avoidance positions");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 1);
    avoidance_map_add(&map, 20, 1);

    GapResult gaps = gap_finder_find(&map, 0, 30);
    /* Expected gaps: [0,9], [11,19], [21,30] */
    ASSERT(gap_result_count(&gaps) == 3, "should find 3 gaps");
    ASSERT(gaps.gaps[0].start == 0  && gaps.gaps[0].end == 9,  "first gap [0,9]");
    ASSERT(gaps.gaps[1].start == 11 && gaps.gaps[1].end == 19, "second gap [11,19]");
    ASSERT(gaps.gaps[2].start == 21 && gaps.gaps[2].end == 30, "third gap [21,30]");

    gap_result_free(&gaps);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 6: GapFinder with edge avoidance
   ---------------------------------------------------------------- */
static void test_gap_finder_edge_cases(void) {
    TEST("GapFinder avoidance at domain edges");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 0, 1);
    avoidance_map_add(&map, 10, 1);

    GapResult gaps = gap_finder_find(&map, 0, 10);
    /* Avoidance at 0 and 10 → one gap [1,9] */
    ASSERT(gap_result_count(&gaps) == 1, "one gap");
    ASSERT(gaps.gaps[0].start == 1 && gaps.gaps[0].end == 9, "gap [1,9]");

    gap_result_free(&gaps);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 7: ConfidenceEstimator defaults and classification
   ---------------------------------------------------------------- */
static void test_confidence_estimator(void) {
    TEST("ConfidenceEstimator classify and estimate");
    ConfidenceConfig cfg = confidence_default_config();

    ASSERT(cfg.threshold_low  > 0.0, "threshold_low > 0");
    ASSERT(cfg.threshold_high > cfg.threshold_low, "threshold_high > low");

    ASSERT(confidence_classify(0.8, &cfg) == CONFIDENCE_HIGH,   "0.8 = HIGH");
    ASSERT(confidence_classify(0.5, &cfg) == CONFIDENCE_MEDIUM, "0.5 = MEDIUM");
    ASSERT(confidence_classify(0.1, &cfg) == CONFIDENCE_LOW,    "0.1 = LOW");

    /* Small gap + close neighbor = high confidence */
    double score = confidence_estimate(1.0, 1.0, &cfg);
    ASSERT(score > 0.5, "small gap/close neighbor => score > 0.5");

    /* Large gap = lower confidence */
    double score2 = confidence_estimate(100.0, 100.0, &cfg);
    ASSERT(score2 < score, "larger gap/distance => lower score");

    PASS();
}

/* ----------------------------------------------------------------
   Test 8: InferenceEngine interpolation between two avoidance points
   ---------------------------------------------------------------- */
static void test_inference_interpolation(void) {
    TEST("InferenceEngine interpolation between avoidance");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 4);
    avoidance_map_add(&map, 14, 6);

    InferenceConfig icfg = inference_default_config();
    ConfidenceConfig ccfg = confidence_default_config();

    DeductionSet ds = inference_run(&map, 0, 20, &icfg, &ccfg);

    /* Should have at least one interpolation deduction */
    DeductionSet interp = deduction_set_filter_by_type(&ds, INFERENCE_INTERPOLATION);
    ASSERT(deduction_set_size(&interp) >= 1, "at least one interpolation");

    /* Interpolated value should be (4+6)/2 = 5.0 */
    bool found_interp = false;
    for (size_t i = 0; i < interp.count; i++) {
        if (fabs(interp.items[i].inferred_value - 5.0) < 0.01) {
            found_interp = true;
            break;
        }
    }
    ASSERT(found_interp, "interpolated value should be ~5.0");

    deduction_set_free(&interp);
    deduction_set_free(&ds);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 9: InferenceEngine exclusion for small gaps
   ---------------------------------------------------------------- */
static void test_inference_exclusion(void) {
    TEST("InferenceEngine exclusion for small gaps");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 1);
    avoidance_map_add(&map, 12, 1);
    /* Gap is just position 11 — small enough for exclusion */

    InferenceConfig icfg = inference_default_config();
    ConfidenceConfig ccfg = confidence_default_config();

    DeductionSet ds = inference_run(&map, 0, 20, &icfg, &ccfg);

    DeductionSet excl = deduction_set_filter_by_type(&ds, INFERENCE_EXCLUSION);
    ASSERT(deduction_set_size(&excl) >= 1, "at least one exclusion deduction");

    deduction_set_free(&excl);
    deduction_set_free(&ds);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 10: InferenceEngine boundary deductions
   ---------------------------------------------------------------- */
static void test_inference_boundary(void) {
    TEST("InferenceEngine boundary at gap edges");
    AvoidanceMap map;
    avoidance_map_init(&map);

    avoidance_map_add(&map, 10, 2);
    avoidance_map_add(&map, 20, 3);
    /* Gap [11,19] → boundary inferences at 11 and 19 */

    InferenceConfig icfg = inference_default_config();
    ConfidenceConfig ccfg = confidence_default_config();

    DeductionSet ds = inference_run(&map, 0, 30, &icfg, &ccfg);

    DeductionSet boundary = deduction_set_filter_by_type(&ds, INFERENCE_BOUNDARY);
    ASSERT(deduction_set_size(&boundary) >= 2, "at least 2 boundary deductions");

    deduction_set_free(&boundary);
    deduction_set_free(&ds);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 11: DeductionSet filtering by confidence
   ---------------------------------------------------------------- */
static void test_deduction_filter_confidence(void) {
    TEST("DeductionSet filter by confidence level");

    /* Manually construct a set */
    DeductionSet ds;
    deduction_set_init(&ds);

    Deduction d1 = { 1.0, 10.0, INFERENCE_INTERPOLATION, CONFIDENCE_HIGH, 0.9 };
    Deduction d2 = { 2.0, 20.0, INFERENCE_BOUNDARY,      CONFIDENCE_LOW,  0.1 };
    Deduction d3 = { 3.0, 30.0, INFERENCE_EXCLUSION,      CONFIDENCE_MEDIUM, 0.5 };
    deduction_set_add(&ds, d1);
    deduction_set_add(&ds, d2);
    deduction_set_add(&ds, d3);

    DeductionSet high = deduction_set_filter_by_confidence(&ds, CONFIDENCE_HIGH);
    ASSERT(deduction_set_size(&high) == 1, "only 1 HIGH");
    ASSERT(fabs(high.items[0].position - 1.0) < 0.001, "it's the HIGH one");

    DeductionSet med_up = deduction_set_filter_by_confidence(&ds, CONFIDENCE_MEDIUM);
    ASSERT(deduction_set_size(&med_up) == 2, "MEDIUM + HIGH = 2");

    deduction_set_free(&high);
    deduction_set_free(&med_up);
    deduction_set_free(&ds);
    PASS();
}

/* ----------------------------------------------------------------
   Test 12: DeductionSet best returns highest confidence
   ---------------------------------------------------------------- */
static void test_deduction_best(void) {
    TEST("DeductionSet best returns highest confidence");

    DeductionSet ds;
    deduction_set_init(&ds);

    Deduction d1 = { 1.0, 10.0, INFERENCE_INTERPOLATION, CONFIDENCE_LOW, 0.2 };
    Deduction d2 = { 2.0, 20.0, INFERENCE_BOUNDARY,      CONFIDENCE_HIGH, 0.95 };
    Deduction d3 = { 3.0, 30.0, INFERENCE_EXCLUSION,      CONFIDENCE_MEDIUM, 0.6 };
    deduction_set_add(&ds, d1);
    deduction_set_add(&ds, d2);
    deduction_set_add(&ds, d3);

    Deduction *best = deduction_set_best(&ds);
    ASSERT(best != NULL, "best is not NULL");
    ASSERT(fabs(best->confidence_score - 0.95) < 0.001, "best score = 0.95");
    ASSERT(fabs(best->position - 2.0) < 0.001, "best position = 2.0");

    deduction_set_free(&ds);
    PASS();
}

/* ----------------------------------------------------------------
   Test 13: InferenceEngine on empty avoidance map
   ---------------------------------------------------------------- */
static void test_inference_empty_map(void) {
    TEST("InferenceEngine empty map produces no deductions");
    AvoidanceMap map;
    avoidance_map_init(&map);

    InferenceConfig icfg = inference_default_config();
    ConfidenceConfig ccfg = confidence_default_config();

    DeductionSet ds = inference_run(&map, 0, 100, &icfg, &ccfg);
    /* No avoidance → one big gap with no flanking → boundary at edges
       but no neighbors → minimal deductions */
    /* Actually with our logic, no avoidance means no flanking avoidance,
       so no boundary deductions either. Count may vary but should be small. */
    ASSERT(deduction_set_size(&ds) == 0, "no deductions with empty map");

    deduction_set_free(&ds);
    avoidance_map_free(&map);
    PASS();
}

/* ----------------------------------------------------------------
   Test 14: DeductionSet best on empty set returns NULL
   ---------------------------------------------------------------- */
static void test_deduction_best_empty(void) {
    TEST("DeductionSet best on empty set returns NULL");

    DeductionSet ds;
    deduction_set_init(&ds);

    Deduction *best = deduction_set_best(&ds);
    ASSERT(best == NULL, "best should be NULL for empty set");

    deduction_set_free(&ds);
    PASS();
}

/* ----------------------------------------------------------------
   Test 15: ConfidenceEstimator score in [0,1]
   ---------------------------------------------------------------- */
static void test_confidence_bounds(void) {
    TEST("ConfidenceEstimator score always in [0,1]");
    ConfidenceConfig cfg = confidence_default_config();

    double extremes[][2] = {
        {0.0, 0.0}, {1000.0, 1000.0}, {0.0, 1000.0}, {1000.0, 0.0},
        {-1.0, -1.0}
    };
    for (int i = 0; i < 5; i++) {
        double score = confidence_estimate(extremes[i][0], extremes[i][1], &cfg);
        ASSERT(score >= 0.0 && score <= 1.0, "score in [0,1]");
    }
    PASS();
}

/* ================================================================== */

int main(void) {
    printf("\n=== Ternary Inference Tests ===\n\n");

    test_avoidance_map_basic();
    test_avoidance_map_accumulate();
    test_avoidance_map_clear();
    test_gap_finder_empty_map();
    test_gap_finder_between_avoidance();
    test_gap_finder_edge_cases();
    test_confidence_estimator();
    test_inference_interpolation();
    test_inference_exclusion();
    test_inference_boundary();
    test_deduction_filter_confidence();
    test_deduction_best();
    test_inference_empty_map();
    test_deduction_best_empty();
    test_confidence_bounds();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
