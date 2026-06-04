# ternary-inference-c

C implementation of ternary inference — deducing knowledge from negative spaces.

## Concept

Ternary inference works in three states:
- **Known** (positive) — directly observed data
- **Unknown** (negative) — explicitly absent  
- **Inferred** — deduced from the structure of gaps between known points

By analyzing avoidance patterns and the gaps between them, we can interpolate, exclude, and bound probable values.

## Components

- **AvoidanceMap** — Maps positions to avoidance counts, maintained in sorted order
- **GapFinder** — Identifies contiguous gaps between avoidance regions
- **InferenceEngine** — Produces deductions from gaps via interpolation, exclusion, and boundary analysis
- **ConfidenceEstimator** — Estimates confidence of each inference based on gap size and neighbor proximity
- **DeductionSet** — Collection of deductions with filtering by type and confidence level

## Build & Test

```bash
gcc -o test_inference tests/test_inference.c src/ternary_inference.c -lm -Wall -O2
./test_inference
```

## Inference Types

| Type | Description |
|------|-------------|
| `INFERENCE_INTERPOLATION` | Value inferred between two known avoidance points |
| `INFERENCE_EXCLUSION` | Position excluded based on surrounding avoidance data |
| `INFERENCE_BOUNDARY` | Value inferred at edge of known/unknown boundary |

## Confidence Levels

- **HIGH** (≥ 0.66) — Strong evidence from tight, well-flanked gaps
- **MEDIUM** (≥ 0.33) — Moderate evidence
- **LOW** (< 0.33) — Weak evidence from large or poorly bounded gaps

## License

MIT
