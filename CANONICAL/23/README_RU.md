# RZA Block23 — XAUUSD M90/M120/M180, comparison of fixed TP sizes

## Fixed signal contract

Only XAUUSD.

Signal timeframes:
- M90
- M120
- M180

All signal candles are built from canonical XAUUSD M5 using complete non-overlapping buckets.

Entry is unchanged from Block22:
- BULL: High - 0.382 × (High - Low)
- BEAR: Low + 0.382 × (High - Low)
- pending lifetime: exactly 3 M5 bars
- BUY LIMIT is modeled in Ask
- SELL LIMIT in Bid

SL is unchanged:
- BULL SL = confirmation Low
- BEAR SL = confirmation High

## Target scenarios

For every accepted signal the same entry is evaluated in five parallel target scenarios:

- TP150
- TP200
- TP300
- TP400
- GEOMETRIC_1618

Fixed targets are measured from the actual filled entry price.

GEOMETRIC_1618 is the unchanged Block22 control:
- BULL = Low + 1.618 × confirmation range
- BEAR = High - 1.618 × confirmation range

No other parameters change between scenarios.

## Purpose

This block answers one question only:

For the larger XAUUSD signal scales M90/M120/M180, is a fixed target of 150/200/300/400 quote-points more suitable than the fully geometric 161.8 target?

This is an explicit user-requested comparison, not an automatic optimizer.

## Outputs

- 23_TRADES.csv
- 23_BY_TF_TARGET.csv
- 23_BY_TF_TARGET_TYPE.csv
- 23_BY_TF_TARGET_YEAR.csv
- 23_SUMMARY.txt
