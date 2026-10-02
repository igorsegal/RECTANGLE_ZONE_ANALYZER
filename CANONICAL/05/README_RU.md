# CANONICAL Block 05 — Final OOS with ABS_TRACK policy

Final OOS uses the same unified rectangle stream and the zone rules ported from `ABS_TRACK_v2.mq4`.

## Important

Before testing 2024+ events, Block 05 replays the full earlier history causally so that active old rectangles can correctly block a new same-direction rectangle.

## Ported rules

- minimum zone-height rule: 225 points;
- minimum same-direction gap: max(20 points, 0.30 × ATR(M5,14));
- zone deletion after close 10 points beyond the opposite boundary;
- historical spread floor: 30 points.

Formation subtype and progress are not used.

## OOS sampling

There is no 1/64 reduction in final OOS.

Every accepted 2024+ rectangle is evaluated.

The output is summary-only, so no giant OOS event CSV is written.
