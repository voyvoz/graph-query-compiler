#ifndef ART_QOPERATOR_HPP
#define ART_QOPERATOR_HPP

#include <iostream>

#include "p_context.hpp"
#include "filter_expression.hpp"
#include "global_definitions.hpp"

#include "query_engine.hpp"

using namespace llvm;

class base_op;
class scan_op;
class foreach_rship_op;
class project;
class expand_op;
class join_op;
class filter_op;
class collect_op;
class sort_op;
class limit_op;
class end_op;
class create_op;

class op_visitor {
public:
    virtual ~op_visitor() = default;

    virtual void visit(std::shared_ptr<scan_op> op) = 0;

    virtual void visit(std::shared_ptr<foreach_rship_op> op) = 0;

    virtual void visit(std::shared_ptr<project> op) = 0;

    virtual void visit(std::shared_ptr<expand_op> op) = 0;

    virtual void visit(std::shared_ptr<filter_op> op) = 0;

    virtual void visit(std::shared_ptr<collect_op> op) = 0;

    virtual void visit(std::shared_ptr<join_op> op) = 0;

    virtual void visit(std::shared_ptr<sort_op> op) = 0;

    virtual void visit(std::shared_ptr<limit_op> op) = 0;

    virtual void visit(std::shared_ptr<end_op> op) = 0;

    virtual void visit(std::shared_ptr<create_op> op) = 0;
};

using algebra_optr = std::shared_ptr<base_op>;
using algebra_optr_list = std::vector<algebra_optr>;

struct result {
    uint64_t *obj_;
    uint64_t type_;
    uint64_t *null;
};

struct result_list_node {
    result_list_node *next;
    result_list_node *prev;
    uint64_t additional_;
    result *res_;
};

struct result_list {
    result_list_node *head_;
    result_list_node *tail_;
    uint64_t size;
};

enum class qop_type {
    none,
    any,
    scan,
    project,
    filter,
    foreach_rship,
    expand,
    cross_join,
    left_join,
    hash_join,
    nest_loop_join,
    sort,
    limit,
    collect,
    create
};

enum class create_type {
    node,
    rship
};

class query_engine;

class base_op {
protected:

public:
    algebra_optr_list inputs_;
    base_op() = default;

    base_op(algebra_optr inp) { inputs_.push_back(inp); }

    void set_consumer(Function *fct) { consumer_ = fct; }

    virtual ~base_op() = default;

    virtual void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false) = 0;

    qop_type type_;
    std::string name_;
    Function *consumer_;
    unsigned op_id_;
    int produced_type_;
};

class scan_op : public base_op, public std::enable_shared_from_this<scan_op> {
public:
    std::string label_;

    scan_op(algebra_optr inp) {
        inputs_.push_back(inp);
    }

    scan_op(std::string label, bool indexed, algebra_optr inp) : label_(label), indexed_(indexed) {
        inputs_.push_back(inp);
        if (label.empty()) {
            name_ = "Scan_All";
        } else {
            name_ = "Scan_" + label_;
        }

        type_ = qop_type::scan;
        produced_type_ = 0;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    bool indexed_;
};

inline algebra_optr Scan(std::string label, algebra_optr op) { return std::make_shared<scan_op>(label, false, op); }
inline algebra_optr IndexScan(algebra_optr op) { return std::make_shared<scan_op>("", true, op); }

enum class RSHIP_DIR {
    FROM = 0,
    TO = 1
};

class foreach_rship_op : public base_op, public std::enable_shared_from_this<foreach_rship_op> {
public:
    std::string dir_str(RSHIP_DIR dir) {
        switch (dir) {
            case RSHIP_DIR::FROM:
                return "from";
            case RSHIP_DIR::TO:
                return "to";
        }
    }

    foreach_rship_op(algebra_optr inp) {
        inputs_.push_back(inp);
    }

