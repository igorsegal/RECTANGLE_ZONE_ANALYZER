# RZA Block17 — CORE10: TP 89 / SL 233

Чистая проверка одной заранее заданной гипотезы Фибоначчи.

## Замороженная корзина
DJ30, NAS100, GER40, SP500, US2000, XAUUSD, XAUAUD, XAUJPY, XPDUSD, XPTUSD.

## Торговый контракт
- Источник событий: Block15 FIX3, 89-only.
- Вход: open следующего M5 после закрытия M5, подтвердившего departure.
- BUY: вход по Ask с историческим XFBAR spread.
- SELL: вход по Bid.
- TP = 89 quote-points от фактической цены входа.
- SL = 233 quote-points от фактической цены входа:
  - BUY: Entry - 233 points;
  - SELL: Entry + 233 points.
- SELL TP/SL контролируются по Ask ≈ Bid + spread текущего M5.
- Если TP и SL достигаются в одной M5-свече и порядок неизвестен: консервативно SL.
- Slippage = 0, не выдумывается.
- Commission пока не включается без спецификации брокера.

## Счёт
- Start deposit = USD 10,000.
- Leverage = 1:500.
- Minimum Margin Level = 500%.
- Position size policy = broker minimum lot.
- Точный USD PnL и margin требуют broker symbol specifications.

## Важно
Никакой сетки SL нет. Проверяется только одна гипотеза: TP=89 / SL=233.
Теоретический break-even Win Rate без издержек:
233 / (233 + 89) = 72.36%.

## Выходы
17_TRADES.csv
17_GLOBAL.csv
17_BY_SYMBOL.csv
17_BY_YEAR.csv
17_SUMMARY.txt
