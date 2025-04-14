#ifndef POSEIDON_CORE_CODEGEN_HPP
#define POSEIDON_CORE_CODEGEN_HPP
#include "qoperator.hpp"


class codegen_visitor : public op_visitor {
    PContext & ctx;
    unsigned cur_size;
public:
    codegen_visitor(PContext &ctx) : ctx(ctx) {cur_size = 0; }

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

};


#endif //POSEIDON_CORE_CODEGEN_HPP
