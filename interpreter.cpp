#include "interpreter.hpp"

#include <utility>
#include "interprete_filter_visitor.h"

unsigned int op_id_ = 1;

interprete_visitor::interprete_visitor(graph_db_ptr gdb, arg_builder & args, result_set *rs) : gdb_(gdb), query_(query(gdb)), args_(args), ifv_(gdb, args), rs_(rs) {
}

void interprete_visitor::visit(std::shared_ptr<scan_op> op) {
    auto q = query(gdb_);
    auto label = args_.string_args.at(op_id_);
    q = q.all_nodes().has_label(label);
    queries_.push_back(q);
    op_id_++;
}

void interprete_visitor::visit(std::shared_ptr<foreach_rship_op> op) {
    auto label = args_.string_args.at(op_id_);
    auto q = queries_.back();
    queries_.pop_back();

    switch(op->dir_) {
        case RSHIP_DIR::TO: {
            if(op->is_variable()) {
                queries_.push_back(q.to_relationships(op->hops_, label));
            } else {
                queries_.push_back(q.to_relationships(label));
            }
            break;
        }
        case RSHIP_DIR::FROM: {
            if(op->is_variable()) {
                queries_.push_back(q.from_relationships(op->hops_, label));
            } else {
                queries_.push_back(q.from_relationships(label));
            }
            break;
        }
    }
    op_id_++;
}

void interprete_visitor::visit(std::shared_ptr<expand_op> op) {
    auto label = args_.string_args.at(op_id_);
    auto q = queries_.back();
    queries_.pop_back();
    switch(op->exp_) {
        case EXPAND::IN:
            queries_.push_back(q.from_node(label));
            break;
        case EXPAND::OUT:
            queries_.push_back(q.to_node(label));
            break;
    }
    op_id_++;
}

void interprete_visitor::visit(std::shared_ptr<filter_op> op) {
    op->fexpr_->accept(0, ifv_);
    auto q = queries_.back();
    queries_.pop_back();

    queries_.push_back(q.property(ifv_.key_, ifv_.get_pred()));
    op_id_++;
}

void interprete_visitor::visit(std::shared_ptr<sort_op> op) {
    query_ = query_.orderby(op->cmp_);
}

void interprete_visitor::visit(std::shared_ptr<collect_op> op) {
    auto q  = queries_.back();
    queries_.pop_back();
    q = q.collect(*rs_);
    queries_.push_back(q);
}

void interprete_visitor::visit(std::shared_ptr<project> op) {
    pexpr_.clear();
    for(auto & p : op->prexpr_) {
        projection::expr pe;

        switch(p.type) {
            case FTYPE::UINT64:
            {
                pe = PExpr_(p.id, builtin::uint64_property(res, p.key));
                break;
            }
            case FTYPE::INT:
            {
                pe = PExpr_(p.id, builtin::int_property(res, p.key));
                break;
            }
            case FTYPE::DOUBLE:
            {
                pe = PExpr_(p.id, builtin::double_property(res, p.key));
                break;
            }
            case FTYPE::STRING:
            {
                pe = PExpr_(p.id, builtin::string_property(res, p.key));
                break;
            }
            case FTYPE::DATE:
            {
                pe = PExpr_(p.id, builtin::ptime_property(res, p.key));
                break;
            }
            case FTYPE::TIME:
            {
                pe = PExpr_(p.id, builtin::ptime_property(res, p.key));
                break;
            }
        }

        pexpr_.push_back(pe);
    }

    auto q = queries_.back();
    queries_.pop_back();
    queries_.push_back(q.project(pexpr_));
}

void interprete_visitor::visit(std::shared_ptr<join_op> op) {
    auto rhs = queries_.back();
    queries_.pop_back();
    auto lhs = queries_.back();
    queries_.pop_back();

    if(op->jop_ == JOIN_OP::CROSS) {
        lhs = lhs.crossjoin(rhs);
    } else {
        lhs = lhs.outerjoin(op->join_pos_, rhs);
    }
    queries_.push_back(rhs);
    queries_.push_back(lhs);
}

void interprete_visitor::start() {
    for(auto & q : queries_) {
        //q.dump();
        q.start();
    }
    op_id_ = 1;
    //rs_->wait();
}

void interprete_visitor::visit(std::shared_ptr<limit_op> op) {
    query_ = query_.limit(op->limit_);
}

void interprete_visitor::visit(std::shared_ptr<end_op> op) {

}

void interprete_visitor::visit(std::shared_ptr<create_op> op) {
    if(op->ctype_ == create_type::rship) {
        auto src = op->src_des_.first;
        auto dst = op->src_des_.second;
        //query_ = query_.create_rship({src, dst},)
    } else {

    }
}
