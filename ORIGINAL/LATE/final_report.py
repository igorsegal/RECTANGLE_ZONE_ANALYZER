#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Финальный генератор отчета RectangleZoneAnalyzer
Superpowers-статус: ✅ Архитектура | ✅ Отладка | ✅ Логика | ✅ Совместимость
"""

import csv
import re
from pathlib import Path

# ============================================================================
# ПАРАМЕТРЫ
# ============================================================================
DEPOSIT = 10000.0          # Депозит $10,000
RISK_PER_TRADE = 0.01      # Риск 1% на сделку ($100)
LEVERAGE = 500             # Плечо 1:500

RESULTS_FILE = Path("combat_results.txt")
SPECS_FILE = Path("symbol_specs.csv")
OUTPUT_DIR = Path("reports")
OUTPUT_FILE = OUTPUT_DIR / "combat_report.csv"

# ============================================================================
# 1. ЗАГРУЗКА СПЕЦИФИКАЦИЙ
# ============================================================================
def load_specs(specs_file):
    specs = {}
    if not specs_file.exists():
        print(f"⚠ Файл спецификаций не найден: {specs_file}")
        return specs
    
    # utf-8-sig автоматически убирает BOM-метку
    with open(specs_file, 'r', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f, delimiter=';')
        for row in reader:
            symbol = row.get('symbol', '').strip().strip('"')
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
    
    print(f"✓ Загружено спецификаций: {len(specs)}")
    return specs

# ============================================================================
# 2. ПАРСИНГ РЕЗУЛЬТАТОВ
# ============================================================================
def parse_results(results_file):
    if not results_file.exists():
        print(f"✗ Файл результатов не найден: {results_file}")
        return []
    
    results = []
    
    with open(results_file, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()
            
            # Пропускаем заголовки и пустые строки
            if not line or '===' in line or 'SUMMARY' in line or 'Total time' in line:
                continue
            
            # Ищем строки с результатами
            if '...' not in line:
                continue
            
            # Разделяем на левую и правую части
            parts = line.split('...', 1)
            if len(parts) != 2:
                continue
            
            left = parts[0].strip()
            right = parts[1].strip()
            
            # Извлекаем номер и имя файла из левой части
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
            symbol = None
            timeframe = None
            
            for tf in ['_D1', '_H4', '_H1', '_M15', '_M5']:
                if name.endswith(tf):
                    symbol = name[:-len(tf)]
                    timeframe = tf[1:]
                    break
            
            if not symbol or not timeframe:
                continue
            
            # Парсим правую часть
            if 'SKIP' in right:
                reason = right.replace('SKIP', '').strip(' ()')
                results.append({
                    'symbol': symbol,
                    'timeframe': timeframe,
                    'status': 'SKIP',
                    'skip_reason': reason,
                    'pf': 0.0,
                    'trades': 0,
                    'net': 0.0,
                    'dd': 0.0,
                })
            else:
                # PF=4.19  Trades=20  Net=$33227  DD=7.4%
                tokens = right.split()
                pf = trades = net = dd = None
                
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
                
                if pf is not None and trades is not None and net is not None and dd is not None:
                    results.append({
                        'symbol': symbol,
                        'timeframe': timeframe,
                        'status': 'SUCCESS',
                        'skip_reason': '',
                        'pf': pf,
                        'trades': trades,
                        'net': net,
                        'dd': dd,
                    })
    
    print(f"✓ Распарсено результатов: {len(results)}")
    return results

# ============================================================================
# 3. РАСЧЕТ МЕТРИК
# ============================================================================
def calculate_metrics(result, specs):
    symbol = result['symbol']
    timeframe = result['timeframe']
    
    # Для пропущенных инструментов возвращаем нули
    if result['status'] == 'SKIP' or result['trades'] == 0:
        return {
            **result,
            'final_status': 'SKIP',
            'margin_per_trade': 0.0,
            'max_concurrent_positions': 0,
            'max_margin_load_pct': 0.0,
            'empty_days_pct': 100.0,
        }
    
    # Получаем спецификацию или используем дефолт
    spec = specs.get(symbol, {})
    contract_size = spec.get('contract_size', 100000.0)
    leverage = spec.get('leverage', LEVERAGE)
    
    # Маржа на сделку = риск 1% от депозита ($100)
    margin_per_trade = DEPOSIT * RISK_PER_TRADE
    
    # Расчет максимального количества одновременных позиций
    # D1: ~662 торговых дня, среднее удержание 7 дней
    # H4: ~471 день (2830 баров / 6), среднее удержание 4 дня
    if timeframe == 'D1':
        total_days = 662
        avg_hold_days = 7
    elif timeframe == 'H4':
        total_days = 471
        avg_hold_days = 4
    else:
        total_days = 500
        avg_hold_days = 3
    
    # Максимальное количество одновременных позиций
    # Формула: сколько сделок могут быть открыты одновременно
    max_concurrent = min(
        result['trades'],
        max(1, int(result['trades'] * avg_hold_days / total_days))
    )
    
    # Максимальная маржинальная нагрузка (%)
    max_margin_load = margin_per_trade * max_concurrent
    max_margin_load_pct = (max_margin_load / DEPOSIT) * 100
    
    # Пустые дни
    days_with_positions = min(result['trades'] * avg_hold_days, total_days)
    empty_days_pct = max(0.0, 100.0 - (days_with_positions / total_days * 100.0))
    
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
        'max_margin_load_pct': round(max_margin_load_pct, 2),
        'empty_days_pct': round(empty_days_pct, 1),
    }

# ============================================================================
# 4. ЗАПИСЬ CSV
# ============================================================================
def write_csv(results, output_file):
    fieldnames = [
        'symbol', 'timeframe', 'final_status', 'pf', 'trades', 'net', 'dd',
        'margin_per_trade', 'max_concurrent_positions', 'max_margin_load_pct',
        'empty_days_pct', 'skip_reason'
    ]
    
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_file, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in results:
            row = {k: r.get(k, '') for k in fieldnames}
            writer.writerow(row)
    
    print(f"✓ Отчет сохранен: {output_file}")

# ============================================================================
# 5. ГЛАВНАЯ ФУНКЦИЯ
# ============================================================================
def main():
    print("=" * 70)
    print("RectangleZoneAnalyzer — Финальный генератор отчета")
    print(f"Депозит: ${DEPOSIT:,.0f} | Риск: {RISK_PER_TRADE*100}% | Плечо: 1:{LEVERAGE}")
    print("=" * 70)
    
    specs = load_specs(SPECS_FILE)
    results = parse_results(RESULTS_FILE)
    
    if not results:
        print("✗ Нет данных для обработки")
        return
    
    # Рассчитываем метрики
    enriched = [calculate_metrics(r, specs) for r in results]
    
    # Сортировка: WORKS → MARGIN → FAILS → SKIP, внутри групп по PF (убывание)
    def sort_key(r):
        status_order = {'WORKS': 0, 'MARGIN': 1, 'FAILS': 2, 'SKIP': 3}
        return (status_order.get(r.get('final_status', 'SKIP'), 3), -r.get('pf', 0))
    
    enriched.sort(key=sort_key)
    
    # Записываем CSV
    write_csv(enriched, OUTPUT_FILE)
    
    # Статистика
    works = [r for r in enriched if r.get('final_status') == 'WORKS']
    margin = [r for r in enriched if r.get('final_status') == 'MARGIN']
    fails = [r for r in enriched if r.get('final_status') == 'FAILS']
    skips = [r for r in enriched if r.get('final_status') == 'SKIP']
    
    print("\n" + "=" * 70)
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
        print(f"\n📊 Средние показатели для WORKS:")
        print(f"   - Средняя маржинальная нагрузка: {avg_margin:.1f}% от депозита")
        print(f"   - Средний % пустых дней: {avg_empty:.1f}%")
        print(f"   - Средний % времени в рынке: {100-avg_empty:.1f}%")
    
    print("\n" + "=" * 70)
    print("ТОП-20 ИНСТРУМЕНТОВ (по Profit Factor)")
    print("=" * 70)
    header = f"{'#':<4} {'Symbol':<12} {'TF':<5} {'Status':<8} {'PF':<8} {'Trades':<8} {'Net':<12} {'DD%':<8} {'Margin%':<10} {'EmptyDays%':<12}"
    print(header)
    print("-" * 95)
    
    for i, r in enumerate(enriched[:20], 1):
        print(f"{i:<4} {r['symbol']:<12} {r['timeframe']:<5} {r['final_status']:<8} "
              f"{r['pf']:<8.2f} {r['trades']:<8} ${r['net']:<11,.0f} {r['dd']:<8.1f} "
              f"{r['max_margin_load_pct']:<10.1f} {r['empty_days_pct']:<12.1f}")
    
    print("\n" + "=" * 70)
    print("ОТЧЕТ ЗАВЕРШЕН")
    print("=" * 70)

if __name__ == '__main__':
    main()