    foreach_rship_op(RSHIP_DIR dir, std::pair<int, int> hops, std::string label, algebra_optr inp) : dir_(dir), label_(label), hops_(hops) {
        name_ = "ForeachRelationship";
        inputs_.push_back(inp);
        produced_type_ = 1;
        type_ = qop_type::foreach_rship;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    bool is_variable() {
        return hops_.first == 0 && hops_.second == 0 ? false : true;
    }

    RSHIP_DIR dir_;
    std::string label_;
    std::pair<int, int> hops_;
};

inline algebra_optr ForeachRship(RSHIP_DIR dir, std::pair<int, int> hops, std::string label, algebra_optr op) {
    return std::make_shared<foreach_rship_op>(dir, hops, label, op);
}



struct pr_expr {
    std::size_t id;
    std::string key;
    FTYPE type;
};

class project : public base_op, public std::enable_shared_from_this<project> {
public:

    project(algebra_optr inp) {
        inputs_.push_back(inp);
    }

    project(std::vector<pr_expr> prexpr, algebra_optr inp) : prexpr_(prexpr) {
        name_ = "Project_";
        inputs_.push_back(inp);

        type_ = qop_type::project;

        for(auto & e : prexpr) {
            r_ids_.insert(e.id);
        }

        produced_type_ = -1;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    std::vector<pr_expr> prexpr_;
    std::set<std::size_t> r_ids_;
    std::vector<int> new_types;
};

inline algebra_optr Project(std::vector<pr_expr> prexpr, algebra_optr op) { return std::make_shared<project>(prexpr, op); }

enum class EXPAND {
    IN,
    OUT
};

std::string expand_str(EXPAND exp);

class expand_op : public base_op, public std::enable_shared_from_this<expand_op> {
public:
    expand_op(EXPAND exp, algebra_optr inp) : exp_(exp) {
        name_ = "Expand";
        inputs_.push_back(inp);
        type_ = qop_type::expand;
        produced_type_ = 0;
    }

    expand_op(EXPAND exp, std::string &label, algebra_optr inp) : exp_(exp), label_(label) {
        name_ = "Expand";
        inputs_.push_back(inp);
        type_ = qop_type::expand;
        produced_type_ = 0;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    EXPAND exp_;
    std::string label_;
};

inline algebra_optr Expand(EXPAND exp, std::string label, algebra_optr op) { return std::make_shared<expand_op>(exp, label, op); }


enum class JOIN_OP {
    CROSS,
    LEFT_OUTER,
    NESTED_LOOP,
    HASH_JOIN
};

std::string join_op_str(JOIN_OP jop);

class join_op : public base_op, public std::enable_shared_from_this<join_op> {
public:
    join_op(JOIN_OP jop, std::pair<int, int> pos, algebra_optr op) : jop_(jop), join_pos_(std::move(pos)) {
        name_ = "Join";
        inputs_.push_back(op);

        switch (jop) {
            case JOIN_OP::LEFT_OUTER:
                type_ = qop_type::left_join;
                break;
            case JOIN_OP::CROSS:
                type_ = qop_type::cross_join;
                break;
            case JOIN_OP::HASH_JOIN:
                type_ = qop_type::hash_join;
                break;
            case JOIN_OP::NESTED_LOOP:
                type_ = qop_type::nest_loop_join;
                break;
        }

        produced_type_ = -1;
    }

    join_op(JOIN_OP jop, std::pair<int, int> pos, algebra_optr lhs, algebra_optr rhs) : jop_(jop),
                                                                                        join_pos_(std::move(pos)) {
        name_ = "Join";
        inputs_.push_back(lhs);
        inputs_.push_back(rhs);

        switch(jop) {
            case JOIN_OP::CROSS:
                type_ = qop_type::cross_join;
                break;
            case JOIN_OP::LEFT_OUTER:
                type_ = qop_type::left_join;
                break;
            case JOIN_OP::HASH_JOIN:
                type_ = qop_type::hash_join;
                break;
            case JOIN_OP::NESTED_LOOP:
                type_ = qop_type::nest_loop_join;
                break;
        }
        produced_type_ = -1;
    }

    Function *codegen_rhs(PContext &ctx, Function *consumer);

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    JOIN_OP jop_;
    std::pair<int, int> join_pos_;

    std::vector<std::array<int*, 100>> join_inputs_;
};

inline algebra_optr Join(JOIN_OP jop, std::pair<int, int> pos, algebra_optr lhs, algebra_optr rhs) {
    return std::make_shared<join_op>(jop, std::move(pos), lhs, rhs);
}

class collect_op : public base_op, public std::enable_shared_from_this<collect_op> {
    
public:

