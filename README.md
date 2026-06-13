# Ternary Inference (C)

**Ternary Inference** is a C library for deducing latent knowledge from ternary avoidance patterns. Given an avoidance map — a record of which positions in a space were systematically avoided (η = −1) — the inference engine identifies gaps, estimates confidence, and produces structured deductions about what the avoidance implies.

## Why It Matters

Inference from absence is a fundamentally different epistemological mode than inference from presence. Standard statistical inference asks "what can we conclude from the data we observed?" Negative-space inference asks "what can we conclude from the data that's *missing*?" If an agent systematically avoids positions 5–12 in a 20-position space, what does it know about positions 5–12? This library operationalizes that question with three inference types: interpolation (filling between known points), exclusion (deducing what's absent from surrounding data), and boundary (inferring at edges of the known region). Applications range from network security (which ports are avoided reveals firewall knowledge) to ecology (which habitats are avoided reveals predator presence) to market analysis (which stocks aren't traded reveals insider information).

## How It Works

### Avoidance Map

The `AvoidanceMap` stores (position, count) pairs sorted by position:

```c
typedef struct {
    int *positions;   // sorted positions
    int *counts;      // avoidance count per position
    size_t count;
} AvoidanceMap;
```

Adding a position is O(log n) (binary search for insertion point), memory is O(n).

### Gap Finding

The `GapFinder` identifies contiguous regions between avoidance clusters:

```
Given avoidance at positions [1,2,3] and [8,9,10] in domain [0,20]:
Gap 1: [4, 7]   (between avoidance clusters)
Gap 2: [11, 20] (after last cluster)
```

Gaps are found in a single O(n) sweep over sorted positions. Each gap's width contributes to the knowledge estimate:

```
knowledge(gap) = 1 - exp(-λ · width · density)
```

### Confidence Estimation

Each deduction carries a confidence score based on:

- **Gap size**: wider gaps → higher confidence (more evidence)
- **Neighbor distance**: closer to known data → higher confidence

```
confidence = w₁ · σ(gap_size) + w₂ · σ(1/neighbor_distance)
```

where σ is a sigmoid function. Confidence is classified as LOW, MEDIUM, or HIGH using configurable thresholds.

### Three Inference Types

1. **Interpolation**: If positions A and B are known (avoided or chosen), and position C is between them, infer C's value:
   ```
   C_inferred = interpolate(A, B, distance_ratio)
   ```

2. **Exclusion**: If a region is surrounded by high avoidance on all sides, the interior is likely also avoided:
   ```
   P(avoided | surrounded_by_avoidance) ∝ product of neighbor avoidance counts
   ```

3. **Boundary**: At the edge of the known region, confidence decreases:
   ```
   confidence_boundary = confidence_base · (1 - distance_to_data / max_range)
   ```

### DeductionSet Operations

Results are collected in a `DeductionSet` that supports:
- Filter by inference type
- Filter by minimum confidence level
- Select highest-confidence deduction

All O(n) in the number of deductions.

## Quick Start

```c
#include "ternary_inference.h"
#include <stdio.h>

int main(void) {
    AvoidanceMap map;
    avoidance_map_init(&map);
    avoidance_map_add(&map, 1, 5);
    avoidance_map_add(&map, 2, 8);
    avoidance_map_add(&map, 3, 6);
    // Gap at 4-7, then avoidance resumes
    avoidance_map_add(&map, 8, 7);
    avoidance_map_add(&map, 9, 4);

    InferenceConfig icfg = inference_default_config();
    ConfidenceConfig ccfg = confidence_default_config();

    DeductionSet ds = inference_run(&map, 0, 20, &icfg, &ccfg);

    printf("Deductions: %zu\n", deduction_set_size(&ds));
    Deduction *best = deduction_set_best(&ds);
    if (best) {
        printf("Best: pos=%d, value=%.2f, confidence=%.2f\n",
               (int)best->position, best->inferred_value,
               best->confidence_score);
    }

    deduction_set_free(&ds);
    avoidance_map_free(&map);
    return 0;
}
```

Compile: `gcc -lm -o demo src/ternary_inference.c && ./demo`

## API

| Type | Key Functions | Description |
|------|--------------|-------------|
| `AvoidanceMap` | `init`, `add`, `get`, `clear` | Position-count store |
| `GapResult` | `gap_finder_find` | Find gaps in avoidance |
| `ConfidenceConfig` | `confidence_default_config`, `confidence_classify`, `confidence_estimate` | Confidence scoring |
| `InferenceEngine` | `inference_run` | Main inference entry point |
| `DeductionSet` | `size`, `filter_by_type`, `filter_by_confidence`, `best` | Result collection |

## Architecture Notes

Ternary Inference is the pure η (eta) engine of the γ + η = C framework — it *only* reasons about absence. Every deduction is derived from what's missing, not what's present. The three inference types (interpolation, exclusion, boundary) correspond to different ways that η propagates: filling internal voids (interpolation), extending avoidance from boundaries (exclusion), and decreasing confidence at edges (boundary). The confidence scores measure how much η-competence the system has — low confidence means the void is genuine ignorance, high confidence means it's knowledgeable avoidance. See [ARCHITECTURE.md](https://github.com/SuperInstance/SuperInstance/blob/main/ARCHITECTURE.md).

## References

1. Pearl, J. (2009). *Causality: Models, Reasoning, and Inference*, 2nd ed. Cambridge University Press. — On reasoning with missing data.
2. Little, R. J. A., & Rubin, D. B. (2002). *Statistical Analysis with Missing Data*, 2nd ed. Wiley. — Missing data mechanisms.
3. Jaynes, E. T. (2003). *Probability Theory: The Logic of Science*. Cambridge University Press. — Maximum entropy inference from incomplete information.

## License

MIT
