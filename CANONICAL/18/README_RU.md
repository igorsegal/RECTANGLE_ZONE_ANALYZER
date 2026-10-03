# RZA Block18 — causal edge pending replay

Block18 исправляет главный недостаток Block17: мы больше не ждём закрытия M5 за прямоугольником.

## Популяция
Не используется отфильтрованный 15_EVENTS.csv.
Block18 заново строит все принятые ABS_TRACK_v2-прямоугольники на H1 для CORE10.

## Causal entry
Сигнал становится известен после закрытия confirmation H1. Pending активируется с открытия следующего H1 (signal_time).

BULL:
- ждём первое касание Bid уровня ZoneHigh;
- обычное исполнение: ZoneHigh + spread;
- если M5 открылся уже выше границы: исполнение по реальному open + spread.

BEAR:
- ждём первое касание Bid уровня ZoneLow;
- обычное исполнение: ZoneLow;
- gap ниже границы: по реальному open.

После фактического входа:
- TP = 89 quote-points;
- SL = 233 quote-points.

BUY TP/SL контролируются по Bid.
SELL TP/SL — по Ask, приближённому как Bid OHLC + исторический XFBAR spread.

Если вход произошёл внутри M5 и та же свеча допускает неблагоприятный SL, но внутрисвечной порядок неизвестен, Block18 fail-closed считает это SL и отдельно считает ENTRY_BAR_AMBIGUOUS_AS_SL.

## Почему это всё ещё не MT4 tick test
M5 OHLC не хранит внутрисвечной tick path. Поэтому Block18 — строгий исторический фильтр, а не финальный execution proof. Если результат выдержит этот консервативный тест, следующий этап — MT4 Strategy Tester на тиковом моделировании.

## Fixed
- CORE10
- TP = 89
- SL = 233
- Start deposit = USD 10,000
- Leverage = 1:500
- Min Margin Level = 500%
- без сетки и оптимизации
