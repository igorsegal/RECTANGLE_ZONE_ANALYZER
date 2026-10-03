# RZA Block19 — вход после закрытия поглощающей H1-свечи

## Методологическая поправка

Прямоугольник описывает поглощённую исходную структуру, но не является точкой входа.

Сигнал возникает только после того, как подтверждающая H1-свеча завершила поглощение:

- ENGULF_2 bullish: бычья confirmation.close > open предыдущей медвежьей source-свечи;
- ENGULF_2 bearish: медвежья confirmation.close < open предыдущей бычьей source-свечи;
- ENGULF_3: второй бар нового направления завершает ту же проверку относительно open исходной source-свечи.

Это полностью совпадает с каноническим detect_at().

## Causal execution

Финальный Close H1 известен только после завершения бара. Поэтому честное исполнение:
- reference = Close confirmation H1;
- actual entry = первый доступный M5 open следующего H1;
- BUY = Ask open с историческим XFBAR spread;
- SELL = Bid open.

Block19 отдельно пишет EntryGapPoints между confirmation close и фактическим entry.

## Торговый контракт

- CORE10 без изменений.
- TP = 89 quote-points от фактического entry.
- SL = 233 quote-points от фактического entry.
- SELL TP/SL контролируются по Ask, оценённому как Bid OHLC + XFBAR spread.
- Если TP и SL попадают в один M5 и порядок неизвестен — консервативно SL.
- Позиция после открытия живёт до TP/SL или конца данных; удаление прямоугольника не закрывает уже открытую сделку.
- Slippage = 0, не выдумывается.
- Commission не включается без broker symbol specs.

## Выходы

- 19_TRADES.csv
- 19_GLOBAL.csv
- 19_BY_SYMBOL.csv
- 19_BY_YEAR.csv
- 19_BY_TYPE.csv
- 19_SUMMARY.txt

Отдельно сравниваются ENGULF_2 и ENGULF_3.
