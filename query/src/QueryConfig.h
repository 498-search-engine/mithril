#ifndef QUERY_CONFIG_H_
#define QUERY_CONFIG_H_

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace query {

class QueryConfig {

private:
    inline static std::string IndexPath = "";
    inline static size_t max_doc_id;
    inline static bool max_doc_id_set = false;
    inline static bool index_path_set = false;

public:
    static void SetIndexPath(const std::string& path) {
        IndexPath = path;
        index_path_set = true;
    }

    static void SetMaxDocId(size_t doc_id) {
        max_doc_id = doc_id;
        max_doc_id_set = true;
    }

    static std::string GetIndexPath() {
        if (!index_path_set) {
            throw std::runtime_error("Index path is not set");
        }
        return IndexPath;
    }

    static size_t GetMaxDocId() {
        if (!max_doc_id_set) {
            throw std::runtime_error("Max doc id is not set");
        }
        return max_doc_id;
    }

    static const std::unordered_set<std::string>& GetValidFields() {
        static const std::unordered_set<std::string> fields = {
            "TITLE",
            "TEXT"
        };
        return fields;
    }

    static const std::unordered_set<std::string>& GetValidOperators() {
        static const std::unordered_set<std::string> operators = {
            "AND",
            "OR",
            "NOT"
        };
        return operators;
    }

    // Allow customization of fields and operators if needed
    static void AddCustomField(const std::string& field) {
        auto& fields = const_cast<std::unordered_set<std::string>&>(GetValidFields());
        fields.insert(field);
    }

    static void AddCustomOperator(const std::string& op) {
        auto& operators = const_cast<std::unordered_set<std::string>&>(GetValidOperators());
        operators.insert(op);
    }
};

} // namespace query

#endif // QUERY_CONFIG_H_
