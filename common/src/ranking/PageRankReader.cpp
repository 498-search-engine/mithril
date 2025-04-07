#include "ranking/PageRankReader.h"

#include "core/config.h"

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

    size_ = st.st_size / sizeof(double);
}

PageRankReader::~PageRankReader() {
    if (map_ != nullptr) {
        munmap(map_, size_ * sizeof(double));
    }
}

double PageRankReader::GetDocumentPageRank(data::docid_t docid) {
    char* data = reinterpret_cast<char*>(map_) + (docid * sizeof(double));

    double bytes;
    memcpy(&bytes, data, sizeof(double));

    /**
     * bit_cast is equivalent to reinterpret_cast
     * for some reason reinterpret_cast on double & uint64_t is not allowed
     * DO NOT CHANGE TO STATIC CAST
     */
    bytes = std::bit_cast<double>(ntohll(std::bit_cast<uint64_t>(bytes)));

    return bytes;
}
};  // namespace mithril::pagerank