# RZA — политика исторических данных

## Каноническая цель

Единственный источник истины для будущих исследований:

```text
M1
```

Все старшие timeframe должны быть детерминированно построены из той же M1
серии:

```text
M1 → M5
M1 → M15
M1 → M30
M1 → H1
M1 → H4
```

Независимо скачанные H1/M5/H4 не считаются эквивалентом такого контракта.

## Почему

Текущая база содержит серии разной глубины и с неодинаковыми пробелами.
Для cross-timeframe исследования это может приводить к:
- temporal misalignment;
- различному price scale;
- несовместимым corporate-action adjustments;
- ложной причинности;
- state contamination в stateful алгоритмах.

## Правила текущего переходного периода

Пока M1 single-source база не построена:

1. cross-timeframe блоки работают fail-closed;
2. H1/M5 coherence проверяется до research replay;
3. конфликтующие duplicates запрещены;
4. неизвестные/несогласованные series исключаются целиком;
5. exclusion всегда отражается в явном data-integrity output;
6. никакой runtime crash не превращается в молчаливый skip;
7. результаты старых H1/M5 блоков считаются исторической исследовательской
   информацией, но не заменяют будущую перепроверку на M1-derived data.

## Будущий M1 builder contract

При появлении новой M1 истории builder должен:

- фиксировать timezone до агрегации;
- сохранять timestamp order;
- дедуплицировать только полностью identical bars;
- conflicting duplicate => DATA_CONTRACT_ERROR;
- инвентаризировать gaps;
- не дорисовывать отсутствующие M1;
- маркировать derived interval как INCOMPLETE, если его полнота не доказана;
- записывать provenance source M1;
- строить OHLC стандартно:
  - Open = first M1 Open;
  - High = max M1 High;
  - Low = min M1 Low;
  - Close = last M1 Close.

Новая M1 база не должна молча перезаписывать старую canonical raw base:
сначала строится отдельно и проходит validator/reconciliation.
