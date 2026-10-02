# RectangleZoneAnalyzer v2 — архитектурная спецификация

**Статус:** согласовано 2026-09-01. **Baseline:** commit `332c341`, тег `legacy-baseline-2026-09-01`.

## Цель

RZA v2 — воспроизводимый, причинный и детерминированный walk-forward движок рядом с legacy-кодом. Поддерживаются все инструменты из `symbol_specs.csv` с доказуемой спецификацией; финансовые значения по умолчанию запрещены.

## Неизменяемая политика

- USD 100 000; риск 0,25% на сделку, 0,5% на инструмент, 2% на портфель от текущего equity.
- Rolling: 3 полных календарных месяца IS, 1 месяц OOS, шаг 1 месяц, UTC, интервалы `[start,end)`.
- Решение после close, вход на следующем open; BUY Ask/Bid, SELL Bid/Ask.
- При TP+SL в одном баре первым считается SL; новая защита действует со следующего бара.
- В конце OOS заявки отменяются, позиции закрываются с издержками; OOS не участвует в выборе.
- RZB2 — канонический формат; JSON — config/manifest; CSV — экспорт.

## Архитектура и данные

`src/v2` содержит `core`, `data`, `storage`, `instruments`, `discovery`, `validation`, `optimization`, `execution`, `portfolio`, `walkforward`, `reporting`, `cli` в namespace `rza::v2`. Нижние слои не вызывают верхние; legacy собирается отдельно и не вызывается v2.

XFBAR проверяется по заголовку, размеру, записям, времени, finite/OHLC-инвариантам, spread, gaps и календарю. Нужно 95% ожидаемых баров в каждом месяце и минимум 5 инструментов в группе. Входы не исправляются на месте.

`TemporalView` запрещает доступ вне диапазона; `available_at <= decision_time`. Features и будущие outcomes — разные типы. Зона видна после close подтверждающего бара, ATR причинный. `FrozenFoldModel` фиксирует границы, групповые параметры `AssetClass+Timeframe`, локальные правила `Symbol+Timeframe`, спецификации, политики, исключения и hashes.

## Оптимизация

В IS применяется недельная purged expanding validation: начальные 4 недели train, forward validation, purge и embargo. Фильтры: ≥30 групповых сделок, ≥5 локальных наблюдений до иерархической оценки, PF>1,05, DD<15%, прибыльны ≥60% символов, концентрация ≤35%, ≥5 инструментов, положительный expectancy и достаточная effective sample.

Score: 30% robust expectancy, 25% lower bound, 20% межсимвольная стабильность, 15% recovery, 10% effective sample минус штрафы за DD, концентрацию, сложность и turnover. Выбирается medoid устойчивого плато. OOS отсутствует в API оптимизатора; отсутствие модели допустимо.

## Исполнение и портфель

Версионированный `InstrumentSpec` задаёт актив, валюты, tick/contract/volume, издержки, календарь и период действия. P&L считается через ticks и историческую FX-конверсию в USD. Неполная спецификация или конверсия отклоняет сделку. Объём округляется вниз и повторно проверяется против риска; стоп не расширяется.

Единый событийный engine обрабатывает gap, SL, TP, timeout и close-решения. Slippage неблагоприятен, spread не списывается дважды. Portfolio владеет капиталом и reservations. Ledger хранит все денежные проводки и точно воспроизводит balance/equity. PF считается по объединённым net trades, DD — по хронологической equity curve. Equity≤0 прекращает experiment.

## RZB2 и воспроизводимость

RZB2 содержит magic, schema/version, endianness, размеры, IDs, checksum, metadata и typed payload; поддерживает bounded reads, atomic rename и mmap. В нём хранятся бары, features, candidates, events, trades, ledger, equity, models и checkpoints. Скорость сравнивается benchmark без заранее заявленного коэффициента.

Manifest фиксирует Git commit/cleanliness, binary/compiler/environment, config/spec/input hashes, folds, model hashes, seeds и lineage. Официальный run требует чистого commit. Повторно использованный для разработки OOS теряет независимый статус.

## Тестовая стратегия

CMake/CTest: unit, property, metamorphic, integration, leakage, regression, differential, acceptance, benchmark и golden. Процесс: `RED → GREEN → REFACTOR → REGRESSION → COMMIT`.

Обязательны ручные P&L/risk тесты Forex, металлов, индексов, equity/crypto CFD; next-bar/Bid-Ask/cost/gap/TP-SL/end-OOS; календарь; ledger replay; PF/DD; повреждённые XFBAR/RZB2; round-trip/version/checksum/atomicity; plateau; determinism; resume и sequential/parallel equivalence.

Leakage gates: future-access, feature timestamps, zone confirmation, outcome/API separation, OOS poisoning, future extension и file-order permutation. Первичный gate включает 25 согласованных регрессий по исполнению, активам, FX, рискам, ledger, folds, leakage, RZB2, resume, cleanup и invalid specs.

Задача завершена только при подтверждённом RED для дефекта, свежих успешных проверках, отсутствии новых warnings, доступных static/sanitizer checks, корректном Git diff, отсутствии secrets/generated staging и benchmark горячего пути.

## Миграция

Порядок: CMake/tests → RZB2/input → instruments/math → ledger/risk → execution → walk-forward → causal discovery → hierarchical optimization → reporting → shadow audit. Каждый компонент проходит contract, tests, differential/leakage диагностику, review и отдельный commit.

V2 становится default после всех gates, отсутствия P0/P1, OOS-only folds, точного ledger, повторяемых hashes, эквивалентности sequential/parallel/resume, полного manifest, shadow audit и clean-build проверки. Legacy затем архивируется; удаление требует отдельного разрешения. Не переносятся absolute paths, every-seventh sampling, Forex constants, averaged portfolio metrics, unconditional success, dual engines, signal-close fills, TP-first, исчезающие позиции, empty catch, фиктивные config fields, profitability claims и credentials.

Git остаётся локальным без явного remote. Пароль из `results/fetch_bybit_and_merge.py` необходимо сменить; secrets поступают только из environment или ignored store.

Отсутствие стратегии или сделок — корректный результат. Исследовательская целостность и риск важнее прибыли и скорости.
