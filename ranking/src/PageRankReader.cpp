#include "PageRankReader.h"

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

    void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED) {
        throw std::runtime_error("Failed to memory map pagerank data");
    }

    size_ = st.st_size / sizeof(double);
    map_ = new double[size_];

    char* data = reinterpret_cast<char*>(mapped);

    for (size_t i = 0; i < size_; i++, data += sizeof(double)) {
        uint64_t bytes;
        memcpy(&bytes, data, sizeof(double));

        bytes = ntohll(bytes);
        memcpy(&map_[i], &bytes, sizeof(double));
    }

    munmap(mapped, st.st_size);
}

double PageRankReader::GetDocumentPageRank(data::docid_t docid) {
    return map_[docid];
}
};  // namespace mithril::pagerank