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

        bool inserted = urlIndex_.Insert(id, 0);
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
    core::OrderedMapFile<url_id_t, int, UrlTreeArity, internal::StringFileComparator> urlIndex_;
};

template<typename T>
concept URLScorer = requires(std::string_view url) {
    { T::Score(url) } -> std::same_as<int>;
};

namespace {

/**
 * @brief Generates K random, unique indicies in [0, N) using the Reservoir
 * Sampling Algorithm L.
 * https://en.wikipedia.org/wiki/Reservoir_sampling#Optimal:_Algorithm_L
 *
 * @param N Index range
 * @param K Number of indicies to generate
 * @return std::vector<size_t> Vector of generated indicies (unique)
 */
std::vector<size_t> GenerateRandomIndicies(size_t N, size_t K) {
    std::vector<size_t> result;
    result.reserve(K);

    // Fill first K indices
    for (size_t i = 0; i < K; i++) {
        result.push_back(i);
    }

    if (K == N) {
        return result;
    }

    // Use a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Algorithm L (Li's algorithm)
    double W = std::exp(std::log(gen() / (double)std::mt19937::max()) / K);

    size_t i = K;
    while (i < N) {
        i += std::floor(std::log(gen() / (double)std::mt19937::max()) / std::log(1 - W)) + 1;
        if (i < N) {
            result[gen() % K] = i;
            W *= std::exp(std::log(gen() / (double)std::mt19937::max()) / K);
        }
    }

    return result;
}

}  // namespace

template<URLScorer Scorer>
class PriorityURLQueue {
    using url_id_t = uint32_t;
    static constexpr auto SampleOverheadFactor = 3;  // TODO: tune this
    static constexpr size_t MinConsideration = 100;

    struct QueuedURL {
        url_id_t id;
        int score;
    };

public:
    PriorityURLQueue(const std::string& directory)
        : store_(directory), queuedURLs_((directory + "/url_queue.dat").c_str()) {}

    bool Seen(std::string_view url) { return store_.Contains(url); }

    size_t Size() const { return queuedURLs_.Size(); }
    bool Empty() const { return Size() == 0; }

    size_t TotalSize() const { return store_.Size(); }

    void PushURL(std::string_view url) {
        auto id = store_.Insert(url);
        if (!id) {
            return;
        }
        auto score = Scorer::Score(url);
        queuedURLs_.PushBack(QueuedURL{*id, score});
    }

    void PopURLs(size_t max, std::vector<std::string>& out) {
        struct Candidate {
            size_t queueIndex;
            QueuedURL url;
        };

        size_t targetSize = std::max(max * SampleOverheadFactor, MinConsideration);
        targetSize = std::min(targetSize, queuedURLs_.Size());

        std::vector<Candidate> candidates;
        candidates.reserve(targetSize);

        auto candidateIndicies = GenerateRandomIndicies(queuedURLs_.Size(), targetSize);
        for (size_t index : candidateIndicies) {
            candidates.push_back({index, queuedURLs_[index]});
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
            out.emplace_back(store_.URL(candidates[i].url.id));
            indiciesToRemove.push_back(candidates[i].queueIndex);
        }

        // Remove retrieved URLs from queue in reverse-index order
        std::sort(indiciesToRemove.begin(), indiciesToRemove.end(), std::greater<>{});
        for (size_t index : indiciesToRemove) {
            RemoveQueuedAtIndex(index);
        }
    }

private:
    void RemoveQueuedAtIndex(size_t index) {
        if (index != queuedURLs_.Size() - 1) {
            std::swap(queuedURLs_[index], queuedURLs_.Back());
        }
        queuedURLs_.PopBack();
    }

    URLStore store_;
    core::VectorFile<QueuedURL> queuedURLs_;
};

}  // namespace mithril

#endif