    collect_op(bool print_on_collect) : print_on_collect_(print_on_collect) {
        name_ = "Collect";
        type_ = qop_type::collect;
        produced_type_ = -1;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    bool print_on_collect_;
};

inline algebra_optr Collect(bool print = false) { return std::make_shared<collect_op>(print); }

class filter_op : public base_op, public std::enable_shared_from_this<filter_op> {
public:
    filter_op(expr fexpr, algebra_optr inp) : fexpr_(fexpr) {
        name_ = "Filter";
        inputs_.push_back(inp);
        type_ = qop_type::filter;
        produced_type_ = -1;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    expr fexpr_;
    std::string op_name_ = "";
};

inline algebra_optr Filter(expr fexpr, algebra_optr op) { return std::make_shared<filter_op>(fexpr, op); }

class sort_op : public base_op, public std::enable_shared_from_this<sort_op> {
    std::pair<int, int> sort_ids;

public:

    sort_op(std::function<bool(const qr_tuple &, const qr_tuple &)> cmp, algebra_optr inp) {
        name_ = "Sort";
        type_ = qop_type::sort;
        inputs_.push_back(inp);
        cmp_ = cmp;
        produced_type_ = -1;
    }

    static void sort(result_set *rs) {
        rs->sort(cmp_);
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    static std::function<bool(const qr_tuple &, const qr_tuple &)> cmp_;
    std::pair<int, int> ids_;
};

inline algebra_optr Sort(std::function<bool(const qr_tuple &, const qr_tuple &)> cmp, algebra_optr inp) { return std::make_shared<sort_op>(cmp, inp); }

class limit_op : public base_op, public std::enable_shared_from_this<limit_op> {
public:

    limit_op(int limit, algebra_optr inp) : limit_(limit) {
        name_ = "Limit";
        type_ = qop_type::limit;
        inputs_.push_back(inp);
        produced_type_ = -1;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    int limit_;
};

inline algebra_optr Limit(int limit, algebra_optr inp) { return std::make_shared<limit_op>(limit, inp); }

class end_op : public base_op, public std::enable_shared_from_this<end_op> {
public:

    end_op() {
        name_ = "End";
        type_ = qop_type::none;
        produced_type_ = -1;
    }

    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    std::vector<std::array<int*, 100>> *join_inputs_;
};

inline algebra_optr End() { return std::make_shared<end_op>(); }

class create_op : public base_op, public std::enable_shared_from_this<create_op> {
public:

    create_op() {}

    create_op(create_type ct, algebra_optr inp) {
        name_ = "CreateNode";
        type_ = qop_type::create;
        inputs_.push_back(inp);
        produced_type_ = static_cast<int>(ct);
    }

    create_op(create_type ct, std::pair<int, int> src_des, algebra_optr inp) {
        name_ = "CreateNode";
        type_ = qop_type::create;
        inputs_.push_back(inp);
        produced_type_ = static_cast<int>(ct);
        src_des_ = src_des;
    }
    
    void codegen(op_visitor & vis, unsigned & op_id, bool interpreted = false);

    create_type ctype_;
    std::pair<int, int> src_des_;
    std::string label_;
    properties_t props_;
};
inline algebra_optr CreateNode(algebra_optr inp) { return std::make_shared<create_op>(create_type::node, inp); }
inline algebra_optr CreateRship(std::pair<int, int> src_des, algebra_optr inp) { return std::make_shared<create_op>(create_type::rship, src_des, inp); }

struct FExp {
    FExp(PContext &ctx, FOP fop, FTYPE type, std::string property, std::string value) : fop_(fop), type_(type),
                                                                                        property_(property),
                                                                                        value_(value), ctx(ctx) {
        get_vec_back = ctx.gen_funcs["qr_list_get_front"];
    }

    void codegen(Function *parent, Value *qr_alloc, BasicBlock *next_true, BasicBlock *next_false);

    FOP fop_;
    FTYPE type_;
    std::string property_;
    std::string value_;
    Value *get_vec_back;
    PContext &ctx;
};


#endif //ART_QOPERATOR_HPP