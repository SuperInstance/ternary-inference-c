# Future Integration: ternary-inference-c

## Current State
C implementation of ternary inference — deducing knowledge from negative spaces. Observes what agents avoid and infers the hidden constraints driving that avoidance.

## Integration Opportunities

### With negative-space-core-c
Core defines negative space; inference extracts knowledge from it. Together on ESP32: a lightweight reasoning engine that deduces room state from avoidance patterns. No LLM needed — pure logical inference from observed behavior.

### With ternary-logic (Rust)
Three-valued logic provides the inference rules. Kleene logic handles unknowns gracefully — when avoidance data is incomplete, inference produces "unknown" rather than wrong answers. The C port implements the inference algorithm; Rust defines the logical framework.

### With compiled-policy-c
Inferred knowledge becomes policy. When inference deduces "action X should be avoided," that becomes a compiled policy rule. The pipeline: observe behavior → infer constraints → compile to policy → deploy on edge.

## Potential in Mature Systems
In room-as-codespace, inference runs on every edge device. Each device observes local agent behavior, infers hidden constraints, and adapts its policies accordingly. This is distributed intelligence: no central coordinator needed for local adaptation. PLATO aggregates inferences across the fleet for global insight.

## Cross-Pollination Ideas
- Inference as room learning — rooms get smarter over time by observing their agents
- Cross-device inference aggregation — combine negative space profiles from multiple ESP32s
- Inference confidence tracking — weight recent observations more heavily than old ones

## Dependencies for Next Steps
- Integration with negative-space-core-c for combined reasoning pipeline
- Inference rule format compatible with compiled-policy-c
- FFI bindings for Rust interop
