# RZA Block15 FIX2R — SUPERPOWER CODE AUDIT

Дата: 2026-10-03

## Итог

- Первичный Block15: **исследовательски superseded**.
- FIX1: **REJECTED BEFORE MARKET TEST**.
- FIX2 исходный: после дополнительной ревизии найден fail-open crash-recovery.
- FIX2R: **STATIC CONTRACT REVIEW PASS**.
- Market-data run FIX2R: **не выполнялся до CI/локального compile+selftest**.

## Критические дефекты, найденные аудитом

### 1. Temporal alignment

Старый код выбирал первый M5 bar `>= SignalTime`.
Если M5-история начиналась спустя месяцы/годы, старый H1 signal мог быть
состыкован с далёким будущим M5.

FIX2R: до исследования вся H1/M5 series проходит preflight.

### 2. Price-scale mismatch

Одинаковый timestamp ещё не гарантирует одинаковый price scale.

FIX2R проверяет:
- Digits;
- Point;
- H1/M5 Open;
- H1 High vs max M5 High;
- H1 Low vs min M5 Low;
- H1 Close vs last M5 Close.

Допуск: один quote point.

### 3. Coherence check стоял слишком поздно

FIX1 проверял конкретный signal уже после M5 ATR / gap logic.
Значит, плохой M5 мог изменить accepted-zone population.

FIX2R: full-series preflight выполняется до ATR и до stateful replay.

### 4. Проверка только signal point недостаточна

Даже хороший SignalTime не доказывает корректность остальной истории.

FIX2R проверяет каждый H1 interval; первый дефект исключает series целиком.

### 5. Конфликтующие M5 duplicates

Раньше первый найденный M5 sibling мог быть выбран без доказательства
тождественности других файлов.

FIX2R:
- identical duplicates допускаются;
- conflicting duplicates => DATA_CONTRACT_ERROR.

### 6. BY_YEAR lifecycle counters

В старой реализации часть годовых счётчиков отражала только direct-path
события и могла создавать ложное впечатление `AcceptedZones == DirectPaths`.

FIX2R учитывает accepted/no-departure на соответствующих стадиях lifecycle.

### 7. Fail-open crash recovery

Дополнительная ревизия после первого Superpower отчёта нашла ещё одну
архитектурную проблему: BAT мог после аварийного завершения добавить symbol
в skip-list, перезапустить блок и в итоге получить PASS без этого symbol.

Для исследовательского кода это недопустимо.

FIX2R:
- auto-skip после crash удалён;
- crash => BLOCK FAIL;
- `15_IN_PROGRESS.txt` остаётся как диагностический указатель;
- продолжение возможно только после понимания и исправления причины.

## Детерминированные self-tests

FIX2R содержит 20 проверок:

1. exact H1/M5 OHLC aggregate PASS;
2. late-start M5 reject;
3. same-time Open scale mismatch reject;
4. Point mismatch reject;
5. chronological first High hit;
6. chronological first Close departure;
7. bullish return requires actual rectangle intersection;
8. High aggregate mismatch reject;
9. Digits mismatch reject;
10. target-before-return classification;
11. same-M5 target/return ambiguity;
12. lifecycle-end miss;
13. percentage denominator arithmetic;
14. internal missing H1-hour M5 anchor reject;
15. Low aggregate mismatch reject;
16. Close aggregate mismatch reject;
17. return-before-target classification;
18. data-end is censored;
19. bearish return requires actual rectangle intersection;
20. one-quote-point tolerance boundary.

## Отдельно зафиксированное ограничение

Полная H1/M5 coherence не доказывает экономическую однородность истории,
если corporate action / split согласованно пересчитал одновременно H1 и M5.

Поэтому после будущего PASS market run обязателен tail audit экстремальных
excursions.

## Будущая data architecture

Каноническая цель проекта:

```text
M1 single source of truth
→ deterministic resampling
→ M5/M15/M30/H1/H4
```

FIX2R — защитный режим для текущей неоднородной базы, а не замена будущей
M1-канонизации.

## Допуск

FIX2R можно допускать к market run только после:
1. repository CI compile;
2. deterministic selftests PASS;
3. Windows local compile+selftest PASS либо эквивалентного Windows CI PASS.
