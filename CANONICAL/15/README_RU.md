# RZA Block15 — прямое движение после поглощения

## Статус

Текущая версия: **FIX2R**.

Это исследование поведения цены, а не торговая модель.

Старый первичный прогон Block15 и FIX1 считаются superseded:
они не являются каноническим доказательством, потому что аудит выявил
недостаточную защиту H1/M5 temporal/price coherence.

## Исследуемая последовательность

```text
accepted H1 engulfing / rectangle
→ confirmation H1 закрылась
→ signal time = open следующего H1
→ первый M5 close за внешней границей rectangle
  в направлении engulfing
→ DIRECT DEPARTURE
→ движение до первого последующего return/touch parent rectangle
  либо до H1 deletion / data end
```

Reaction leg не используется.

## Уровни

```text
100
150
200
250
300
350
400
500
1000
2000 points
```

Отсчёт:

```text
BULL: zone_high + N * Point
BEAR: zone_low  - N * Point
```

Это расстояния от parent outer edge, не от исполнимой торговой цены.

## Причинность

Поглощение известно только после закрытия confirmation H1.

Direct departure считается подтверждённым только после закрытия M5 за
внешней границей rectangle в ожидаемую сторону.

Если target и первый return впервые наблюдаются на одной M5-свече,
внутрисвечной порядок не придумывается:

```text
SAME_M5_AMBIGUOUS
```

## FIX2R: fail-closed data contract

Главное изменение — H1/M5 проверяются **до** ATR, gap gate, active-zone replay
и измерения direct path.

Порядок:

```text
load H1 + M5
→ full-series H1/M5 coherence preflight
→ only PASS series
→ M5 ATR
→ ABS_TRACK gap/lifecycle replay
→ accepted zones
→ direct-move measurement
```

Для каждой H1-серии обязательно:

1. H1 и M5 принадлежат одному symbol.
2. `Digits` совпадает.
3. `Point` совпадает.
4. Для каждого H1 open timestamp существует M5 bar с тем же timestamp.
5. M5 aggregate внутри H1-интервала воспроизводит H1:
   - Open,
   - max High,
   - min Low,
   - last Close,
   с допуском не более одного quote point.
6. Любой дефект исключает **всю series** до research replay.
7. Несколько M5-файлов одного symbol допускаются только если их semantic
   fingerprints совпадают; конфликтующие duplicates дают DATA_CONTRACT_ERROR.
8. Runtime crash никогда не превращается в молчаливый skip symbol.
   Block завершается FAIL и оставляет `15_IN_PROGRESS.txt` с символом,
   на котором остановился процесс.

Это не торговые фильтры. Это защита целостности входных данных.

## Почему исключается вся series

ABS_TRACK — stateful: старый rectangle влияет на допустимость последующих
rectangle через active-zone gap/lifecycle.

Если пропустить только один испорченный H1/M5 участок и продолжить дальше,
active state уже может отличаться от истинного.

Поэтому при неизвестной когерентности серия не частично чинится, а целиком
не допускается к статистике.

## Встроенные self-tests

Перед полным запуском `15.bat` автоматически выполняет:

```text
rza_block15.exe --selftest
```

FIX2R содержит 20 детерминированных проверок, включая:

- точную H1/M5 OHLC aggregation;
- позднее начало M5;
- внутренне отсутствующий H1-hour M5 anchor;
- Open/High/Low/Close mismatch;
- Point mismatch;
- Digits mismatch;
- chronological first-hit search;
- bullish и bearish return/touch geometry;
- target-before-return;
- return-before-target;
- same-M5 ambiguity;
- lifecycle miss;
- censored data end;
- clear/lower/upper percentage arithmetic;
- one-quote-point tolerance.

Self-test не заменяет полный market run. Он доказывает только детерминированные
контракты кода.

## Выходы

```text
CANONICAL\15out\15_EVENTS.csv
CANONICAL\15out\15_GLOBAL.csv
CANONICAL\15out\15_BY_SYMBOL.csv
CANONICAL\15out\15_BY_YEAR.csv
CANONICAL\15out\15_SUMMARY.txt
CANONICAL\15out\15_SKIPPED.csv
CANONICAL\15out\15_FAILURES.csv
CANONICAL\15out\15_DATA_INTEGRITY.csv
CANONICAL\15out\15_IN_PROGRESS.txt   (только если процесс аварийно оборвался)
```

## Ограничение существующей базы

Даже строгая H1/M5 coherence-проверка не превращает текущую историческую базу
в идеальный источник данных.

Долгосрочная каноническая архитектура проекта:

```text
single source of truth = M1
M1 → deterministic builder → M5/M15/M30/H1/H4
```

До появления новой M1-базы FIX2R позволяет использовать имеющуюся базу только
в fail-closed режиме и явно показывает, какие series не прошли data contract.

## Что не является каноническим результатом

Первичный Block15 давал, среди прочего, 59.58% для 100 points и 15.32%
для 2000 points. После обнаружения temporal-alignment contamination эти
значения считать **предварительными и не использовать как доказательство**.

Канонические цифры Block15 появятся только после PASS FIX2R на допустимых
coherent series и последующего tail/data audit.
