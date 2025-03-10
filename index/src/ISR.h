#ifndef INDEX_ISR_H
#define INDEX_ISR_H


#include "PostingBlock.h"

#include <memory>
#include <string>
#include <vector>

namespace mithril {

class ISR {
public:
    virtual ~ISR() = default;

    // Core ops
    virtual uint32_t getCurrentDocId() const = 0;
    virtual bool next() = 0;                        // Advances to next posting, returns false when exhausted
    virtual bool nextDoc() = 0;                     // Advances to next document, returns false when exhausted
    virtual bool seek(uint32_t target_doc_id) = 0;  // Seeks to first posting >= target
    virtual bool atEnd() const = 0;

    virtual uint32_t getFrequency() const = 0;

    virtual std::string getName() const = 0;
};

class TermISR : public ISR {
public:
    explicit TermISR(std::unique_ptr<BlockReader> reader, const std::string& term);
    ~TermISR() override = default;

    uint32_t getCurrentDocId() const override;
    bool next() override;
    bool nextDoc() override;
    bool seek(uint32_t target_doc_id) override;
    bool atEnd() const override;
    uint32_t getFrequency() const override;
    std::string getName() const override { return "Term[" + term_ + "]"; }
    std::vector<uint32_t> getPositions() const;

private:
    std::unique_ptr<BlockReader> reader_;
    std::string term_;
    const Posting* current_posting_{nullptr};
    bool is_at_end_{false};
};

class AndISR : public ISR {
public:
    explicit AndISR(std::vector<std::unique_ptr<ISR>> children);
    ~AndISR() override = default;

    uint32_t getCurrentDocId() const override;
    bool next() override;
    bool nextDoc() override;
    bool seek(uint32_t target_doc_id) override;
    bool atEnd() const override;
    uint32_t getFrequency() const override;
    std::string getName() const override { return "AND"; }

private:
    std::vector<std::unique_ptr<ISR>> children_;
    uint32_t current_doc_id_{0};
    bool is_at_end_{false};

    bool findMatchingDocument();
};

class OrISR : public ISR {
public:
    explicit OrISR(std::vector<std::unique_ptr<ISR>> children);
    ~OrISR() override = default;

    uint32_t getCurrentDocId() const override;
    bool next() override;
    bool nextDoc() override;
    bool seek(uint32_t target_doc_id) override;
    bool atEnd() const override;
    uint32_t getFrequency() const override;
    std::string getName() const override { return "OR"; }

private:
    std::vector<std::unique_ptr<ISR>> children_;
    uint32_t current_doc_id_{0};
    bool is_at_end_{false};
    int current_min_idx_{-1};

    void findMinimumISR();
};

// Factory functions for creating ISRs
std::unique_ptr<ISR> createTermISR(const std::string& term, const std::string& index_path);
std::unique_ptr<ISR> createAndISR(std::vector<std::unique_ptr<ISR>> children);
std::unique_ptr<ISR> createOrISR(std::vector<std::unique_ptr<ISR>> children);

}  // namespace mithril

#endif