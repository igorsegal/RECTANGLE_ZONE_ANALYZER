# CANONICAL Block 03 — Full per-instrument ABS_TRACK test

Block 03 is rebuilt after the ABS_TRACK_v2 audit.

## Test unit

The scientific test unit is:

```text
SYMBOL + TIMEFRAME
```

The number of BIN files is only database metadata. It is not treated as the number of tested market events.

## Formation detection

Detection follows the supplied ABS_TRACK order exactly:

1. test the latest two closed bars;
2. only if no two-bar match exists, test the three-bar construction.

The three-bar detector has no extra condition requiring the intermediate bar to remain inside the source body.

If an older source already created a live rectangle, the active-zone distance rule suppresses the duplicate later candidate.

## Zone policy

The supplied ABS_TRACK_v2 rules are used:

```text
MinZoneHeightPoints = 225
RequiredGap = max(20 * Point, 0.30 * ATR(M5,14))
DeletionThreshold = 10 * Point
HistoricalSpreadFloor = 30 * Point
```

Only a rectangle accepted by those rules enters the reaction/breakout test.

## No sampling

There is no 1/64 development sample anymore.

Every accepted development rectangle is evaluated.

No giant event-level CSV is written. The main result is compact:

```text
03_INSTRUMENT_STATS.csv
```

Columns include:

```text
Symbol
Timeframe
CandidateFormations
AcceptedZones
RejectedGap
Resolved
Reaction
Breakout
ReactionPct
CI95
OtherLifecycle
```

The overall summary is secondary and is written to `03_SUMMARY.txt`.

The old 61.01% result belongs to the previous zone definition and is obsolete.

2024+ remains reserved for the final OOS.
