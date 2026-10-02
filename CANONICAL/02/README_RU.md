# CANONICAL Block 02 — Rectangle Formation Atlas

Block 02 выполняет первый массовый причинный проход по канонической базе:

`D:\AHexaTrader\1DataFiles\raw`

Он **не торгует** и ничего не оптимизирует.

## Что фиксируется

Для каждого подтверждённого события:

- `ENGULF_2` или `ENGULF_3`;
- BULLISH / BEARISH;
- Symbol и Timeframe;
- индексы и времена source / intermediate / confirmation;
- `AvailableAt = ConfirmBarTime + PeriodSeconds`;
- границы зоны по полному High-Low исходного B0;
- OHLC участвующих баров;
- для `ENGULF_3` — raw и clamped progress первого разворотного бара;
- progress bucket:
  - `P00_25`
  - `P25_50`
  - `P50_75`
  - `P75_90`
  - `P90_100`

Progress — только измеряемый признак. Он не участвует в признании события.

## Выходы

В `CANONICAL/02out`:

- `02_FORMATION_ATLAS.csv` — одна строка на formation event;
- `02_FILE_SUMMARY.csv` — сводка по каждому XFBAR-файлу;
- `02_FAILURES.csv` — ошибки чтения/валидации;
- `02_SUMMARY.txt` — общая сводка.

## Gate

Block 02 требует:

- XFBAR001;
- version 1;
- record size 60;
- точный размер файла;
- finite OHLC;
- корректные OHLC-инварианты;
- строго возрастающее время;
- совпадение first_time/last_time с заголовком.

Канонический gate имеет нулевой tolerance: любой невалидный XFBAR даёт `BLOCK02 FAIL`.

## Чего Block 02 намеренно не делает

- не считает реакцию/пробой;
- не считает PF/Win Rate;
- не создаёт TP/SL;
- не отбирает параметры;
- не смешивает ENGULF_2 и ENGULF_3;
- не использует progress как фильтр.

Следующий блок будет измерять будущее поведение цены **после** уже замороженного `AvailableAt`.
