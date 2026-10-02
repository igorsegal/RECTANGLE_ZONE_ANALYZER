# CANONICAL Block 05 — Final OOS 2024+

Это последняя независимая проверка текущей структурной гипотезы RZA.

## Замороженная логика

Block 05 использует ровно ту же логику, что новый Block 03:

- единица теста — `Symbol + Timeframe`;
- порядок обнаружения formation соответствует `ABS_TRACK_v2`;
- границы прямоугольника соответствуют `ABS_TRACK_v2`;
- `RequiredGap = max(20 * Point, 0.30 * ATR(M5,14))`;
- удаление живой зоны — после close за противоположной границей на 10 points;
- исторический spread-floor — 30 points;
- formation subtype не используется;
- progress не используется;
- sampling отключён.

## Важный принцип OOS

Перед началом 2024+ полный предыдущий ряд проигрывается причинно.

Это нужно, чтобы живые уровни, созданные до 2024 года, корректно существовали или были удалены к моменту первого OOS-события.

Только после этого считаются результаты событий с decision-time:

```text
>= 2024-01-01T00:00:00Z
```

## Результаты

Основной файл:

```text
05_OOS_INSTRUMENT_STATS.csv
```

Одна строка = один `Symbol + Timeframe`.

Поля:

```text
Symbol
Timeframe
OOSCandidateFormations
OOSAcceptedZones
OOSRejectedGap
Resolved
Reaction
Breakout
ReactionPct
CI95LowPct
CI95HighPct
OtherLifecycle
```

Также создаются:

```text
05_OOS_SUMMARY.txt
05_FAILURES.csv
```

## Контроль чистоты

- одинаковый дубликат ряда учитывается один раз;
- конфликтующий дубликат = FAIL;
- отсутствие M5 ATR = явное исключение ряда;
- все принятые OOS-зоны тестируются, 1/64 нет;
- параметры после просмотра OOS не меняются.

Block 05 не является PnL-тестом. Он проверяет только воспроизводимость реакции/пробоя на независимом периоде 2024+.


## OOS trade emulation

Block 05 использует абсолютно ту же замороженную эмуляцию сделки, что Block 03, и пишет:

```text
05_OOS_TRADE_EMULATION.csv
```

Правила входа/выхода и учёта исторического spread не меняются после просмотра development-результата.


### FIX — исполнение только на следующем Open

OOS использует ту же строгую причинную модель исполнения, что development:

```text
signal known at bar close -> execution at next real bar open
```

Никакого исполнения по уже известному Close сигнального бара.
