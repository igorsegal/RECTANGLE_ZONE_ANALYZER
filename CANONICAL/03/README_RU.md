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


## Контроль уникальности и ATR

Каждая пара `Symbol + Timeframe` должна быть уникальной.

Если в базе найдены два полностью одинаковых ряда для одной пары, второй ряд не учитывается повторно и фиксируется как `DUPLICATE_IDENTICAL_SERIES`.

Если для одной пары найдены разные ряды, Block 03 завершится FAIL как `DUPLICATE_SERIES_CONFLICT`.

Если для ряда невозможно получить M5 ATR, этот ряд явно исключается из теста. Использовать только `MinGapPoints` вместо полного правила ABS_TRACK запрещено.


## Эмуляция сделки

В том же проходе Block 03 теперь создаёт `03_TRADE_EMULATION.csv`.

Правило сделки специально простое и причинное:

```text
bullish zone -> BUY
bearish zone -> SELL
entry = close первого touch-бара внутри зоны
exit  = close первого следующего бара, закрывшегося вне зоны
```

Для исполнения используется исторический spread из XFBAR:

- BUY: вход по Ask = Bid close + spread, выход по Bid close;
- SELL: вход по Bid close, выход по Ask = Bid close + spread.

Комиссия, slippage и swap пока равны нулю и явно фиксируются в summary.

Это event-level эмуляция сделки, а не портфельный/маржинальный backtest. Одновременные события пока оцениваются независимо.


### FIX — строго причинное исполнение

Предыдущая первая версия эмуляции использовала Close touch/outcome бара как цену исполнения. Это слишком оптимистично: сам факт touch/outcome известен только после закрытия этого бара.

Исправлено:

```text
touch подтверждён на Close бара T
BUY/SELL исполняется на Open бара T+1

outcome подтверждён на Close бара X
закрытие сделки исполняется на Open бара X+1
```

Spread берётся уже с фактического бара исполнения.

Также исправлен формат CSV для серий с нулём закрытых сделок: теперь каждая строка имеет ровно 13 полей.


## Глубина реакции после touch

Добавлен файл:

```text
03_REACTION_LEVELS.csv
```

Для каждого `Symbol + Timeframe` считаются уровни:

```text
100, 150, 200, 250, 300, 350, 400, 500 points
```

Определение реакции:

- сначала должен состояться наш подтверждённый close-touch после ожидаемого departure;
- измерение начинается только со следующего бара после touch, чтобы не использовать неизвестную внутрибараную последовательность;
- bullish: расстояние считается вверх от внешней границы `ZoneHigh`;
- bearish: расстояние считается вниз от внешней границы `ZoneLow`;
- измеряется максимальное благоприятное отклонение, пока уровень жив по правилам ABS_TRACK;
- конец измерения — бар удаления уровня по ABS_TRACK или development cutoff.

`Hit100 ... Hit500` — кумулятивные величины. Если реакция дошла до 350 points, она учитывается также в 100/150/200/250/300.
