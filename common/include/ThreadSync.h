#ifndef COMMON_PAUSEPOINT_H
#define COMMON_PAUSEPOINT_H

#include "core/cv.h"
#include "core/mutex.h"

#include <atomic>
#include <vector>

/**
 * @brief ThreadSync helps synchronize the lifetimes of threads. It supports
 * shutting down threads or pausing threads at a rendezvous point.
 */
class ThreadSync {
public:
    ThreadSync() = default;

    /**
     * @brief Registers a condition variable to notify when thread
     * synchronization is required. See ShouldSynchronize(). Lifetime of cv must
     * be valid for lifetime of this ThreadSync object.
     *
     * @param cv CV to notify.
     */
    void RegisterCV(core::cv* cv);

    /**
     * @brief Returns whether a shutdown or pause synchronization is needed.
     */
    bool ShouldSynchronize() const;
    /**
     * @brief Returns whether a shutdown has been requested.
     */
    bool ShouldShutdown() const;
    /**
     * @brief Returns whether a pause has been requested.
     */
    bool ShouldPause() const;

    /**
     * @brief Possibly pauses the calling thread if a pause has been requested.
     * If not, returns immediately.
     */
    void MaybePause();

    /**
     * @brief Notifies threads of a requested shutdown.
     */
    void Shutdown();

    /**
     * @brief Initiates a pause, waiting for a certain number of threads to
     * reach the pause point before returning.
     *
     * @param n Number of corresponding ShouldPause() calls to wait for.
     */
    void StartPause(int n);

    /**
     * @brief Ends the current pause, allowing all threads waiting at a
     * ShouldPause() point to continue.
     */
    void EndPause();

private:
    void DoPause();

    mutable core::Mutex mu_;
    core::cv allPausedCv_;
    core::cv unpauseCv_;

    std::vector<core::cv*> wantsNotifies_;
    std::atomic<int> numPaused_{0};
    std::atomic<bool> shouldPause_{false};
    std::atomic<bool> shutdown_{false};
};


#endif
