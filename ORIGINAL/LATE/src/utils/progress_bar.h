#pragma once
// ============================================================================
// progress_bar.h — Консольный прогресс-бар
// ============================================================================
// Одна ответственность: отображение прогресса в консоли.
// ============================================================================

#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <memory>
#include <vector>

namespace rza {

class ProgressBar {
public:
    ProgressBar(int total_work, const std::string& title = "Progress")
        : total_work_(total_work), title_(title), current_(0), running_(true) {
        render_thread_ = std::thread(&ProgressBar::render_loop, this);
    }

    ~ProgressBar() {
        stop();
    }

    void advance(int delta = 1) {
        current_.fetch_add(delta, std::memory_order_relaxed);
    }

    void set(int value) {
        current_.store(value, std::memory_order_relaxed);
    }

    void stop() {
        if (running_.exchange(false)) {
            if (render_thread_.joinable()) {
                render_thread_.join();
            }
            render(true);
            std::cout << "\n";
        }
    }

    int current() const { return current_.load(std::memory_order_relaxed); }
    int total() const { return total_work_; }

private:
    void render_loop() {
        while (running_.load(std::memory_order_relaxed)) {
            render(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void render(bool final_render) {
        int cur = current_.load(std::memory_order_relaxed);
        int tot = total_work_;
        if (tot <= 0) tot = 1;

        int pct = static_cast<int>((cur * 100LL) / tot);
        if (pct > 100) pct = 100;

        const int bar_width = 40;
        int pos = (bar_width * cur) / tot;
        if (pos > bar_width) pos = bar_width;

        std::lock_guard<std::mutex> lock(console_mutex_);
        std::cout << "\r" << title_ << " [";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::setw(3) << pct << "%  ("
                  << cur << " / " << tot << ")";

        if (final_render) {
            std::cout << "  DONE";
        }
        std::cout.flush();
    }

    int total_work_;
    std::string title_;
    std::atomic<int> current_;
    std::atomic<bool> running_;
    std::thread render_thread_;
    std::mutex console_mutex_;
};

class MultiProgressBar {
public:
    MultiProgressBar(int num_tasks, const std::string& title = "Tasks")
        : num_tasks_(num_tasks), title_(title), running_(true), done_count_(0) {
        task_progress_ = std::make_unique<std::atomic<long long>[]>(num_tasks);
        task_names_.resize(num_tasks);
        for (int i = 0; i < num_tasks; ++i) {
            task_progress_[i].store(0, std::memory_order_relaxed);
        }
        render_thread_ = std::thread(&MultiProgressBar::render_loop, this);
    }

    ~MultiProgressBar() {
        stop();
    }

    void set_task_name(int task_idx, const std::string& name) {
        std::lock_guard<std::mutex> lock(console_mutex_);
        if (task_idx >= 0 && task_idx < num_tasks_) {
            task_names_[task_idx] = name;
        }
    }

    void set_task_progress(int task_idx, int current, int total) {
        if (task_idx >= 0 && task_idx < num_tasks_) {
            long long packed = (static_cast<long long>(total) << 32) |
                               (static_cast<long long>(current) & 0xFFFFFFFFLL);
            task_progress_[task_idx].store(packed, std::memory_order_relaxed);
        }
    }

    void mark_task_done(int task_idx) {
        if (task_idx >= 0 && task_idx < num_tasks_) {
            long long packed = task_progress_[task_idx].load(std::memory_order_relaxed);
            int total = static_cast<int>(packed >> 32);
            long long new_packed = (static_cast<long long>(total) << 32) |
                                   (static_cast<long long>(total) & 0xFFFFFFFFLL);
            task_progress_[task_idx].store(new_packed, std::memory_order_relaxed);
            done_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void stop() {
        if (running_.exchange(false)) {
            if (render_thread_.joinable()) {
                render_thread_.join();
            }
            std::cout << "\n";
        }
    }

private:
    void render_loop() {
        while (running_.load(std::memory_order_relaxed)) {
            render();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    void render() {
        std::lock_guard<std::mutex> lock(console_mutex_);

        int done = done_count_.load(std::memory_order_relaxed);
        std::cout << "\r" << title_ << ": " << done << "/" << num_tasks_ << " tasks done    \n";

        for (int i = 0; i < num_tasks_; ++i) {
            long long packed = task_progress_[i].load(std::memory_order_relaxed);
            int total = static_cast<int>(packed >> 32);
            int current = static_cast<int>(packed & 0xFFFFFFFFLL);

            std::string name = task_names_[i].empty() ? ("Task " + std::to_string(i))
                                                      : task_names_[i];

            if (total <= 0) {
                std::cout << "  [" << i << "] " << name << ": waiting...\n";
                continue;
            }

            int pct = (current * 100) / total;
            const int bar_width = 30;
            int pos = (bar_width * current) / total;

            std::cout << "  [" << i << "] " << std::left << std::setw(20) << name.substr(0, 20)
                      << " [";
            for (int j = 0; j < bar_width; ++j) {
                if (j < pos) std::cout << "=";
                else if (j == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << std::setw(3) << pct << "%\n";
        }
        std::cout.flush();
    }

    int num_tasks_;
    std::string title_;
    std::atomic<bool> running_;
    std::unique_ptr<std::atomic<long long>[]> task_progress_;
    std::vector<std::string> task_names_;
    std::atomic<int> done_count_;
    std::thread render_thread_;
    std::mutex console_mutex_;
};

} // namespace rza