import os
import pandas as pd

# Путь к вашим результатам
ranking_file = "results/pattern_ranking.csv"
summary_file = "results/wf_summary.csv"


def check_profitability():
    print("=== АУДИТ ПРИБЫЛЬНОСТИ И ТОРГОВЫХ ПАР ===")

    if os.path.exists(ranking_file):
        df = pd.read_csv(ranking_file)
        print("\n🏆 ТОП ПАРЫ И ПАТТЕРНЫ ПО РЕЙТИНГУ:")
        # Выводим лучшие пары по Профит-Фактору / Профиту
        columns_to_show = [col for col in df.columns if col in [
            "symbol", "ticker", "profit_factor", "win_rate", "net_profit", "recovery_factor"]]
        print(df[columns_to_show].head(5).to_string(index=False))
    else:
        print(f"\n⚠️ Файл {ranking_file} не найден. Запустите сначала generate_report.py")

    if os.path.exists(summary_file):
        df_sum = pd.read_csv(summary_file)
        print("\n📊 СВОДНАЯ СТАТИСТИКА ПО ЭТАПАМ (WALK-FORWARD):")
        if "profit_factor" in df_sum.columns:
            avg_pf = df_sum["profit_factor"].mean()
            print(f"-> Средний Профит-Фактор системы на тестах: {avg_pf:.2f}")
            if avg_pf > 1.2:
                print("✅ Математика алгоритма ПРИБЫЛЬНА на незнакомых данных!")
            else:
                print(
                    "⚠️ Внимание: Система на грани окупаемости комиссий. Нужен пересчет параметров.")


if __name__ == "__main__":
    check_profitability()
