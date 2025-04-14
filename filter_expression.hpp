#ifndef ART_FILTER_EXPRESSION_HPP
#define ART_FILTER_EXPRESSION_HPP

#include <string>
#include <memory>
#include <iostream>
#include "p_context.hpp"
#include "filter_visitor.hpp"


enum class FOP {
    EQ = 0,
    NEQ = 1,
    LE = 2,
    LT = 3,
    GE = 4,
    GT = 5,
    AND = 6,
    OR = 7,
    NOT = 8
};

enum class FOP_TYPE {
    INT = 0,
    DOUBLE = 1,
    STRING = 2,
    DATE = 3,
    TIME = 4,
    OP = 5,
    BOOL_OP = 6,
    KEY = 7,
    UINT64 = 8
};

struct expression;
struct binary_predicate;
struct fep_visitor;
struct number_token;
struct key_token;
struct str_token;
struct fct_call;
struct eq_predicate;
struct and_predicate;
struct or_predicate;
using expr = std::shared_ptr<expression>;
using bin_expr = std::shared_ptr<binary_predicate>;

struct expression_visitor {
protected:
    expression_visitor() = default;

public:
    virtual ~expression_visitor() = default;

    virtual void visit(int rank, std::shared_ptr<number_token> op) = 0;

    virtual void visit(int rank, std::shared_ptr<key_token> op) = 0;

    virtual void visit(int rank, std::shared_ptr<str_token> op) = 0;

    virtual void visit(int rank, std::shared_ptr<fct_call> op) = 0;

    virtual void visit(int rank, std::shared_ptr<eq_predicate> op) = 0;

    virtual void visit(int rank, std::shared_ptr<and_predicate> op) = 0;

    virtual void visit(int rank, std::shared_ptr<or_predicate> op) = 0;
};

struct expression {
    int opd_num;
    std::string name_;
    FOP_TYPE ftype_;

    virtual ~expression() {};

    virtual std::string operator()() const = 0;

    virtual void accept(int rank, expression_visitor &vis) = 0;

    std::string fop_str(FOP fop) const;
};

struct number_token : public expression, public std::enable_shared_from_this<number_token> {
    int value_;

    number_token(const int value = 0);

    std::string operator()() const override;

    void accept(int rank, expression_visitor &fep) override;
};

inline expr Int(const int value = 0) { return std::make_shared<number_token>(value); }


struct key_token : public expression, std::enable_shared_from_this<key_token> {
    std::string key_;
    unsigned qr_id_; 

    key_token(unsigned qr_id, std::string key);

    std::string operator()() const override;

    void accept(int rank, expression_visitor &fep) override;
};

inline expr Key(unsigned qr_id, std::string value = 0) { return std::make_shared<key_token>(qr_id, value); }

struct str_token : public expression, std::enable_shared_from_this<str_token> {
    std::string str_;

    str_token(std::string str);

    std::string operator()() const override;

    void accept(int rank, expression_visitor &fep) override;
};

inline expr Str(std::string value = 0) { return std::make_shared<str_token>(value); }

struct fct_call : public expression, std::enable_shared_from_this<fct_call> {
    using fct_t = bool (*)(int*);  
    fct_t fct_;

    fct_call(fct_t fct);

    std::string operator()() const override;

    void accept(int rank, expression_visitor &fep) override;
};

inline expr Fct(fct_call::fct_t fct) { return std::make_shared<fct_call>(fct); }

struct binary_predicate : public expression {
    expr left_;
    expr right_;
    FOP fop_;
    bool is_bool_;
    bool prec_;

    binary_predicate(FOP fop, expr const left = 0, expr const right = 0, bool prec = 0, bool is_bool = 0);

    std::string operator()() const override;
};

struct eq_predicate : public binary_predicate, std::enable_shared_from_this<eq_predicate> {
    eq_predicate(expr const left, expr const right, bool prec, bool not_ = false);

    void accept(int rank, expression_visitor &fep) override;
};

inline bin_expr EQ(expr lhs, expr rhs, bool prec = 0) { return std::make_shared<eq_predicate>(lhs, rhs, prec); }

struct and_predicate : public binary_predicate, std::enable_shared_from_this<and_predicate> {
    and_predicate(bin_expr const left, bin_expr const right, bool prec);

    void accept(int rank, expression_visitor &fep) override;
};

inline bin_expr AND(bin_expr lhs, bin_expr rhs, bool prec = 0) {
    return std::make_shared<and_predicate>(lhs, rhs, prec);
}


struct or_predicate : public binary_predicate, std::enable_shared_from_this<or_predicate> {
    or_predicate(bin_expr const left, bin_expr const right, bool prec);

    void accept(int rank, expression_visitor &fep) override;

};

inline bin_expr OR(bin_expr lhs, bin_expr rhs, bool prec = 0) { return std::make_shared<or_predicate>(lhs, rhs, prec); }

struct grexpr {
    expr lhs;
    bin_expr op;
    expr rhs;
};


struct fep_visitor : public expression_visitor {
    Value *roi_;
    Value *type_;
    int opd_cnt = 0;

    fep_visitor(PContext *ctx, Function *parent, Value *qr_node, BasicBlock *next, BasicBlock *end);

    void visit(int rank, std::shared_ptr<number_token> num) override;

    void visit(int rank, std::shared_ptr<key_token> key) override;

    void visit(int rank, std::shared_ptr<str_token> str) override;

    void visit(int rank, std::shared_ptr<fct_call> fct) override;

    void visit(int rank, std::shared_ptr<eq_predicate> eq) override;

    void visit(int rank, std::shared_ptr<and_predicate> andpr) override;

    void visit(int rank, std::shared_ptr<or_predicate> orpr) override;

    Value *alloc(std::string name, Type *type, Value *val = nullptr);

    BasicBlock *add_bb(std::string name);

    PContext *ctx_;
    Function *fct_;

    Value *qr_node_;
    BasicBlock *next_;
    BasicBlock *end_;
    std::vector<expr> expr_stack;

    std::map<std::string, Value *> alloc_stack;
    std::map<int, Value *> gen_vals_;
    std::map<std::string, BasicBlock *> bbs_;

    std::vector<AllocaInst *> keys;

};


#endif //ART_FILTER_EXPRESSION_HPP
