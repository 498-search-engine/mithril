#ifndef CRAWLER_PRIORITYURLQUEUE_H
#define CRAWLER_PRIORITYURLQUEUE_H

#include "core/optional.h"
#include "core/ordered_map_file.h"
#include "core/vector_file.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/unistd.h>

namespace mithril {

namespace internal {

/**
 * @brief A on-disk list of strings.
 */
class StringFile {
    struct Header {
        size_t totalBytes;
    };

public:
    StringFile(const char* dataPath, const char* offsetPath) : offsetFile_(offsetPath) {
        bool exists = access(dataPath, F_OK) != -1;

        fd_ = open(dataPath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd_ == -1) {
            throw std::runtime_error("failed to open string file");
        }

        if (exists) {
            struct stat st {};
            if (fstat(fd_, &st) == -1) {
                close(fd_);
                throw std::runtime_error("failed to get string file size");
            }
            fileSize_ = st.st_size;
        } else {
            // Initialize file with empty space
            fileSize_ = core::PageSize * 8;
            if (ftruncate(fd_, fileSize_) == -1) {
                close(fd_);
                throw std::runtime_error("failed to initialize string file");
            }
        }

        mapped_ = mmap(nullptr, fileSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapped_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("failed to map string file");
        }

        if (!exists) {
            // Initialize header
            DataHeader()->totalBytes = 0;
        }

        totalBytes_ = DataHeader()->totalBytes;
    }

    ~StringFile() {
        if (mapped_ != nullptr) {
            munmap(mapped_, fileSize_);
            mapped_ = nullptr;
        }
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }

    void PushBack(std::string_view s) {
        while (DataCapacity() - totalBytes_ < s.size()) {
            Resize(fileSize_ * 2);
        }

        // Copy string data into space
        std::memcpy(Data() + totalBytes_, s.data(), s.size());

        // Increment used space
        size_t offset = totalBytes_;
        totalBytes_ += s.size();
        DataHeader()->totalBytes = totalBytes_;

        // Append end of data
        offsetFile_.PushBack(offset);
    }

    void PopBack() {
        if (offsetFile_.Size() == 0) {
            throw std::out_of_range("index out of range");
        }
        auto start = offsetFile_.Back();
        offsetFile_.PopBack();
        totalBytes_ = start;
    }

    std::string_view operator[](size_t n) const {
        if (n >= offsetFile_.Size()) {
            throw std::out_of_range("index out of range");
        }

        auto start = offsetFile_[n];
        auto end = offsetFile_.Size() - 1 == n ? totalBytes_ : offsetFile_[n + 1];
        auto size = end - start;

        return std::string_view{Data() + start, size};
    }

    size_t Size() const { return offsetFile_.Size(); }
    bool Empty() const { return offsetFile_.Empty(); }

private:
    Header* DataHeader() { return static_cast<Header*>(mapped_); }
    size_t DataCapacity() const { return fileSize_ - sizeof(Header); }

    char* Data() { return static_cast<char*>(mapped_) + sizeof(Header); }
    const char* Data() const { return static_cast<char*>(mapped_) + sizeof(Header); }

    void Resize(size_t newFileSize) {
        if (mapped_ != nullptr) {
            munmap(mapped_, fileSize_);
        }

        if (ftruncate(fd_, newFileSize) == -1) {
            throw std::runtime_error("failed to resize url data file");
        }

        mapped_ = mmap(nullptr, newFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapped_ == MAP_FAILED) {
            throw std::runtime_error("failed to remap url data file after resize");
        }

        fileSize_ = newFileSize;
    }

    int fd_{-1};
    void* mapped_{nullptr};
    size_t fileSize_{0};
    size_t totalBytes_{0};

    core::VectorFile<size_t> offsetFile_;
};

struct StringFileComparator {
    const StringFile& f;
    int operator()(auto a, auto b) const {
        auto stringA = f[a];
        auto stringB = f[b];
        return stringA.compare(stringB);
    }
    int operator()(auto a, std::string_view b) const {
        auto stringA = f[a];
        return stringA.compare(b);
    }
};

}  // namespace internal

constexpr size_t UrlTreeArity = 128;

/**
 * @brief A on-disk store of URLs.
 */
class URLStore {
public:
    using url_id_t = uint32_t;

    URLStore(const std::string& directory)
        : URLStore((directory + "/url_data.dat").c_str(),
                   (directory + "/url_offsets.dat").c_str(),
                   (directory + "/url_index.dat").c_str()) {}

    URLStore(const char* urlDataPath, const char* offsetPath, const char* urlIndexPath)
        : stringFile_(urlDataPath, offsetPath), urlIndex_(urlIndexPath, internal::StringFileComparator{stringFile_}) {}

    /**
     * @brief Returns whether the given URL is in the store already.
     *
     * @param url URL to check for
     */
    bool Contains(std::string_view url) const { return urlIndex_.Contains(url); }

    /**
     * @brief Returns the number of URLs in the store.
     */
    size_t Size() const { return stringFile_.Size(); }

    /**
     * @brief Returns whether the URL store is empty.
     */
    bool Empty() const { return stringFile_.Empty(); }

