#include "ternary_inference.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
   AvoidanceMap
   ================================================================ */

void avoidance_map_init(AvoidanceMap *map) {
    map->positions = NULL;
    map->counts    = NULL;
    map->count     = 0;
    map->capacity  = 0;
}

void avoidance_map_free(AvoidanceMap *map) {
    free(map->positions);
    free(map->counts);
    map->positions = NULL;
    map->counts    = NULL;
    map->count     = 0;
    map->capacity  = 0;
}

static void avoidance_map_ensure(AvoidanceMap *map, size_t need) {
    if (need <= map->capacity) return;
    size_t newcap = map->capacity ? map->capacity * 2 : 8;
    while (newcap < need) newcap *= 2;
    map->positions = realloc(map->positions, newcap * sizeof(int));
    map->counts    = realloc(map->counts,    newcap * sizeof(int));
    map->capacity  = newcap;
}

/* Insert in sorted order, or update existing */
void avoidance_map_add(AvoidanceMap *map, int position, int count) {
    /* Binary search for insertion point */
    size_t lo = 0, hi = map->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (map->positions[mid] < position) lo = mid + 1;
        else hi = mid;
    }
    /* If already exists, update */
    if (lo < map->count && map->positions[lo] == position) {
        map->counts[lo] += count;
        return;
    }
    /* Insert new */
    avoidance_map_ensure(map, map->count + 1);
    memmove(&map->positions[lo + 1], &map->positions[lo],
            (map->count - lo) * sizeof(int));
    memmove(&map->counts[lo + 1], &map->counts[lo],
            (map->count - lo) * sizeof(int));
    map->positions[lo] = position;
    map->counts[lo]    = count;
    map->count++;
}

int avoidance_map_get(const AvoidanceMap *map, int position) {
    /* Binary search */
    size_t lo = 0, hi = map->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (map->positions[mid] < position) lo = mid + 1;
        else hi = mid;
    }
    if (lo < map->count && map->positions[lo] == position)
        return map->counts[lo];
    return 0;
}

size_t avoidance_map_size(const AvoidanceMap *map) {
    return map->count;
}

void avoidance_map_clear(AvoidanceMap *map) {
    map->count = 0;
}

/* ================================================================
   GapFinder
   ================================================================ */

static void gap_result_ensure(GapResult *r, size_t need) {
    if (need <= r->capacity) return;
    size_t newcap = r->capacity ? r->capacity * 2 : 8;
    while (newcap < need) newcap *= 2;
    r->gaps     = realloc(r->gaps, newcap * sizeof(Gap));
    r->capacity = newcap;
}

GapResult gap_finder_find(const AvoidanceMap *map, int domain_start, int domain_end) {
    GapResult result = { NULL, 0, 0 };

    if (map->count == 0) {
        /* Entire domain is a gap */
        if (domain_start <= domain_end) {
            gap_result_ensure(&result, 1);
            result.gaps[0].start = domain_start;
            result.gaps[0].end   = domain_end;
            result.count = 1;
        }
        return result;
    }

    int cursor = domain_start;

    for (size_t i = 0; i < map->count; i++) {
        int pos = map->positions[i];
        if (pos > cursor) {
            /* Gap from cursor to pos-1 */
            int gap_start = cursor;
            int gap_end   = pos - 1;
            if (gap_start <= gap_end) {
                gap_result_ensure(&result, result.count + 1);
                result.gaps[result.count].start = gap_start;
                result.gaps[result.count].end   = gap_end;
                result.count++;
            }
        }
        cursor = pos + 1;
    }

    /* Trailing gap */
    if (cursor <= domain_end) {
        gap_result_ensure(&result, result.count + 1);
        result.gaps[result.count].start = cursor;
        result.gaps[result.count].end   = domain_end;
        result.count++;
    }

    return result;
}

void gap_result_free(GapResult *result) {
    free(result->gaps);
    result->gaps     = NULL;
    result->count    = 0;
    result->capacity = 0;
}

size_t gap_result_count(const GapResult *result) {
    return result->count;
}

/* ================================================================
   ConfidenceEstimator
   ================================================================ */

