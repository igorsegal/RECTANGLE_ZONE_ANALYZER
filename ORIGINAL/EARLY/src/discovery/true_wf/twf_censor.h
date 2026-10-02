#pragma once
// ============================================================================
// twf_censor.h — Цензурация будущего для True Walk-Forward (Блок 06)
// ============================================================================
// Одна ответственность: фильтрация касаний так, чтобы в оптимизационном
// окне не было информации из будущего.
// ============================================================================

#include <vector>
#include "discovery/touches/touch_types.h"
#include "discovery/true_wf/twf_window.h"

namespace rza {

class TWFCensor {
public:
    // Отфильтровать касания для оптимизационного окна
    // (только касания внутри opt-окна)
    static std::vector<TouchRecord> censor_opt(
        const std::vector<TouchRecord>& all_touches,
        const TWFWindow& window);

    // Отфильтровать касания для валидационного окна
    // (только касания внутри val-окна)
    static std::vector<TouchRecord> censor_val(
        const std::vector<TouchRecord>& all_touches,
        const TWFWindow& window);

    // Отфильтровать касания для всего окна (opt + val)
    static std::vector<TouchRecord> censor_window(
        const std::vector<TouchRecord>& all_touches,
        const TWFWindow& window);

private:
    // Проверка, попадает ли касание в диапазон времени
    static bool in_range(const TouchRecord& touch,
                         int64_t start_time, int64_t end_time);
};

} // namespace rza