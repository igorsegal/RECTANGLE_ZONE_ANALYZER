#pragma once
// ============================================================================
// wf_segmenter.h — Деление истории на сегменты (Блок 05)
// ============================================================================
// Одна ответственность: деление массива касаний на N равных сегментов
// по времени и определение train/OOS окон.
// ============================================================================

#include <vector>
#include <cstdint>
#include "discovery/touches/touch_types.h"

namespace rza {

// ============================================================================
// Один сегмент
// ============================================================================
struct WFSegment {
    int      index = -1;
    int64_t  start_time = 0;
    int64_t  end_time = 0;

    // Касания в этом сегменте
    std::vector<const TouchRecord*> touches;

    int touch_count() const { return static_cast<int>(touches.size()); }
};

// ============================================================================
// Результат сегментации
// ============================================================================
struct WFSegmentation {
    bool                    success = false;
    std::string             error_message;

    std::vector<WFSegment>  segments;

    int      total_segments = 0;
    int      training_segments = 0;
    int      oos_segments = 0;

    // Индексы train/OOS сегментов
    std::vector<int> train_indexes;
    std::vector<int> oos_indexes;
};

// ============================================================================
// Класс для сегментации
// ============================================================================
class WFSegmenter {
public:
    // Разделить касания на N сегментов
    static WFSegmentation segment(
        const std::vector<TouchRecord>& touches,
        int number_of_segments,
        int training_segments);

private:
    // Определить границы сегментов по времени
    static void compute_boundaries(
        const std::vector<TouchRecord>& touches,
        int number_of_segments,
        std::vector<int64_t>& boundaries);

    // Распределить касания по сегментам
    static void distribute_touches(
        const std::vector<TouchRecord>& touches,
        const std::vector<int64_t>& boundaries,
        std::vector<WFSegment>& segments);
};

} // namespace rza