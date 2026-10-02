# CANONICAL Block 03 — ABS_TRACK zone policy

Block 03 now uses the rectangle policy from the supplied `ABS_TRACK_v2.mq4`.

The formation detector remains unified: there is one stream of confirmed engulf rectangles. Formation subtype and progress are not used for analysis.

## Ported zone rules

- `MinZoneHeightPoints = 225`
- `DistanceATRTimeframe = M5`
- `DistanceATRPeriod = 14`
- `MinGapATR = 0.30`
- `MinGapPoints = 20`
- `DeletionThreshold = 10 points`
- `ReplayHistoricalSpreadPoints = 30`

Required distance:

```text
required_gap = max(20 * Point, 0.30 * ATR(M5,14))
```

A new rectangle is accepted only if it is far enough from every still-active rectangle of the same direction.

An active bullish rectangle is removed after a bar closes below:

```text
zone_low - 10 * Point
```

A bearish rectangle is removed after a bar closes above:

```text
zone_high + 10 * Point
```

Zone bounds also follow `CalculateZoneBounds()` from ABS_TRACK, including the 225-point minimum-height rule and the 30-point historical spread floor.

## Development sample

The distance/deletion state is replayed on ALL candidate formations.

Only after a zone is accepted is the existing deterministic 1/64 development sample applied to the reaction/breakout screen.

This preserves exact active-zone state while keeping the development event file compact.
