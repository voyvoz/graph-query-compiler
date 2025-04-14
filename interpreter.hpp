#ifndef POSEIDON_CORE_INTERPRETER_HPP
#define POSEIDON_CORE_INTERPRETER_HPP

#include "qoperator.hpp"
#include "filter_expression.hpp"
#include "query.hpp"
#include "interprete_filter_visitor.h"


class interprete_visitor : public op_visitor {
    query query_;
    graph_db_ptr gdb_;
    result_set *rs_;
    interprete_filter_visitor ifv_;
    projection::expr_list pexpr_;
    arg_builder args_;

    std::vector<query> queries_;
public:
    interprete_visitor(graph_db_ptr gdb, arg_builder & args, result_set *rs);

    void visit(std::shared_ptr<scan_op> op) override;

    void visit(std::shared_ptr<foreach_rship_op> op) override;

    void visit(std::shared_ptr<project> op) override;

    void visit(std::shared_ptr<expand_op> op) override;

    void visit(std::shared_ptr<filter_op> op) override;

    void visit(std::shared_ptr<collect_op> op) override;

    void visit(std::shared_ptr<join_op> op) override;

    void visit(std::shared_ptr<sort_op> op) override;

    void visit(std::shared_ptr<limit_op> op) override;

    void visit(std::shared_ptr<end_op> op) override;

    void visit(std::shared_ptr<create_op> op) override;

    void start();

    result_set &get_results() { return *rs_; }
};


#endif //POSEIDON_CORE_INTERPRETER_HPP
