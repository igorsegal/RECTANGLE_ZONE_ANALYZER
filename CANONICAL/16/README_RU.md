# RZA Block16 — CORE10 causal trade replay

## Замороженная корзина

Индексы:
- DJ30
- NAS100
- GER40
- SP500
- US2000

Металлы:
- XAUUSD
- XAUAUD
- XAUJPY
- XPDUSD
- XPTUSD

## Торговый контракт

- Источник событий: Block15 FIX3, только 89 points.
- Вход разрешён только после закрытия M5-бара, подтвердившего выход из родительского прямоугольника.
- Исполнение входа: open следующего реально существующего M5-бара.
- BUY входит по Ask = Bid open + исторический spread из XFBAR.
- SELL входит по Bid open.
- TP = 89 quote-points от фактической цены входа.
- BUY SL = нижняя граница прямоугольника минус 1 quote-point.
- SELL SL = верхняя граница прямоугольника плюс 1 quote-point.
- Для SELL TP/SL проверяются по Ask, приближённому как Bid OHLC + spread текущего M5-бара.
- Если TP и SL находятся внутри одного M5-бара и порядок неизвестен, результат консервативно считается SL.
- Slippage не выдумывается: 0 points на этом этапе.
- Spread учитывается из исторических XFBAR.

## Счёт

Заморожено:
- стартовый депозит: USD 10,000;
- плечо: 1:500;
- минимальный Margin Level: 500%;
- политика размера позиции: broker minimum lot;
- число одновременных позиций не ограничивается, если проходит margin rule.

Точный USD PnL, комиссия и margin simulation требуют реальных symbol specifications конкретного брокерского счёта: minimum lot, tick size, tick value, margin required/contract size и commission. Block16 не подставляет выдуманные значения; сначала проверяет саму торговую геометрию в points и R.

## Выходы

- 16_TRADES.csv
- 16_GLOBAL.csv
- 16_BY_SYMBOL.csv
- 16_BY_YEAR.csv
- 16_SUMMARY.txt

Главные метрики: Win Rate, Avg R, Profit Factor в R, средний initial risk в points, max simultaneous trades.
