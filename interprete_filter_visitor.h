#ifndef POSEIDON_CORE_INTERPRETE_FILTER_VISITOR_H
#define POSEIDON_CORE_INTERPRETE_FILTER_VISITOR_H

#include "qoperator.hpp"
#include "filter_expression.hpp"
#include "query.hpp"

class interprete_filter_visitor : public expression_visitor {
    dcode_t dict_value_;
    uint64_t int_value_;
    uint64_t ui_value_;
    double fp_value_;

    enum class value_type {
        dcodev,
        intv,
        uiv,
        fpv
    };

    enum class predicate {
        eq,
        ueq
    };

    value_type val_type_;
    predicate pred_;

    graph_db_ptr gdb_;

    arg_builder args_;

public:
    interprete_filter_visitor(graph_db_ptr gdb);
    interprete_filter_visitor(graph_db_ptr gdb, arg_builder & args);

    void visit(int rank, std::shared_ptr<number_token> num) override;

    void visit(int rank, std::shared_ptr<key_token> key) override;

    void visit(int rank, std::shared_ptr<str_token> str) override;

    void visit(int rank, std::shared_ptr<fct_call> str) override;

    void visit(int rank, std::shared_ptr<eq_predicate> eq) override;

    void visit(int rank, std::shared_ptr<and_predicate> andpr) override;

    void visit(int rank, std::shared_ptr<or_predicate> orpr) override;

    std::string key_;
    std::function<bool(const p_item &)> get_pred();
};


#endif //POSEIDON_CORE_INTERPRETE_FILTER_VISITOR_H
