#include "ranking/PageRankReader.h"

#include "core/config.h"

#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>

namespace mithril::pagerank {
PageRankReader::PageRankReader() {
    core::Config config = core::Config("pagerank.conf");
    const std::string outputFile = std::string(config.GetString("output_file").Cstr());

    int fd = open(outputFile.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open pagerank output file: " + outputFile);
    }

    struct stat st{};
    if (fstat(fd, &st) == -1) {
        throw std::runtime_error("Failed to stat pagerank output file: " + outputFile);
    }

    map_ = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (map_ == MAP_FAILED) {
        throw std::runtime_error("Failed to memory map pagerank data");
    }

    start = ntohl(*reinterpret_cast<uint32_t*>(map_));
    size_ = (st.st_size - 4) / sizeof(float);
}

PageRankReader::~PageRankReader() {
    if (map_ != nullptr) {
        munmap(map_, (size_ * sizeof(float)) + 4);
    }
}

float PageRankReader::GetDocumentPageRank(data::docid_t docid) {
    if (docid < start) {
        throw std::runtime_error("bad pagerank docid: " + std::to_string(docid));
    }
    if (docid >= start + size_) {
        throw std::runtime_error("bad pagerank docid: " + std::to_string(docid));
    }

    char* data = (reinterpret_cast<char*>(map_) + 4) + ((docid - start) * sizeof(float));

    float bytes;
    memcpy(&bytes, data, sizeof(float));

    /**
     * bit_cast is equivalent to reinterpret_cast
     * DO NOT CHANGE TO STATIC CAST
     */
    bytes = std::bit_cast<float>(ntohl(std::bit_cast<uint32_t>(bytes)));

    return bytes;
}
};  // namespace mithril::pagerank