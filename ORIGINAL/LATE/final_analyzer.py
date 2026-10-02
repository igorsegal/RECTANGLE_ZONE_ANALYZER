import csv
import re
from pathlib import Path

DEPOSIT = 10000.0
RISK_PER_TRADE = 0.01
LEVERAGE = 500

# Имена файлов точно как в ваших данных
RESULTS_FILE = Path("Pasted_Text_1787771839055.txt")
SPECS_FILE = Path("symbol_specs.csv")
OUTPUT_DIR = Path("reports")
OUTPUT_FILE = OUTPUT_DIR / "combat_report.csv"

def load_specs(specs_file):
    specs = {}
    if not specs_file.exists():
        print(f"WARNING: Файл спецификаций не найден: {specs_file}")
        return specs
    
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
    print(f"OK: Загружено спецификаций: {len(specs)}")
    return specs

def parse_results(results_file):
    if not results_file.exists():
        print(f"ERROR: Файл результатов не найден: {results_file}")
        return []
    
    results = []
    with open(results_file, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()
            if '...' not in line:
                continue
            
            parts = line.split('...', 1)
            if len(parts) != 2:
                continue
            
            left = parts[0].strip()
            right = parts[1].strip()
            
            if not left.startswith('[') or ']' not in left:
                continue
            
            filename = left.split(']', 1)[1].strip()
            if not filename.endswith('.bin'):
                continue
            
            name = filename[:-4]
            symbol = None
            timeframe = None
            for tf in ['_D1', '_H4', '_H1', '_M15', '_M5']:
                if name.endswith(tf):
                    symbol = name[:-len(tf)]
                    timeframe = tf[1:]
                    break
            
            if not symbol or not timeframe:
                continue
            
            if 'SKIP' in right:
                reason = right.replace('SKIP', '').strip(' ()')
                results.append({
                    'symbol': symbol, 'timeframe': timeframe, 'status': 'SKIP',
                    'skip_reason': reason, 'pf': 0.0, 'trades': 0, 'net': 0.0, 'dd': 0.0,
                })
            else:
                pf = trades = net = dd = None
                for token in right.split():
                    if token.startswith('PF='):
                        try: pf = float(token[3:])
                        except ValueError: pass
                    elif token.startswith('Trades='):
                        try: trades = int(token[7:])
                        except ValueError: pass
                    elif token.startswith('Net=$'):
                        try: net = float(token[4:].replace(',', ''))
                        except ValueError: pass
                    elif token.startswith('DD=') and token.endswith('%'):
                        try: dd = float(token[3:-1])
                        except ValueError: pass
                
                if pf is not None and trades is not None and net is not None and dd is not None:
                    results.append({
                        'symbol': symbol, 'timeframe': timeframe, 'status': 'SUCCESS',
                        'skip_reason': '', 'pf': pf, 'trades': trades, 'net': net, 'dd': dd,
                    })
    print(f"OK: Распарсено результатов: {len(results)}")
    return results

def calculate_metrics(result, specs):
    symbol = result['symbol']
    timeframe = result['timeframe']
    
    if result['status'] == 'SKIP' or result['trades'] == 0:
        return {
            **result, 'final_status': 'SKIP', 'margin_per_trade': 0.0,
            'max_concurrent_positions': 0, 'max_margin_load': 0.0,
            'max_margin_load_pct': 0.0, 'empty_days_pct': 100.0,
            'empty_weeks_pct': 100.0, 'empty_months_pct': 100.0, 'time_in_market_pct': 0.0,
        }
    
    spec = specs.get(symbol, {})
    # ИСПРАВЛЕНО: корректно закрыты кавычки и скобки
    contract_size = float(spec.get('contract_size', 100000))
    margin_initial = float(spec.get('margin_initial', 0))
    
    margin_per_trade = DEPOSIT * RISK_PER_TRADE
    
    if timeframe == 'D1':
        max_concurrent = max(1, min(3, result['trades'] // 20))
        total_days = 662
        avg_hold_days = 7
    elif timeframe == 'H4':
        max_concurrent = max(1, min(5, result['trades'] // 40))
        total_days = 471
        avg_hold_days = 4
    else:
        max_concurrent = 1
        total_days = 500
        avg_hold_days = 2
    
    max_margin_load = margin_per_trade * max_concurrent
    max_margin_load_pct = (max_margin_load / DEPOSIT) * 100
    
    days_with_positions = min(result['trades'] * avg_hold_days, total_days)
    empty_days_pct = max(0.0, 100.0 - (days_with_positions / total_days * 100.0)) if total_days > 0 else 100.0
    empty_weeks_pct = max(0.0, 100.0 - (result['trades'] / (total_days / 7.0) * 100.0)) if total_days > 0 else 100.0
    empty_months_pct = max(0.0, 100.0 - (result['trades'] / (total_days / 30.0) * 100.0)) if total_days > 0 else 100.0
    time_in_market_pct = 100.0 - empty_days_pct
    
    if result['pf'] >= 1.5:
        final_status = 'WORKS'
    elif result['pf'] >= 1.0:
        final_status = 'MARGIN'
    else:
        final_status = 'FAILS'
    
    return {
        **result, 'final_status': final_status,
        'margin_per_trade': round(margin_per_trade, 2),
        'max_concurrent_positions': max_concurrent,
        'max_margin_load': round(max_margin_load, 2),
        'max_margin_load_pct': round(max_margin_load_pct, 2),
        'empty_days_pct': round(empty_days_pct, 1),
        'empty_weeks_pct': round(empty_weeks_pct, 1),
        'empty_months_pct': round(empty_months_pct, 1),
        'time_in_market_pct': round(time_in_market_pct, 1),
    }

def main():
    print("=" * 70)
    print("RectangleZoneAnalyzer - Финальный генератор отчета")
    print(f"Депозит: ${DEPOSIT:,.0f} | Риск: {RISK_PER_TRADE*100}% | Плечо: 1:{LEVERAGE}")
    print("=" * 70)
    
    specs = load_specs(SPECS_FILE)
    results = parse_results(RESULTS_FILE)
    
    if not results:
        print("ERROR: Нет данных для обработки")
        return
    
    enriched = [calculate_metrics(r, specs) for r in results]
    
    def sort_key(r):
        status_order = {'WORKS': 0, 'MARGIN': 1, 'FAILS': 2, 'SKIP': 3}
        return (status_order.get(r.get('final_status', 'SKIP'), 3), -r.get('pf', 0))
    
    enriched.sort(key=sort_key)
    
    OUTPUT_DIR.mkdir(exist_ok=True)
    
    fieldnames = [
        'symbol', 'timeframe', 'final_status', 'pf', 'trades', 'net', 'dd',
        'margin_per_trade', 'max_concurrent_positions', 'max_margin_load', 'max_margin_load_pct',
        'empty_days_pct', 'empty_weeks_pct', 'empty_months_pct', 'time_in_market_pct', 'skip_reason'
    ]
    
    with open(OUTPUT_FILE, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in enriched:
            row = {k: r.get(k, '') for k in fieldnames}
            writer.writerow(row)
    
    print(f"\nOK: Отчет сохранен: {OUTPUT_FILE.resolve()}")
    
    works = [r for r in enriched if r.get('final_status') == 'WORKS']
    margin = [r for r in enriched if r.get('final_status') == 'MARGIN']
    fails = [r for r in enriched if r.get('final_status') == 'FAILS']
    skips = [r for r in enriched if r.get('final_status') == 'SKIP']
    
    print("\n" + "=" * 70)
    print("СВОДНАЯ СТАТИСТИКА")
    print("=" * 70)
    print(f"Всего инструментов: {len(enriched)}")
    print(f"  WORKS  (PF>=1.5):  {len(works)}")
    print(f"  MARGIN (1.0-1.5):  {len(margin)}")
    print(f"  FAILS  (PF<1.0):   {len(fails)}")
    print(f"  SKIP:              {len(skips)}")
    
    if works:
        avg_margin = sum(r['max_margin_load_pct'] for r in works) / len(works)
        avg_empty = sum(r['empty_days_pct'] for r in works) / len(works)
        print(f"\nСредние показатели для WORKS:")
        print(f"  - Маржинальная нагрузка: {avg_margin:.1f}% от депозита")
        print(f"  - Пустых дней: {avg_empty:.1f}%")
        print(f"  - Времени в рынке: {100-avg_empty:.1f}%")
    
    print("\n" + "=" * 70)
    print("ТОП-20 ИНСТРУМЕНТОВ (по Profit Factor)")
    print("=" * 70)
    header = f"{'#':<4} {'Symbol':<12} {'TF':<5} {'Status':<8} {'PF':<8} {'Trades':<8} {'Net':<12} {'DD%':<8} {'Margin%':<10} {'EmptyDays%':<12}"
    print(header)
    print("-" * 105)
    
    for i, r in enumerate(enriched[:20], 1):
        print(f"{i:<4} {r['symbol']:<12} {r['timeframe']:<5} {r['final_status']:<8} "
              f"{r['pf']:<8.2f} {r['trades']:<8} ${r['net']:<11,.0f} {r['dd']:<8.1f} "
              f"{r['max_margin_load_pct']:<10.1f} {r['empty_days_pct']:<12.1f}")
    
    print("\n" + "=" * 70)
    print("ОТЧЕТ ЗАВЕРШЕН")
    print("=" * 70)

if __name__ == '__main__':
    main()