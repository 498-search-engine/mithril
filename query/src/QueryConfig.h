#ifndef QUERY_CONFIG_H_
#define QUERY_CONFIG_H_

#include <string>
#include <unordered_set>

namespace query {

class QueryConfig {
public:
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