    /**
     * @brief Inserts a URL into the store, if it doesn't already exist in the
     * store.
     *
     * @param url URL to insert
     * @return core::Optional<url_id_t> ID of inserted URL, or nullopt if no
     * insertion occurred
     */
    core::Optional<url_id_t> Insert(std::string_view url) {
        auto id = static_cast<url_id_t>(stringFile_.Size());
        stringFile_.PushBack(url);

        bool inserted = urlIndex_.Insert(id, id);
        if (!inserted) {
            // String was already in the index, un-push from the string file
            stringFile_.PopBack();
            return core::nullopt;
        }

        // Insertion successful
        return {id};
    }

    std::string_view URL(url_id_t id) const { return stringFile_[id]; }

private:
    internal::StringFile stringFile_;
    core::OrderedMapFile<url_id_t, url_id_t, UrlTreeArity, internal::StringFileComparator> urlIndex_;
};

template<typename T>
concept URLScorer = requires(std::string_view url) {
    { T::Score(url) } -> std::same_as<unsigned int>;
};

template<URLScorer Scorer>
class PriorityURLQueue {
    using url_id_t = uint32_t;
    static constexpr auto SampleOverheadFactor = 1.5;  // TODO: tune this

    struct QueuedURL {
        url_id_t id;
        unsigned int score;
    };

    using queue_t = core::VectorFile<QueuedURL>;

public:
    PriorityURLQueue(const std::string& directory, unsigned int highScoreCutoff, unsigned int highScoreQueuePercent)
        : highScoreCutoff_(highScoreCutoff),
          highScoreQueuePercent_(highScoreQueuePercent),
          store_(directory),
          highScoreQueuedURLs_((directory + "/url_queue.dat").c_str()),
          lowScoreQueuedURLs_((directory + "/url_queue_low_score.dat").c_str()) {}

    bool Seen(std::string_view url) { return store_.Contains(url); }

    size_t Size() const { return highScoreQueuedURLs_.Size() + lowScoreQueuedURLs_.Size(); }
    bool Empty() const { return Size() == 0; }

    size_t TotalSize() const { return store_.Size(); }

    void PushURL(std::string_view url) {
        auto id = store_.Insert(url);
        if (!id) {
            return;
        }

        auto score = Scorer::Score(url);
        auto queued = QueuedURL{*id, score};

        if (score >= highScoreCutoff_) {
            highScoreQueuedURLs_.PushBack(queued);
        } else {
            lowScoreQueuedURLs_.PushBack(queued);
        }
    }

    template<typename Filter>
    void PopURLs(size_t max, std::vector<std::string>& out, Filter f) {
        // Randomly choose either the low score or high score queue based on the
        // percentage configured
        auto choice = static_cast<unsigned int>(rand() % 100);
        if (choice < highScoreQueuePercent_) {
            PopURLsFromQueue(store_, highScoreQueuedURLs_, max, out, f);
        } else {
            PopURLsFromQueue(store_, lowScoreQueuedURLs_, max, out, f);
        }
    }

private:
    template<typename Filter>
    static void PopURLsFromQueue(URLStore& store, queue_t& queue, size_t max, std::vector<std::string>& out, Filter f) {
        struct Candidate {
            size_t queueIndex;
            QueuedURL url;
        };

        auto sampleSize = static_cast<size_t>(static_cast<double>(max) * SampleOverheadFactor);
        size_t targetSize = std::min(sampleSize, queue.Size());

        std::vector<Candidate> candidates;
        candidates.reserve(targetSize);

        if (queue.Size() * 2 < targetSize) {
            // The queued urls list is small, don't bother with randomness
            for (size_t i = 0; i < queue.Size(); ++i) {
                if (!f(store.URL(queue[i].id))) {
                    continue;
                }
                candidates.push_back({i, queue[i]});
                if (candidates.size() >= targetSize) {
                    break;
                }
            }
        } else {
            // Randomly select indicies into the queued urls list
            auto seen = std::set<size_t>{};
            for (size_t i = 0; i < targetSize * 2; ++i) {
                size_t index = rand() % queue.Size();
                if (seen.contains(index)) {
                    continue;
                }
                seen.insert(index);
                if (!f(store.URL(queue[index].id))) {
                    continue;
                }
                candidates.push_back({index, queue[index]});
                if (candidates.size() >= targetSize) {
                    break;
                }
            }
        }

        // Sort candidate URLs by their score
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.url.score > b.url.score;
        });

        size_t targetReturn = std::min(max, candidates.size());
        out.reserve(targetReturn);
        auto indiciesToRemove = std::vector<size_t>{};
        indiciesToRemove.reserve(targetReturn);

        // Push retrieved URLs
        for (size_t i = 0; i < targetReturn; ++i) {
            out.emplace_back(store.URL(candidates[i].url.id));
            indiciesToRemove.push_back(candidates[i].queueIndex);
        }

        // Remove retrieved URLs from queue in reverse-index order
        std::sort(indiciesToRemove.begin(), indiciesToRemove.end(), std::greater<>{});
        for (size_t index : indiciesToRemove) {
            RemoveQueuedAtIndex(queue, index);
        }
    }

    static void RemoveQueuedAtIndex(queue_t& queue, size_t index) {
        if (index != queue.Size() - 1) {
            std::swap(queue[index], queue.Back());
        }
        queue.PopBack();
    }

    unsigned int highScoreCutoff_;
    unsigned int highScoreQueuePercent_;

    URLStore store_;
    queue_t highScoreQueuedURLs_;
    queue_t lowScoreQueuedURLs_;
};

}  // namespace mithril

#endif
