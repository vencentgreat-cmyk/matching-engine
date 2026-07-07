#pragma once
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
#include <cfloat>

struct SymbolStats {
    double openPrice = 0.0;     // 第一笔成交价
    double closePrice = 0.0;    // 最近一笔成交价
    double highPrice = 0.0;     // 最高成交价
    double lowPrice = DBL_MAX;  // 最低成交价
    int totalVolume = 0;        // 总成交量
    int tradeCount = 0;         // 成交笔数
    double totalValue = 0.0;    // 总成交金额（用于算 VWAP）

    void recordTrade(double price, int quantity) {
        if (tradeCount == 0) {
            openPrice = price;
        }
        closePrice = price;
        if (price > highPrice) highPrice = price;
        if (price < lowPrice) lowPrice = price;
        totalVolume += quantity;
        totalValue += price * quantity;
        tradeCount++;
    }

    double vwap() const {
        if (totalVolume == 0) return 0.0;
        return totalValue / totalVolume;
    }
};

class SessionStats {
public:
    void recordTrade(const std::string& symbol, double price, int quantity) {
        stats_[symbol].recordTrade(price, quantity);
    }

    void print(const std::string& symbol = "") const {
        if (symbol.empty()) {
            if (stats_.empty()) {
                std::cout << "No trades this session." << std::endl;
                return;
            }
            for (const auto& [sym, s] : stats_) {
                printSymbol(sym, s);
            }
        } else {
            auto it = stats_.find(symbol);
            if (it == stats_.end()) {
                std::cout << "No trades for " << symbol << " this session." << std::endl;
                return;
            }
            printSymbol(symbol, it->second);
        }
    }

    void reset() {
        stats_.clear();
    }

    bool empty() const {
        return stats_.empty();
    }

private:
    std::map<std::string, SymbolStats> stats_;

    void printSymbol(const std::string& symbol, const SymbolStats& s) const {
        std::cout << "\n--- " << symbol << " Session Summary ---\n"
                  << std::fixed << std::setprecision(2)
                  << "  Open:       " << s.openPrice << "\n"
                  << "  High:       " << s.highPrice << "\n"
                  << "  Low:        " << s.lowPrice << "\n"
                  << "  Close:      " << s.closePrice << "\n"
                  << "  VWAP:       " << s.vwap() << "\n"
                  << "  Volume:     " << s.totalVolume << "\n"
                  << "  Trades:     " << s.tradeCount << "\n"
                  << std::endl;
    }
};
