#include "ISRFactory.h"

#include "IndexStreamReader.h"
#include "TermReader.h"
#include "GenericTermReader.h"
#include "TextPreprocessor.h"
#include "IdentityISR.h"

#include <string>
#include <memory>

namespace mithril {

TermReaderFactory::TermReaderFactory(const core::MemMapFile& index_file,
                                     TermDictionary& term_dict,
                                     PositionIndex& position_index)
    : index_file_(index_file), term_dict_(term_dict), position_index_(position_index) {}

std::unique_ptr<IndexStreamReader> TermReaderFactory::CreateISR(const std::string& term,
                                                                FieldType field)
{
    const auto normalized_term = TokenNormalizer::normalize(term, field);
    if (normalized_term == "" || StopwordFilter::isStopword(term)) {
        return std::make_unique<IdentityISR>();
    } else if (field == FieldType::ALL) {
        return std::make_unique<GenericTermReader>(normalized_term, index_file_, term_dict_, position_index_);
    } else {
        return std::make_unique<TermReader>("", normalized_term, index_file_, term_dict_, position_index_);
    }
}

}  // namespace mithril
