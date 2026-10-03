# RZA Block22 — XAUUSD only, multi-timeframe Fibonacci geometry

## Scope

Рабочая выборка сознательно сужена до XAUUSD.
Остальные инструменты не удаляются из проекта, но в Block22 не участвуют.

Сигнальные таймфреймы строятся из канонического XAUUSD M5:

- M15
- M30
- M60
- M90
- M120
- M180

Агрегация неперекрывающаяся. Используются только полностью собранные корзины M5, выровненные по кратным периода от Unix epoch.

## Сигнал

На каждом синтетическом таймфрейме применяется тот же канонический:
- formation_detector
- ABS_TRACK_v2 active-zone policy
- ENGULF_2 / ENGULF_3

## Entry

После закрытия confirmation-свечи лимит активен максимум 3 следующие M5.

BULL:
Entry = High - 0.382 × (High - Low)

BEAR:
Entry = Low + 0.382 × (High - Low)

BUY LIMIT моделируется в Ask.
SELL LIMIT — в Bid.

Если за 3 M5 лимит не достигнут:
NO_FILL.

## Geometric SL / TP

Фиксированные 89/233 points полностью убраны.

BULL:
- SL = confirmation Low
- TP = Low + 1.618 × (High - Low)

BEAR:
- SL = confirmation High
- TP = High - 1.618 × (High - Low)

При точном fill на 38.2% базовое отношение reward/risk:
1.000 / 0.618 ≈ 1.618.

Break-even Win Rate без издержек ≈ 38.2%.

## Зачем этот блок

Это не оптимизация таймфрейма и не поиск лучшего коэффициента.
Проверяется одна и та же заранее зафиксированная торговая геометрия на шести структурных масштабах XAUUSD.

Отчёты:
- 22_TRADES.csv
- 22_BY_TIMEFRAME.csv
- 22_BY_TF_TYPE.csv
- 22_BY_TF_YEAR.csv
- 22_SUMMARY.txt
