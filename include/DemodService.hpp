/**
 * @file DemodService.hpp
 * @brief AMQP-driven demodulation service.
 */
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

/**
 * @brief Represents a pending demodulation task queued from an AMQP message.
 */
struct PendingDemod {
    std::string modulation;      ///< Detected modulation string.
    double      center_freq_hz;  ///< Centre frequency in Hz.
    double      bandwidth_hz;    ///< Signal bandwidth in Hz.
    double      symbol_rate_sps; ///< Symbol rate hint in sps (0 = unknown).
    float       confidence;      ///< Classifier confidence [0, 1].
    int64_t     timestamp_ms;    ///< Detection timestamp in milliseconds.
};

/**
 * @class DemodService
 * @brief Subscribes to AMQP analysis results, deduplicates, and dispatches
 *        demodulation tasks to DemodRouter.
 *
 * Runs two internal threads:
 *  - A subscription thread listening to the AMQP analysis topic.
 *  - A worker thread consuming the pending-task queue via DemodRouter.
 *
 * @note Call start() once after construction, stop() before destruction.
 */
class DemodService {
public:
    /**
     * @brief Construct a DemodService.
     *
     * @param cfg Application configuration.
     */
    explicit DemodService(const AppConfig& cfg);

    /// @brief Destructor — calls stop() if still running.
    ~DemodService();

    /**
     * @brief Start the subscription and worker threads.
     *
     * @note Must be called exactly once before any tasks are processed.
     */
    void start();

    /**
     * @brief Stop both internal threads gracefully.
     *
     * Signals threads to exit and joins them.  Safe to call multiple times.
     */
    void stop();

private:
    /// @brief AMQP subscription loop (runs in sub_thread_).
    void subscriptionLoop();

    /// @brief Task dispatch loop (runs in worker_thread_).
    void workerLoop();

    /**
     * @brief Publish a DemodResult to the AMQP demod topic.
     * @param r Completed demodulation result.
     */
    void publishResult(const DemodResult& r);

    /**
     * @brief Called from the subscription thread when a new analysis result arrives.
     * @param p Pending demodulation task parsed from the AMQP message.
     */
    void onAnalysisResult(const PendingDemod& p);

    AppConfig   cfg_;       ///< Application configuration.
    IqFetcher   fetcher_;   ///< IQ sample fetcher.
    DemodRouter router_;    ///< Dispatch router.

    std::atomic<bool> running_{false}; ///< True while threads are active.
    std::thread       sub_thread_;     ///< AMQP subscription thread.
    std::thread       worker_thread_;  ///< Task worker thread.

    std::mutex               q_mu_; ///< Protects queue_.
    std::condition_variable  q_cv_; ///< Signals worker when tasks arrive.
    std::queue<PendingDemod> queue_; ///< Pending demodulation tasks.

    /// @brief Frequency deduplication map: freq_bucket → last_demod_ms.
    std::unordered_map<int64_t, int64_t> recent_demod_;

    std::shared_ptr<ServiceAmqpHandler> amqp_handler_; ///< AMQP session handler.
};

} // namespace demod
