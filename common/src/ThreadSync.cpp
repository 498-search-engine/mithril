#include "ThreadSync.h"

#include "core/cv.h"
#include "core/locks.h"

void ThreadSync::RegisterCV(core::cv* cv) {
    core::LockGuard lock(mu_);
    wantsNotifies_.push_back(cv);
}

bool ThreadSync::ShouldSynchronize() const {
    return shutdown_ || shouldPause_;
}

bool ThreadSync::ShouldShutdown() const {
    return shutdown_;
}

bool ThreadSync::ShouldPause() const {
    return shouldPause_;
}

void ThreadSync::MaybePause() {
    if (!shouldPause_.load()) {
        return;
    }
    DoPause();
}

void ThreadSync::DoPause() {
    core::LockGuard lock(mu_);
    if (!shouldPause_) {
        return;
    }

    ++numPaused_;
    allPausedCv_.Signal();
    for (auto* cv : wantsNotifies_) {
        cv->Signal();
    }
    unpauseCv_.Wait(lock, [this] { return !shouldPause_ || shutdown_; });
    --numPaused_;
}

void ThreadSync::Shutdown() {
    core::LockGuard lock(mu_);
    if (shutdown_) {
        return;
    }
    shutdown_.store(true);
    unpauseCv_.Broadcast();
    for (auto* cv : wantsNotifies_) {
        cv->Broadcast();
    }
}

void ThreadSync::StartPause(int n) {
    core::LockGuard lock(mu_);
    shouldPause_.store(true);
    allPausedCv_.Wait(lock, [this, n] { return numPaused_ == n; });
}

void ThreadSync::EndPause() {
    core::LockGuard lock(mu_);
    shouldPause_ = false;
    unpauseCv_.Broadcast();
}
