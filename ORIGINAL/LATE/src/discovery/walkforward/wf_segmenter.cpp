// ============================================================================
// wf_segmenter.cpp — Реализация сегментации
// ============================================================================

#include "discovery/walkforward/wf_segmenter.h"
#include <algorithm>

namespace rza {

// ============================================================================
// Вычисление границ сегментов
// ============================================================================
void WFSegmenter::compute_boundaries(
    const std::vector<TouchRecord>& touches,
    int number_of_segments,
    std::vector<int64_t>& boundaries)
{
    boundaries.clear();
    if (touches.empty() || number_of_segments <= 0) return;

    // Находим min/max время касаний
    int64_t min_time = touches[0].touch_bar_time;
    int64_t max_time = touches[0].touch_bar_time;
    for (const auto& t : touches) {
        if (t.touch_bar_time < min_time) min_time = t.touch_bar_time;
        if (t.touch_bar_time > max_time) max_time = t.touch_bar_time;
    }

    // Добавляем начало
    boundaries.push_back(min_time);

    // Вычисляем равные интервалы
    double total_duration = static_cast<double>(max_time - min_time);
    double segment_duration = total_duration / number_of_segments;

    for (int i = 1; i < number_of_segments; ++i) {
        int64_t boundary = min_time + static_cast<int64_t>(i * segment_duration);
        boundaries.push_back(boundary);
    }

    // Добавляем конец
    boundaries.push_back(max_time + 1);
}

// ============================================================================
// Распределение касаний по сегментам
// ============================================================================
void WFSegmenter::distribute_touches(
    const std::vector<TouchRecord>& touches,
    const std::vector<int64_t>& boundaries,
    std::vector<WFSegment>& segments)
{
    int num_segments = static_cast<int>(segments.size());

    for (const auto& touch : touches) {
        int64_t t = touch.touch_bar_time;

        // Находим сегмент, в который попадает это касание
        int seg_idx = -1;
        for (int i = 0; i < num_segments; ++i) {
            if (t >= segments[i].start_time && t < segments[i].end_time) {
                seg_idx = i;
                break;
            }
        }

        if (seg_idx >= 0) {
            segments[seg_idx].touches.push_back(&touch);
        }
    }
}

// ============================================================================
// Главная функция сегментации
// ============================================================================
WFSegmentation WFSegmenter::segment(
    const std::vector<TouchRecord>& touches,
    int number_of_segments,
    int training_segments)
{
    WFSegmentation result;
    result.success = false;

    if (touches.empty()) {
        result.error_message = "No touches to segment";
        return result;
    }

    if (number_of_segments <= 0 || training_segments <= 0 ||
        training_segments >= number_of_segments) {
        result.error_message = "Invalid segment configuration: number=" +
                               std::to_string(number_of_segments) +
                               " training=" + std::to_string(training_segments);
        return result;
    }

    // Вычисляем границы
    std::vector<int64_t> boundaries;
    compute_boundaries(touches, number_of_segments, boundaries);

    if (boundaries.size() != static_cast<size_t>(number_of_segments + 1)) {
        result.error_message = "Failed to compute boundaries";
        return result;
    }

    // Создаём сегменты
    result.segments.resize(number_of_segments);
    for (int i = 0; i < number_of_segments; ++i) {
        result.segments[i].index = i;
        result.segments[i].start_time = boundaries[i];
        result.segments[i].end_time = boundaries[i + 1];
    }

    // Распределяем касания
    distribute_touches(touches, boundaries, result.segments);

    // Заполняем train/OOS индексы
    result.total_segments = number_of_segments;
    result.training_segments = training_segments;
    result.oos_segments = number_of_segments - training_segments;

    for (int i = 0; i < training_segments; ++i) {
        result.train_indexes.push_back(i);
    }
    for (int i = training_segments; i < number_of_segments; ++i) {
        result.oos_indexes.push_back(i);
    }

    result.success = true;
    return result;
}

} // namespace rza