ConfidenceConfig confidence_default_config(void) {
    ConfidenceConfig cfg;
    cfg.gap_weight      = 0.4;
    cfg.neighbor_weight = 0.6;
    cfg.threshold_low   = 0.33;
    cfg.threshold_high  = 0.66;
    return cfg;
}

ConfidenceLevel confidence_classify(double score, const ConfidenceConfig *cfg) {
    if (score >= cfg->threshold_high) return CONFIDENCE_HIGH;
    if (score >= cfg->threshold_low)  return CONFIDENCE_MEDIUM;
    return CONFIDENCE_LOW;
}

double confidence_estimate(double gap_size, double neighbor_distance,
                           const ConfidenceConfig *cfg) {
    /* Larger gaps → less confidence (more uncertainty) */
    double gap_factor = 1.0 / (1.0 + gap_size * cfg->gap_weight);
    /* Closer neighbors → more confidence */
    double neighbor_factor = 1.0 / (1.0 + neighbor_distance * cfg->neighbor_weight);
    /* Combine */
    double score = (gap_factor + neighbor_factor) / 2.0;
    /* Clamp to [0, 1] */
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;
    return score;
}

/* ================================================================
   InferenceEngine
   ================================================================ */

InferenceConfig inference_default_config(void) {
    InferenceConfig cfg;
    cfg.interpolation_range = 50;
    cfg.exclusion_margin    = 2;
    return cfg;
}

DeductionSet inference_run(const AvoidanceMap *map,
                           int domain_start, int domain_end,
                           const InferenceConfig *icfg,
                           const ConfidenceConfig *ccfg) {
    DeductionSet ds;
    deduction_set_init(&ds);

    if (icfg == NULL) {
        InferenceConfig def = inference_default_config();
        return inference_run(map, domain_start, domain_end, &def, ccfg);
    }
    if (ccfg == NULL) {
        ConfidenceConfig def = confidence_default_config();
        return inference_run(map, domain_start, domain_end, icfg, &def);
    }

    /* Find gaps */
    GapResult gaps = gap_finder_find(map, domain_start, domain_end);

    for (size_t g = 0; g < gaps.count; g++) {
        int gs = gaps.gaps[g].start;
        int ge = gaps.gaps[g].end;
        int gap_size = ge - gs + 1;

        if (gap_size == 0) continue;

        /* Find the avoidance positions bordering this gap */
        int left_avoid_pos  = -1;
        int right_avoid_pos = -1;
        for (size_t i = 0; i < map->count; i++) {
            if (map->positions[i] == gs - 1) left_avoid_pos = map->positions[i];
            if (map->positions[i] == ge + 1) right_avoid_pos = map->positions[i];
        }

        /* INTERPOLATION: If gap is flanked by avoidance on both sides, interpolate midpoints */
        if (left_avoid_pos >= 0 && right_avoid_pos >= 0 &&
            gap_size <= icfg->interpolation_range) {
            int mid = gs + (ge - gs) / 2;
            double left_val  = (double)avoidance_map_get(map, left_avoid_pos);
            double right_val = (double)avoidance_map_get(map, right_avoid_pos);
            double interp    = (left_val + right_val) / 2.0;

            double neighbor_dist = (double)(right_avoid_pos - left_avoid_pos) / 2.0;
            double score = confidence_estimate((double)gap_size, neighbor_dist, ccfg);

            Deduction d;
            d.position        = (double)mid;
            d.inferred_value  = interp;
            d.type            = INFERENCE_INTERPOLATION;
            d.confidence      = confidence_classify(score, ccfg);
            d.confidence_score = score;
            deduction_set_add(&ds, d);
        }

        /* EXCLUSION: For small gaps flanked by avoidance, we can exclude positions */
        if (left_avoid_pos >= 0 && right_avoid_pos >= 0 &&
            gap_size <= icfg->exclusion_margin * 2 + 1) {
            for (int p = gs; p <= ge; p++) {
                double score = confidence_estimate(0.0,
                    (double)(p - left_avoid_pos + right_avoid_pos - p), ccfg);
                Deduction d;
                d.position        = (double)p;
                d.inferred_value  = 0.0; /* excluded */
                d.type            = INFERENCE_EXCLUSION;
                d.confidence      = confidence_classify(score, ccfg);
                d.confidence_score = score;
                deduction_set_add(&ds, d);
            }
        }

        /* BOUNDARY: At gap edges adjacent to avoidance regions */
        if (left_avoid_pos >= 0 && gap_size > 1) {
            /* Left boundary */
            double score = confidence_estimate((double)gap_size, 1.0, ccfg);
            Deduction d;
            d.position        = (double)gs;
            d.inferred_value  = (double)avoidance_map_get(map, left_avoid_pos) * 0.5;
            d.type            = INFERENCE_BOUNDARY;
            d.confidence      = confidence_classify(score, ccfg);
            d.confidence_score = score;
            deduction_set_add(&ds, d);
        }
        if (right_avoid_pos >= 0 && gap_size > 1) {
            /* Right boundary */
            double score = confidence_estimate((double)gap_size, 1.0, ccfg);
            Deduction d;
            d.position        = (double)ge;
            d.inferred_value  = (double)avoidance_map_get(map, right_avoid_pos) * 0.5;
            d.type            = INFERENCE_BOUNDARY;
            d.confidence      = confidence_classify(score, ccfg);
            d.confidence_score = score;
            deduction_set_add(&ds, d);
        }

        /* For gaps with no flanking avoidance (edge gaps), do boundary inference */
        if (left_avoid_pos < 0 && right_avoid_pos >= 0) {
            double score = confidence_estimate((double)gap_size,
                (double)(right_avoid_pos - ge), ccfg);
            Deduction d;
            d.position        = (double)ge;
            d.inferred_value  = (double)avoidance_map_get(map, right_avoid_pos) * 0.25;
            d.type            = INFERENCE_BOUNDARY;
            d.confidence      = confidence_classify(score, ccfg);
            d.confidence_score = score;
            deduction_set_add(&ds, d);
        }
        if (right_avoid_pos < 0 && left_avoid_pos >= 0) {
            double score = confidence_estimate((double)gap_size,
                (double)(gs - left_avoid_pos), ccfg);
            Deduction d;
            d.position        = (double)gs;
            d.inferred_value  = (double)avoidance_map_get(map, left_avoid_pos) * 0.25;
            d.type            = INFERENCE_BOUNDARY;
            d.confidence      = confidence_classify(score, ccfg);
            d.confidence_score = score;
            deduction_set_add(&ds, d);
        }
    }

    gap_result_free(&gaps);
    return ds;
}

