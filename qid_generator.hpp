#ifndef POSEIDON_CORE_QID_GENERATOR_HPP
#define POSEIDON_CORE_QID_GENERATOR_HPP
#include "qoperator.hpp"

class qid_generator : public op_visitor {
public:
    std::string qid;

    virtual void visit(std::shared_ptr<scan_op> op) {
        qid += "S";
    }

    virtual void visit(std::shared_ptr<foreach_rship_op> op) {
        qid += "4";
    }

    virtual void visit(std::shared_ptr<project> op) {
        qid += "P";
    }

    virtual void visit(std::shared_ptr<expand_op> op) {
        qid += "E";
    }

    virtual void visit(std::shared_ptr<filter_op> op) {
        qid += "F";
    }

    virtual void visit(std::shared_ptr<collect_op> op)  {
        qid += "C";
    }

    virtual void visit(std::shared_ptr<join_op> op) {
        qid += "J";
    }

    virtual void visit(std::shared_ptr<sort_op> op) {
        qid += "S";
    }

    virtual void visit(std::shared_ptr<limit_op> op) {
        qid += "L";
    }

    virtual void visit(std::shared_ptr<end_op> op) {
        qid += "N";
    }

    virtual void visit(std::shared_ptr<create_op> op) {
        qid += "A";
    }

};
#endif //POSEIDON_CORE_QID_GENERATOR_HPP
