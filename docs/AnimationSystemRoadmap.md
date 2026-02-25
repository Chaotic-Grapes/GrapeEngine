# Animation System Roadmap

## Current Coverage
- Sprite-sheet animation with uniform frame timing (FPS).
- Row or frame-window playback.
- Looping/non-looping playback.
- Runtime state tracking via AnimationState2D.
- Editor preview via AnimationPreviewSystem.

## Missing Features
- Clip library and animation controller/state machine.
- Transitions and blend trees.
- Per-frame timing (holds) and variable frame durations.
- Reverse/ping-pong playback and seek controls.
- Per-frame UVs, pivots, and per-frame offsets (atlas metadata).
- Animation notifies/events tied to clip frames.
- Runtime blending between multiple animations per entity.
- Animation layering/masking (upper/lower body or additive layers).
- Sync groups for multi-entity or multi-track timing alignment.
- Root motion or transform-driven animation output.
- Serialization of animation clips and controllers as assets.

## Suggested Phases
1. Clips + controller (state machine, transitions, events/notifies).
2. Per-frame timing + ping-pong + seek.
3. Per-frame atlas metadata (UVs, pivots, offsets).
4. Blending (blend trees, layered animation).
