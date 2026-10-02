# RZA v2 — программа реализации

> **Для агентных исполнителей:** ОБЯЗАТЕЛЬНЫЙ ПОДНАВЫК: использовать `superpowers:subagent-driven-development` (рекомендуется) или `superpowers:executing-plans`, выполняя планы по задачам с чекбоксами.

**Цель:** Поэтапно построить и доказательно проверить RZA v2, не смешивая независимые подсистемы в один неревьюируемый план.

**Архитектура:** Новое ядро `rza::v2` создаётся рядом с отдельно собираемым legacy. Каждая фаза заканчивается работающим, тестируемым продуктовым срезом и собственным verification gate.

**Стек:** C++17, CMake 4.4+, CTest, MinGW-w64 GCC 16.1, PowerShell, Git, RZB2.

**Спецификация:** `docs/superpowers/specs/2026-09-01-rza-v2-design.md`

## Общие ограничения

- Все официальные расчёты соблюдают 3 календарных месяца IS + 1 месяц OOS, шаг 1 месяц, UTC.
- Риски: 0,25% сделка, 0,5% инструмент, 2% портфель, USD 100 000.
- Вход только на следующем баре; Bid/Ask; SL раньше TP; закрытие в конце OOS.
- Финансовые fallback-значения запрещены; невалидная спецификация отклоняется.
- RZB2 каноничен; JSON служит config/manifest; CSV создаётся только экспортом.
- Каждая поведенческая задача следует RED → GREEN → REFACTOR → REGRESSION → COMMIT.
- Legacy не является oracle и не вызывается официальным v2-контуром.

---

## Последовательность фаз

1. **Foundation:** CMake/CTest, строгие warnings, v2/legacy targets, scenario test support, clean-build документация.
2. **Storage and data:** безопасный XFBAR reader, data validation, RZB2 header/records/checksum/atomic write/mmap, I/O benchmark.
3. **Instrument math:** time-versioned specs, классы активов, историческая FX-конверсия, P&L и sizing golden tests.
4. **Portfolio:** ledger, account, risk reservations, equity, combined PF и chronological DD.
5. **Execution:** signals/orders/fills, next-bar/Bid-Ask, costs/gaps, SL-first, management и end-OOS.
6. **Temporal walk-forward:** calendar folds, TemporalView, FrozenFoldModel, poisoning/future-extension/resume tests.
7. **Causal discovery:** ATR, zones, touches, features/outcomes и local rule candidates, по одному компоненту.
8. **Hierarchical optimization:** inner purged validation, hard filters, robust score, effective samples и plateau medoid.
9. **Experiment/reporting:** immutable manifest, CLI lifecycle, RZB2 artifacts, diagnostics, CSV export и sensitivity.
10. **Shadow/default migration:** real-data shadow audit, parallel determinism, full gates, default switch и legacy archive.

Для каждой следующей фазы перед реализацией создаётся отдельный файл `docs/superpowers/plans/YYYY-MM-DD-rza-v2-<phase>.md` с точными интерфейсами, тестами и commit-границами. План фазы 1: `2026-09-01-rza-v2-foundation.md`.

