#pragma once
#include "AppConfig.hpp"
#include "IqFetcher.hpp"
#include "DemodRouter.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

namespace demod {

class ServiceAmqpHandler;

struct PendingDemod {
    std::string modulation;
    double      center_freq_hz;
    double      bandwidth_hz;
    double      symbol_rate_sps;
    float       confidence;
    int64_t     timestamp_ms;
};

class DemodService {
public:
    explicit DemodService(const AppConfig& cfg);
    ~DemodService();

    void start();
    void stop();

private:
    void subscriptionLoop();
    void workerLoop();
    void publishResult(const DemodResult& r);
    void onAnalysisResult(const PendingDemod& p);

    AppConfig  cfg_;
    IqFetcher  fetcher_;
    DemodRouter router_;

    std::atomic<bool> running_{false};
    std::thread       sub_thread_;
    std::thread       worker_thread_;

    std::mutex              q_mu_;
    std::condition_variable q_cv_;
    std::queue<PendingDemod> queue_;

    // Frequency dedup: freq_bucket → last_demod_ms
    std::unordered_map<int64_t, int64_t> recent_demod_;

    std::shared_ptr<ServiceAmqpHandler> amqp_handler_;
};

} // namespace demod
