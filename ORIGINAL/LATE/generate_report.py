#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор отчета RectangleZoneAnalyzer
Депозит: $10,000 | Плечо: 1:500 | Риск: 1% на сделку
"""

import csv
import re
import os
from pathlib import Path
from datetime import datetime

# ============================================================================
# ПАРАМЕТРЫ
# ============================================================================
DEPOSIT = 10000.0          # Депозит $10,000
RISK_PER_TRADE = 0.01      # Риск 1% на сделку ($100 маржи на сделку)
LEVERAGE = 500             # Плечо 1:500

RESULTS_FILE = Path("Pasted_Text_1787771839055.txt")
SPECS_FILE = Path("symbol_specs.csv")
OUTPUT_DIR = Path("reports")
OUTPUT_FILE = OUTPUT_DIR / "combat_report.csv"

# ============================================================================
# 1. ЗАГРУЗКА СПЕЦИФИКАЦИЙ
# ============================================================================
def load_specs(specs_file):
    """Загружает спецификации инструментов из CSV."""
    specs = {}
    if not specs_file.exists():
        print(f"WARNING: Файл спецификаций не найден: {specs_file}")
        return specs

    # Используем latin-1 для чтения (никогда не падает)
    try:
        with open(specs_file, 'r', encoding='latin-1') as f:
            reader = csv.DictReader(f, delimiter=';')
            for row in reader:
                symbol = row.get('symbol', '').strip()
                if not symbol:
                    continue
                try:
                    specs[symbol] = {
                        'contract_size': float(row.get('trade_contract_size', 100000)),
                        'margin_initial': float(row.get('margin_initial', 0)),
                        'leverage': int(row.get('account_leverage', 500)),
                    }
                except (ValueError, TypeError):
                    pass
        print(f"OK: Загружено спецификаций: {len(specs)}")
    except Exception as e:
        print(f"ERROR: Ошибка чтения файла спецификаций: {e}")
    
    return specs

# ============================================================================
# 2. ПАРСИНГ РЕЗУЛЬТАТОВ
# ============================================================================
def parse_results(results_file):
    """Парсит файл результатов прогона."""
    if not results_file.exists():
        print(f"ERROR: Файл результатов не найден: {results_file}")
        return []

    results = []
    
    try:
        with open(results_file, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                
                # Пропускаем пустые строки и заголовки
                if not line or '===' in line or 'SUMMARY' in line or 'Total time' in line:
                    continue
                
                # Ищем строки с результатами: [ N/1048] SYMBOL.bin ... PF=X.XX Trades=N Net=$N DD=X.X%
                if '...' not in line:
                    continue
                
                # Разделяем по "..."
                parts = line.split('...', 1)
                if len(parts) != 2:
                    continue
                
                left = parts[0].strip()
                right = parts[1].strip()
                
                # Извлекаем номер и имя файла
                if not left.startswith('['):
                    continue
                
                # Убираем [ и ]
                left = left[1:].strip()
                if ']' not in left:
                    continue
                
                bracket_content, filename = left.split(']', 1)
                filename = filename.strip()
                
                if not filename.endswith('.bin'):
                    continue
                
                # Извлекаем символ и таймфрейм
                name = filename[:-4]  # убираем .bin
                if '_D1' in name:
                    symbol = name.replace('_D1', '')
                    timeframe = 'D1'
                elif '_H4' in name:
                    symbol = name.replace('_H4', '')
                    timeframe = 'H4'
                elif '_H1' in name:
                    symbol = name.replace('_H1', '')
                    timeframe = 'H1'
                elif '_M15' in name:
                    symbol = name.replace('_M15', '')
                    timeframe = 'M15'
                elif '_M5' in name:
                    symbol = name.replace('_M5', '')
                    timeframe = 'M5'
                else:
                    continue
                
                # Парсим правую часть
                if 'SKIP' in right:
                    # SKIP (reason)
                    results.append({
                        'symbol': symbol,
                        'timeframe': timeframe,
                        'status': 'SKIP',
                        'pf': 0.0,
                        'trades': 0,
                        'net': 0.0,
                        'dd': 0.0,
                        'qs': 0.0,
                    })
                else:
                    # PF=4.19  Trades=20  Net=$33227  DD=7.4%
                    tokens = right.split()
                    pf = trades = net = dd = qs = None
                    
                    for token in tokens:
                        if token.startswith('PF='):
                            try:
                                pf = float(token[3:])
                            except ValueError:
                                pass
                        elif token.startswith('Trades='):
                            try:
                                trades = int(token[7:])
                            except ValueError:
                                pass
                        elif token.startswith('Net=$'):
                            try:
                                net = float(token[4:].replace(',', ''))
                            except ValueError:
                                pass
                        elif token.startswith('DD=') and token.endswith('%'):
                            try:
                                dd = float(token[3:-1])
                            except ValueError:
                                pass
                        elif token.startswith('QS='):
                            try:
                                qs = float(token[3:])
                            except ValueError:
                                pass
                    
                    if pf is not None and trades is not None and net is not None and dd is not None:
                        results.append({
                            'symbol': symbol,
                            'timeframe': timeframe,
                            'status': 'SUCCESS',
                            'pf': pf,
                            'trades': trades,
                            'net': net,
                            'dd': dd,
                            'qs': qs if qs is not None else 0.0,
                        })
    except Exception as e:
        print(f"ERROR: Ошибка парсинга результатов: {e}")
    
    print(f"OK: Распарсено результатов: {len(results)}")
    return results

# ============================================================================
# 3. РАСЧЕТ МЕТРИК
# ============================================================================
def calculate_metrics(result, specs):
    """Рассчитывает маржинальную нагрузку, пустые дни и другие метрики."""
    symbol = result['symbol']
    timeframe = result['timeframe']
    
    # Для пропущенных инструментов возвращаем нули
    if result['status'] == 'SKIP' or result['trades'] == 0:
        return {
            **result,
            'final_status': 'SKIP',
            'margin_per_trade': 0.0,
            'max_concurrent_positions': 0,
            'max_margin_load': 0.0,
            'max_margin_load_pct': 0.0,
            'empty_days_pct': 100.0,
            'empty_weeks_pct': 100.0,
            'empty_months_pct': 100.0,
            'time_in_market_pct': 0.0,
        }

    # Получаем спецификации или используем дефолт
    spec = specs.get(symbol, {})
    contract_size = spec.get('contract_size', 100000.0)
    leverage = spec.get('leverage', LEVERAGE)
    
    # Маржа на сделку: фиксированный риск 1% от депозита ($100)
    margin_per_trade = DEPOSIT * RISK_PER_TRADE
    
    # Оценка максимальных одновременных позиций
    # Для D1: обычно 1-3 позиции, для H4: 2-5 позиций, для H1: 3-8 позиций
    if timeframe == 'D1':
        max_concurrent = max(1, min(3, result['trades'] // 20))
        total_days = 662       # ~2.5 года данных
        avg_hold_days = 7      # среднее удержание в днях
    elif timeframe == 'H4':
        max_concurrent = max(1, min(5, result['trades'] // 40))
        total_days = 471       # ~2.5 года данных (в днях)
        avg_hold_days = 4      # среднее удержание в днях
    elif timeframe == 'H1':
        max_concurrent = max(1, min(8, result['trades'] // 60))
        total_days = 471
        avg_hold_days = 2
    else:
        max_concurrent = 1
        total_days = 471
        avg_hold_days = 1

    # Максимальная маржинальная нагрузка
    max_margin_load = margin_per_trade * max_concurrent
    max_margin_load_pct = (max_margin_load / DEPOSIT) * 100.0
    
    # Пустые периоды
    days_with_positions = min(result['trades'] * avg_hold_days, total_days)
    empty_days_pct = max(0.0, 100.0 - (days_with_positions / total_days * 100.0)) if total_days > 0 else 100.0
    empty_weeks_pct = max(0.0, 100.0 - (result['trades'] / (total_days / 7.0) * 100.0)) if total_days > 0 else 100.0
    empty_months_pct = max(0.0, 100.0 - (result['trades'] / (total_days / 30.0) * 100.0)) if total_days > 0 else 100.0
    time_in_market_pct = 100.0 - empty_days_pct

    # Статус
    if result['pf'] >= 1.5:
        final_status = 'WORKS'
    elif result['pf'] >= 1.0:
        final_status = 'MARGIN'
    else:
        final_status = 'FAILS'

    return {
        **result,
        'final_status': final_status,
        'margin_per_trade': round(margin_per_trade, 2),
        'max_concurrent_positions': max_concurrent,
        'max_margin_load': round(max_margin_load, 2),
        'max_margin_load_pct': round(max_margin_load_pct, 2),
        'empty_days_pct': round(empty_days_pct, 1),
        'empty_weeks_pct': round(empty_weeks_pct, 1),
        'empty_months_pct': round(empty_months_pct, 1),
        'time_in_market_pct': round(time_in_market_pct, 1),
    }

# ============================================================================
# 4. ЗАПИСЬ CSV
# ============================================================================
def write_csv(results, output_file):
    """Записывает результаты в CSV."""
    fieldnames = [
        'symbol', 'timeframe', 'final_status', 'pf', 'trades', 'net', 'dd', 'qs',
        'margin_per_trade', 'max_concurrent_positions', 'max_margin_load',
        'max_margin_load_pct', 'empty_days_pct', 'empty_weeks_pct',
        'empty_months_pct', 'time_in_market_pct'
    ]
    
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    try:
        with open(output_file, 'w', newline='', encoding='utf-8-sig') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, delimiter=';')
            writer.writeheader()
            for r in results:
                row = {k: r.get(k, '') for k in fieldnames}
                writer.writerow(row)
        print(f"OK: Отчет сохранен: {output_file}")
    except Exception as e:
        print(f"ERROR: Ошибка записи файла: {e}")

# ============================================================================
# 5. ГЛАВНАЯ ФУНКЦИЯ
# ============================================================================
def main():
    print("=" * 70)
    print("RectangleZoneAnalyzer - Генератор отчета")
    print(f"Депозит: ${DEPOSIT:,.0f} | Плечо: 1:{LEVERAGE} | Риск: {RISK_PER_TRADE*100}%")
    print("=" * 70)

    specs = load_specs(SPECS_FILE)
    results = parse_results(RESULTS_FILE)

    if not results:
        print("ERROR: Нет данных для обработки")
        return

    # Рассчитываем метрики для каждого инструмента
    enriched = [calculate_metrics(r, specs) for r in results]

    # Сортируем: сначала WORKS по PF (убывание), потом остальные
    def sort_key(r):
        status_order = {'WORKS': 0, 'MARGIN': 1, 'FAILS': 2, 'SKIP': 3}
        return (status_order.get(r.get('final_status', 'SKIP'), 3), -r.get('pf', 0))

    enriched.sort(key=sort_key)

    # Записываем в CSV
    write_csv(enriched, OUTPUT_FILE)

    # Статистика
    works = [r for r in enriched if r.get('final_status') == 'WORKS']
    margin = [r for r in enriched if r.get('final_status') == 'MARGIN']
    fails = [r for r in enriched if r.get('final_status') == 'FAILS']
    skips = [r for r in enriched if r.get('final_status') == 'SKIP']

    print()
    print("=" * 70)
    print("СВОДНАЯ СТАТИСТИКА")
    print("=" * 70)
    print(f"Всего инструментов: {len(enriched)}")
    print(f"  ✓ WORKS (PF≥1.5):   {len(works)}")
    print(f"  ~ MARGIN (1.0-1.5): {len(margin)}")
    print(f"  ✗ FAILS (PF<1.0):   {len(fails)}")
    print(f"  ⊘ SKIP:             {len(skips)}")

    if works:
        avg_margin = sum(r['max_margin_load_pct'] for r in works) / len(works)
        avg_empty = sum(r['empty_days_pct'] for r in works) / len(works)
        print()
        print("Средние показатели для WORKS:")
        print(f"  - Маржинальная нагрузка: {avg_margin:.1f}% от депозита")
        print(f"  - Пустых дней: {avg_empty:.1f}%")
        print(f"  - Времени в рынке: {100-avg_empty:.1f}%")

    print()
    print("=" * 70)
    print("ТОП-20 ИНСТРУМЕНТОВ (по Profit Factor)")
    print("=" * 70)
    header = f"{'#':<4} {'Symbol':<12} {'TF':<5} {'Status':<8} {'PF':<8} {'Trades':<8} {'Net':<12} {'DD%':<8} {'Margin%':<10} {'EmptyDays%':<12}"
    print(header)
    print("-" * len(header))
    
    for i, r in enumerate(enriched[:20], 1):
        print(f"{i:<4} {r['symbol']:<12} {r['timeframe']:<5} {r['final_status']:<8} "
              f"{r['pf']:<8.2f} {r['trades']:<8} ${r['net']:<11,.0f} {r['dd']:<8.1f} "
              f"{r['max_margin_load_pct']:<10.1f} {r['empty_days_pct']:<12.1f}")

    print()
    print("=" * 70)
    print("ОТЧЕТ ЗАВЕРШЕН")
    print("=" * 70)

if __name__ == '__main__':
    main()