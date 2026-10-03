# RZA Block15 — прямое движение после поглощения

## Статус

Текущая версия: **FIX3**.

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
89
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

Уровень **89 points** добавлен отдельно по исследовательской гипотезе пользователя как число Фибоначчи. Он не подбирался по результатам и не заменяет остальные фиксированные уровни.

## Причинность

Поглощение известно только после закрытия confirmation H1.

Direct departure считается подтверждённым только после закрытия M5 за
внешней границей rectangle в ожидаемую сторону.

Если target и первый return впервые наблюдаются на одной M5-свече,
внутрисвечной порядок не придумывается:

```text
SAME_M5_AMBIGUOUS
```

## FIX3: fail-closed data contract

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
4. Для каждого H1-интервала [H1_time, H1_time+3600) существует хотя бы один M5 bar.
   Точный M5 timestamp в hh:00 не обязателен: сессионный рынок может начинаться позже внутри часа.
5. Все доступные M5 bars внутри этого H1-интервала воспроизводят H1:
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

## Основание для FIX3

Отдельный coherence audit по текущей базе дал:

```text
PAIRS_AUDITED=522
CLEAN_PAIRS=521
BAD_PAIRS=1 (EURUSD)
H1_PARTIAL_ANCHOR=407106
H1_PARTIAL_ANCHOR_OHLC_MATCH=407058
PARTIAL_ANCHOR_MATCH_PCT_OF_PARTIAL=99.988209459
```

То есть требование exact M5 anchor в hh:00 было слишком строгим и ошибочно
смешивало нормальную сессионную структуру с реальным дефектом данных.

EURUSD остаётся incoherent series и исключается целиком: в нём сосредоточены
все 42,718 H1-часов без M5 внутри часа и все 11,464 OHLC mismatch.
Одна H1 series без M5 sibling также остаётся отдельным явным skip.

## Встроенные self-tests

Перед полным запуском `15.bat` автоматически выполняет:

```text
rza_block15.exe --selftest
```

FIX3 содержит 20 детерминированных проверок, включая:

- точную H1/M5 OHLC aggregation;
- полный H1-час без M5;
- partial-session H1 hour, где первый M5 начинается позже hh:00, но агрегат OHLC совпадает;
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

До появления новой M1-базы FIX3 позволяет использовать имеющуюся базу только
в fail-closed режиме и явно показывает, какие series не прошли data contract.

## Что не является каноническим результатом

Первичный Block15 давал, среди прочего, 59.58% для 100 points и 15.32%
для 2000 points. После обнаружения temporal-alignment contamination эти
значения считать **предварительными и не использовать как доказательство**.

Канонические цифры Block15 появятся только после PASS FIX3 на допустимых
coherent series и последующего tail/data audit.

## Repository CI

Ветка проверяется GitHub Actions на GCC, Clang и MSVC; каждый build обязан
успешно выполнить встроенный `--selftest` до допуска market run.
