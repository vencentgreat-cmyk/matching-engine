#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstddef>
#include <string>

class LatencyStats {
public:
    explicit LatencyStats(size_t expected_samples = 0) {
        if (expected_samples > 0) {
            samples_.reserve(expected_samples);
        }
    }

    void record(double ns) {
        samples_.push_back(ns);
    }

    void report(const std::string& label = "Latency") {
        if (samples_.empty()) {
            std::cout << label << ": no samples\n";
            return;
        }

        std::sort(samples_.begin(), samples_.end());

        auto pct = [&](double p) {
            size_t idx = static_cast<size_t>(samples_.size() * p);
            if (idx >= samples_.size()) idx = samples_.size() - 1;
            return samples_[idx];
        };

        std::cout << "\n=== " << label << " (n=" << samples_.size() << ") ===\n";
        std::cout << std::fixed << std::setprecision(0);
        std::cout << "  p50:   " << pct(0.50)   << " ns\n";
        std::cout << "  p90:   " << pct(0.90)   << " ns\n";
        std::cout << "  p99:   " << pct(0.99)   << " ns\n";
        std::cout << "  p99.9: " << pct(0.999)  << " ns\n";
        std::cout << "  max:   " << samples_.back() << " ns\n";
    }

    size_t size() const { return samples_.size(); }

private:
    std::vector<double> samples_;
};
