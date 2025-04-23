#ifndef INDEX_ISRFACTORY_H
#define INDEX_ISRFACTORY_H

#include "IndexStreamReader.h"
#include "core/mem_map_file.h"
#include "TermDictionary.h"
#include "PositionIndex.h"
#include "TextPreprocessor.h"

#include <memory>
#include <string>

namespace mithril {

class TermReaderFactory {
public:
    TermReaderFactory(const core::MemMapFile& index_file,
                      TermDictionary& term_dict,
                      PositionIndex& position_index);

    TermReaderFactory(const TermReaderFactory&) = delete;
    TermReaderFactory& operator=(const TermReaderFactory&) = delete;

    ~TermReaderFactory() = default;

    /**
     * @brief Returns appropriate term reader ISR
     * 
     * @param term : std::string
     * @return std::unique_ptr<IndexStreamReader> 
     */
    std::unique_ptr<IndexStreamReader> CreateISR(const std::string& term, FieldType field = FieldType::ALL);

private:
    const core::MemMapFile& index_file_;
    TermDictionary& term_dict_;
    PositionIndex& position_index_;
};

}  // namespace mithril

#endif