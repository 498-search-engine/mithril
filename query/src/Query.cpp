#include "Query.h" inline mithril::FieldType mithril::detail::TokenTypeToField(TokenType token_type) {
switch (token_type) {
case TokenType::WORD:
    return mithril::FieldType::ALL;
case TokenType::TITLE:
    return mithril::FieldType::TITLE;
case TokenType::URL:
    return mithril::FieldType::URL;
case TokenType::ANCHOR:
    return mithril::FieldType::ANCHOR;
case TokenType::DESC:
    return mithril::FieldType::DESC;
case TokenType::BODY:
    return mithril::FieldType::BODY;
default:  // WARNING: this should not happen
    return mithril::FieldType::ALL;
}
}
std::vector<uint32_t> TermQuery::evaluate() const {
    TermReaderFactory term_reader_factory(index_file_, term_dict_, position_index_);
    const auto field = mithril::detail::TokenTypeToField(token_.type);
    auto term = term_reader_factory.CreateISR(token_.value, field);

    std::vector<uint32_t> results;
    results.reserve(MAX_DOCUMENTS);

    while (term->hasNext()) {
        const uint32_t docId = term->currentDocID();
        results.emplace_back(docId);
        term->moveNext();
    }

    return results;
}
std::unique_ptr<mithril::IndexStreamReader> TermQuery::generate_isr() const {
    TermReaderFactory term_reader_factory(index_file_, term_dict_, position_index_);
    const auto field = mithril::detail::TokenTypeToField(token_.type);
    return term_reader_factory.CreateISR(token_.value, field);
}
std::string TermQuery::to_string() const {
    return "TERM(" + token_.value + " " + token_.toString() + ")";
}
std::vector<uint32_t> AndQuery::evaluate() const {
    std::vector<uint32_t> left_docs = left_->evaluate();
    std::vector<uint32_t> right_docs = right_->evaluate();
    auto intersected_ids = intersect_simple(left_docs, right_docs);

    return intersected_ids;
}
std::unique_ptr<mithril::IndexStreamReader> AndQuery::generate_isr() const {
    auto left_isr = left_->generate_isr();
    auto right_isr = right_->generate_isr();

    // TODO: decide if this is the right behaviour
    if (left_isr->isIdentity() && right_isr->isIdentity()) {
        return std::make_unique<mithril::IdentityISR>();
    } else if (left_isr->isIdentity()) {
        return std::move(right_isr);
    } else if (right_isr->isIdentity()) {
        return std::move(left_isr);
    } else {
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        readers.push_back(std::move(left_isr));
        readers.push_back(std::move(right_isr));
        return std::make_unique<mithril::TermAND>(std::move(readers));
    }
}
std::string AndQuery::to_string() const {
    return "AND(" + left_->to_string() + ", " + right_->to_string() + ")";
}
OrQuery::OrQuery(Query* left, Query* right) : left_(left), right_(right) {
    if (!left && !right) {
        std::cerr << "Need a left and right query\n";
        exit(1);
    }
}
std::vector<uint32_t> OrQuery::evaluate() const {
    std::vector<uint32_t> left_docs = left_->evaluate();
    std::vector<uint32_t> right_docs = right_->evaluate();
    auto intersected_ids = union_simple(left_docs, right_docs);

    return intersected_ids;
}
std::unique_ptr<mithril::IndexStreamReader> OrQuery::generate_isr() const {
    auto left_isr = left_->generate_isr();
    auto right_isr = right_->generate_isr();

    // TODO: decide if this is the right behaviour
    if (left_isr->isIdentity() && right_isr->isIdentity()) {
        return std::make_unique<mithril::IdentityISR>();
    } else if (left_isr->isIdentity()) {
        return std::move(right_isr);
    } else if (right_isr->isIdentity()) {
        return std::move(left_isr);
    } else {
        std::vector<std::unique_ptr<mithril::IndexStreamReader>> readers;
        readers.push_back(std::move(left_isr));
        readers.push_back(std::move(right_isr));
        return std::make_unique<mithril::TermOR>(std::move(readers));
    }
}
NotQuery::NotQuery(Query* expression)
    : expression_(expression),
      not_isr_(
          std::make_unique<mithril::NotISR>(std::move(expression->generate_isr()), query::QueryConfig::GetMaxDocId())) {
    if (!expression) {
        std::cerr << "Need an expression for NOT query\n";
        exit(1);
    }
}
std::vector<uint32_t> NotQuery::evaluate() const {
    // Get all documents that match the expression
    std::vector<uint32_t> expr_docs = expression_->evaluate();
    std::vector<uint32_t> all_docs;
    all_docs.reserve(query::QueryConfig::GetMaxDocId() - expr_docs.size());

    // Generate all document IDs from 0 to max_doc_id
    for (uint32_t i = 0; i < query::QueryConfig::GetMaxDocId(); i++) {
        all_docs.push_back(i);
    }

    // Return documents that are NOT in expr_docs
    std::vector<uint32_t> result;
    size_t expr_idx = 0;

    for (uint32_t doc_id : all_docs) {
        while (expr_idx < expr_docs.size() && expr_docs[expr_idx] < doc_id) {
            expr_idx++;
        }

        if (expr_idx >= expr_docs.size() || expr_docs[expr_idx] != doc_id) {
            result.push_back(doc_id);
        }
    }

    return result;
}
std::unique_ptr<mithril::IndexStreamReader> NotQuery::generate_isr() const {
    return std::make_unique<mithril::NotISR>(expression_->generate_isr(), query::QueryConfig::GetMaxDocId());
}
std::unique_ptr<mithril::IndexStreamReader> QuoteQuery::generate_isr() const {

    std::vector<std::string> quote_terms = ExtractQuoteTerms(quote_token_);
    std::cout << "Quote terms: ";
    for (const auto& term : quote_terms) {
        std::cout << "Quote term: " << term << std::endl;
    }

    return std::make_unique<::mithril::TermQuote>(
        query::QueryConfig::GetIndexPath(), quote_terms, index_file_, term_dict_, position_index_);
}
std::unique_ptr<mithril::IndexStreamReader> PhraseQuery::generate_isr() const {
    std::vector<std::string> phrase_terms = ExtractQuoteTerms(phrase_token_);
    std::cout << "Phrase terms: ";
    for (const auto& term : phrase_terms) {
        std::cout << "Phrase term: " << term << std::endl;
    }

    // Use TermPhrase for fuzzy phrase matching
    return std::make_unique<::mithril::TermPhrase>(
        query::QueryConfig::GetIndexPath(), phrase_terms, index_file_, term_dict_, position_index_);
}
std::vector<uint32_t> QuoteQuery::evaluate() const {
    // Process the quote token to get the exact phrase
    std::string phrase = quote_token_.value;

    // Tokenize the phrase to extract individual terms
    std::istringstream iss(phrase);
    std::vector<std::string> terms;
    std::string term;
    while (iss >> term) {
        terms.push_back(term);
    }

    if (terms.empty()) {
        return {};
    }

    // Use generate_isr to get the stream reader and process results
    std::unique_ptr<mithril::IndexStreamReader> isr = generate_isr();
    std::vector<uint32_t> result;
    while (isr->hasNext()) {
        result.push_back(isr->currentDocID());
        isr->moveNext();
    }
    return result;
}
std::vector<uint32_t> PhraseQuery::evaluate() const {
    // Process the phrase token to get the terms for proximity search
    std::string phrase = phrase_token_.value;

    // Tokenize the phrase
    std::istringstream iss(phrase);
    std::vector<std::string> terms;
    std::string term;
    while (iss >> term) {
        terms.push_back(term);
    }

    if (terms.empty()) {
        return {};
    }

    // Use generate_isr to get the stream reader and process results
    std::unique_ptr<mithril::IndexStreamReader> isr = generate_isr();
    std::vector<uint32_t> result;
    while (isr->hasNext()) {
        result.push_back(isr->currentDocID());
        isr->moveNext();
    }
    return result;
}