/* ================================================================
   DeductionSet
   ================================================================ */

void deduction_set_init(DeductionSet *ds) {
    ds->items    = NULL;
    ds->count    = 0;
    ds->capacity = 0;
}

void deduction_set_free(DeductionSet *ds) {
    free(ds->items);
    ds->items    = NULL;
    ds->count    = 0;
    ds->capacity = 0;
}

void deduction_set_add(DeductionSet *ds, Deduction d) {
    if (ds->count >= ds->capacity) {
        size_t newcap = ds->capacity ? ds->capacity * 2 : 8;
        ds->items    = realloc(ds->items, newcap * sizeof(Deduction));
        ds->capacity = newcap;
    }
    ds->items[ds->count++] = d;
}

size_t deduction_set_size(const DeductionSet *ds) {
    return ds->count;
}

DeductionSet deduction_set_filter_by_type(const DeductionSet *ds, InferenceType type) {
    DeductionSet result;
    deduction_set_init(&result);
    for (size_t i = 0; i < ds->count; i++) {
        if (ds->items[i].type == type) {
            deduction_set_add(&result, ds->items[i]);
        }
    }
    return result;
}

DeductionSet deduction_set_filter_by_confidence(const DeductionSet *ds, ConfidenceLevel min_level) {
    DeductionSet result;
    deduction_set_init(&result);
    for (size_t i = 0; i < ds->count; i++) {
        if (ds->items[i].confidence >= min_level) {
            deduction_set_add(&result, ds->items[i]);
        }
    }
    return result;
}

Deduction *deduction_set_best(const DeductionSet *ds) {
    if (ds->count == 0) return NULL;
    Deduction *best = &ds->items[0];
    for (size_t i = 1; i < ds->count; i++) {
        if (ds->items[i].confidence_score > best->confidence_score) {
            best = &ds->items[i];
        }
    }
    return best;
}
