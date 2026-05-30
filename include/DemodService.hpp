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
#include <vector>
#include "au/units/hertz.hh"

namespace demod {

class ServiceAmqpHandler;

/**
 * @brief Represents a pending demodulation task queued from an AMQP message.
 */
struct PendingDemod {
    std::string              modulation;   ///< Detected modulation string.
    au::QuantityD<au::Hertz> center_freq;  ///< Centre frequency.
    au::QuantityD<au::Hertz> bandwidth;    ///< Signal bandwidth.
    au::QuantityD<au::Hertz> symbol_rate;  ///< Symbol rate hint (0 Hz = unknown).
    float                    confidence;   ///< Classifier confidence [0, 1].
    int64_t                  timestamp_ms; ///< Detection timestamp in milliseconds.
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
     * @brief Called from the subscription thread when a DEMOD_REQUEST message arrives.
     * @param p Demodulation task parsed from the DEMOD_REQUEST message.
     */
    void onDemodRequest(const PendingDemod& p);

    // ── Streaming ─────────────────────────────────────────────────────────────

    /**
     * @brief Holds state for one active streaming session.
     *
     * The stream thread reads @p active to decide whether to continue looping.
     * Setting @p active to false from any thread causes the loop to exit after
     * the current IQ fetch completes.
     */
    struct StreamSession {
        PendingDemod      params;
        std::atomic<bool> active{true};
    };
    using StreamPtr = std::shared_ptr<StreamSession>;

    /**
     * @brief Start continuous demodulation for @p stream_id.
     *
     * Creates a StreamSession and spawns a dedicated thread.  If a session with
     * the same @p stream_id already exists the call is silently ignored.
     */
    void startStream(const std::string& stream_id, const PendingDemod& p);

    /**
     * @brief Signal stream @p stream_id to stop after its current IQ fetch.
     *
     * Non-blocking — sets the active flag and removes the session from the map.
     * The underlying thread joins at the next stop() or service destruction.
     */
    void stopStream(const std::string& stream_id);

    /// @brief Body of each stream worker thread.
    void streamLoop(StreamPtr session, std::string stream_id);

    std::mutex streams_mu_;
    /// Active sessions keyed by stream_id.
    std::unordered_map<std::string, StreamPtr> streams_;
    /// All stream threads — joined in stop().
    std::mutex thread_mu_;
    std::vector<std::thread> stream_threads_;

    AppConfig   cfg_;       ///< Application configuration.
    IqFetcher   fetcher_;   ///< IQ sample fetcher.
    DemodRouter router_;    ///< Dispatch router.

    std::atomic<bool> running_{false}; ///< True while threads are active.
    std::thread       sub_thread_;     ///< AMQP subscription thread.
    std::thread       worker_thread_;  ///< Task worker thread.

    std::mutex               q_mu_; ///< Protects queue_.
    std::condition_variable  q_cv_; ///< Signals worker when tasks arrive.
    std::queue<PendingDemod> queue_; ///< Pending demodulation tasks.


    std::shared_ptr<ServiceAmqpHandler> amqp_handler_; ///< AMQP session handler.
};

} // namespace demod
