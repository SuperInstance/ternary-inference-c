#ifndef TERNARY_INFERENCE_H
#define TERNARY_INFERENCE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Types ---- */

typedef enum {
    INFERENCE_INTERPOLATION,   /* value inferred between two known points */
    INFERENCE_EXCLUSION,       /* value excluded based on surrounding data */
    INFERENCE_BOUNDARY,        /* value inferred at edge of known region */
} InferenceType;

typedef enum {
    CONFIDENCE_LOW    = 0,
    CONFIDENCE_MEDIUM = 1,
    CONFIDENCE_HIGH   = 2,
} ConfidenceLevel;

typedef struct {
    double position;           /* spatial position of the inference */
    double inferred_value;     /* the deduced value */
    InferenceType type;        /* kind of inference */
    ConfidenceLevel confidence;/* qualitative confidence */
    double confidence_score;   /* numeric confidence 0..1 */
} Deduction;

typedef struct {
    Deduction *items;
    size_t count;
    size_t capacity;
} DeductionSet;

/* AvoidanceMap: tracks avoidance counts at discrete positions */
typedef struct {
    int    *positions;         /* sorted positions */
    int    *counts;            /* avoidance count per position */
    size_t  count;
    size_t  capacity;
} AvoidanceMap;

/* Gap: a contiguous range between avoidance regions */
typedef struct {
    int start;
    int end;
} Gap;

/* GapFinder result */
typedef struct {
    Gap    *gaps;
    size_t  count;
    size_t  capacity;
} GapResult;

/* ConfidenceEstimator config */
typedef struct {
    double gap_weight;         /* weight of gap size on confidence */
    double neighbor_weight;    /* weight of neighbor proximity */
    double threshold_low;      /* below this = LOW confidence */
    double threshold_high;     /* above this = HIGH confidence */
} ConfidenceConfig;

/* InferenceEngine config */
typedef struct {
    int interpolation_range;   /* max range for interpolation */
    int exclusion_margin;      /* margin for exclusion inference */
} InferenceConfig;

/* ---- AvoidanceMap ---- */

void avoidance_map_init(AvoidanceMap *map);
void avoidance_map_free(AvoidanceMap *map);
void avoidance_map_add(AvoidanceMap *map, int position, int count);
int  avoidance_map_get(const AvoidanceMap *map, int position);
size_t avoidance_map_size(const AvoidanceMap *map);
void avoidance_map_clear(AvoidanceMap *map);

/* ---- GapFinder ---- */

GapResult gap_finder_find(const AvoidanceMap *map, int domain_start, int domain_end);
void     gap_result_free(GapResult *result);
size_t   gap_result_count(const GapResult *result);

/* ---- ConfidenceEstimator ---- */

ConfidenceConfig confidence_default_config(void);
ConfidenceLevel  confidence_classify(double score, const ConfidenceConfig *cfg);
double           confidence_estimate(double gap_size, double neighbor_distance,
                                     const ConfidenceConfig *cfg);

/* ---- InferenceEngine ---- */

InferenceConfig inference_default_config(void);
DeductionSet    inference_run(const AvoidanceMap *map,
                              int domain_start, int domain_end,
                              const InferenceConfig *icfg,
                              const ConfidenceConfig *ccfg);

/* ---- DeductionSet ---- */

void          deduction_set_init(DeductionSet *ds);
void          deduction_set_free(DeductionSet *ds);
void          deduction_set_add(DeductionSet *ds, Deduction d);
size_t        deduction_set_size(const DeductionSet *ds);
DeductionSet  deduction_set_filter_by_type(const DeductionSet *ds, InferenceType type);
DeductionSet  deduction_set_filter_by_confidence(const DeductionSet *ds, ConfidenceLevel min_level);
Deduction    *deduction_set_best(const DeductionSet *ds);  /* highest confidence */

#ifdef __cplusplus
}
#endif

#endif /* TERNARY_INFERENCE_H */
