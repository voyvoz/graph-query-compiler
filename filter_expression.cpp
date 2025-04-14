#include "filter_expression.hpp"

std::string expression::fop_str(FOP fop) const {
    switch (fop) {
        case FOP::EQ:
            return "=";
        case FOP::NEQ:
            return "!=";
        case FOP::LE:
            return "<=";
        case FOP::LT:
            return "<";
        case FOP::GE:
            return ">=";
        case FOP::GT:
            return "<";
        case FOP::AND:
            return "&&";
        case FOP::OR:
            return "||";
        case FOP::NOT:
            return "!";
    }
}

number_token::number_token(const int value) : value_(value) {
    name_ = "INT";
    ftype_ = FOP_TYPE::INT;
}

std::string number_token::operator()() const {
    return std::to_string(value_);
}

void number_token::accept(int rank, expression_visitor &fep) {
    fep.visit(rank, shared_from_this());
}

key_token::key_token(unsigned qr_id, std::string key) : qr_id_(qr_id), key_(key) {
    name_ = "KEY";
    ftype_ = FOP_TYPE::KEY;
}

std::string key_token::operator()() const {
    return key_;
}

void key_token::accept(int rank, expression_visitor &fep) {
    fep.visit(rank, shared_from_this());
}

str_token::str_token(std::string str) : str_(str) {
    name_ = "STR";
    ftype_ = FOP_TYPE::STRING;
}

std::string str_token::operator()() const {
    return str_;
}

void str_token::accept(int rank, expression_visitor &fep) {
    fep.visit(rank, shared_from_this());
}

fct_call::fct_call(fct_t fct) : fct_(fct) {
    name_ = "FCT";
}

void fct_call::accept(int rank, expression_visitor &fep) {
    fep.visit(rank, shared_from_this());
}

std::string fct_call::operator()() const {
    return "";
}

binary_predicate::binary_predicate(FOP fop, const expr left, const expr right, bool prec, bool is_bool)
        : left_(left), right_(right), fop_(fop), prec_(prec), is_bool_(is_bool) {}

std::string binary_predicate::operator()() const {
    auto lhs = (*left_)();
    auto rhs = (*right_)();
    auto op = fop_str(fop_);
    auto pro = prec_ ? "(" : "";
    auto epi = prec_ ? ")" : "";
    return pro + lhs + op + rhs + epi;
}

eq_predicate::eq_predicate(const expr left, const expr right, bool prec, bool not_)
        : binary_predicate(FOP::EQ, left, right, prec) {
    name_ = "EQ";
    ftype_ = FOP_TYPE::OP;
    if (not_) fop_ = FOP::NEQ;
}

void eq_predicate::accept(int rank, expression_visitor &fep) {
    left_->accept(rank+1, fep);
    right_->accept(rank+1, fep);
    fep.visit(rank, shared_from_this());
    // TODO: do binary stuff here
}

and_predicate::and_predicate(const bin_expr left, const bin_expr right, bool prec)
        : binary_predicate(FOP::AND, left, right, prec, true) {
    ftype_ = FOP_TYPE::BOOL_OP;
    name_ = "AND";
}

void and_predicate::accept(int rank, expression_visitor &fep) {
    left_->accept(rank+1, fep);
    right_->accept(rank+1, fep);
    fep.visit(rank, shared_from_this());
    // TODO: do binary stuff here
}

or_predicate::or_predicate(const bin_expr left, const bin_expr right, bool prec)
        : binary_predicate(FOP::OR, left, right, prec, true) {
    ftype_ = FOP_TYPE::BOOL_OP;
    name_ = "OR";
}

void or_predicate::accept(int rank, expression_visitor &fep) {
    left_->accept(rank+1, fep);
    right_->accept(rank+1, fep);
    fep.visit(rank, shared_from_this());
    // TODO: do binary stuff here
}

fep_visitor::fep_visitor(PContext *ctx, Function *parent, Value *qr_node, BasicBlock *next, BasicBlock *end)
        : ctx_(ctx),
          fct_(parent),
          qr_node_(qr_node),
          next_(next),
          end_(end) {
    //assert(qr_node->getType() == ctx_->qrResultPtrTy && "qr_node must be query result node alloca!");

    //roi_ = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateStructGEP(qr_node, 0));
    //type_ = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateStructGEP(qr_node, 1));
    roi_ = qr_node;
}

void fep_visitor::visit(int rank, std::shared_ptr<number_token> num) {
    num->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), num);

    gen_vals_[opd_cnt] = alloc("int_" + std::to_string(num->value_), ctx_->int64Ty,
                               ConstantInt::get(ctx_->int64Ty, num->value_));
    opd_cnt++;
}

void fep_visitor::visit(int rank, std::shared_ptr<key_token> key)  {
    key->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), key);
    auto spitem = add_bb("search_pitem_" + key->key_);
    ctx_->getBuilder().CreateBr(spitem);
    ctx_->getBuilder().SetInsertPoint(spitem);

    auto dc = ctx_->get_dcode(key->key_);
    auto pcode = ConstantInt::get(ctx_->int32Ty, APInt(32, dc));
    auto rhs_unknown = alloc("rhs_unknown", ctx_->int64Ty, ctx_->UNKNOWN_ID);

    auto is_node = add_bb("is_node_" + std::to_string(opd_cnt));
    auto is_rship = add_bb("is_rship_" + std::to_string(opd_cnt));
    auto for_each_pitem = add_bb("for_each_pitem_" + std::to_string(opd_cnt));
    auto for_each_increment = add_bb("for_each_increment" + std::to_string(opd_cnt));
    auto pitem_found = add_bb("pitem_found" + std::to_string(opd_cnt));
    auto next_prop_it = add_bb("next_prop_it" + std::to_string(opd_cnt));
    //auto cmp_type = ctx_->getBuilder().CreateICmpEQ(type_, ctx_->LLVM_ZERO, "cmp_type");
    ctx_->getBuilder().CreateBr(is_node);
    //ctx_->getBuilder().CreateCondBr(cmp_type, is_node, is_rship);

    ctx_->getBuilder().SetInsertPoint(is_rship);
    ctx_->getBuilder().CreateRet(nullptr);

    ctx_->getBuilder().SetInsertPoint(is_node);
    auto cur_item_arr = ctx_->getBuilder().CreateAlloca(ctx_->pSetRawArrTy);
    auto cur_item = ctx_->getBuilder().CreateAlloca(ctx_->pitemTy);
    auto cur_pset = ctx_->getBuilder().CreateAlloca(ctx_->propertySetPtrTy);

    auto res_node = ctx_->getBuilder().CreateBitCast(roi_, ctx_->nodePtrTy);
    auto plist_id = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateStructGEP(res_node, 4));
    auto plist_id_alloc = alloc("roi_plist_alloc", ctx_->int64Ty, plist_id);

    auto loop_cnt = ctx_->getBuilder().CreateAlloca(ctx_->int64Ty);

    auto loop_body = ctx_->while_loop_condition(fct_, plist_id_alloc, rhs_unknown, ctx_->WHILE_COND::LT, end_,
                                                [&](BasicBlock *body, BasicBlock *epilog) {
                                                    auto pid = ctx_->getBuilder().CreateLoad(plist_id_alloc);
                                                    auto pset = ctx_->getBuilder().CreateCall(
                                                            ctx_->extern_func("pset_get_item_at"),
                                                            {fct_->args().begin(), pid});
                                                    ctx_->getBuilder().CreateStore(pset, cur_pset);
                                                    auto pitems = ctx_->getBuilder().CreateStructGEP(pset, 2);
                                                    auto item_arr = ctx_->getBuilder().CreateLoad(
                                                            ctx_->getBuilder().CreateStructGEP(pitems, 0));
                                                    ctx_->getBuilder().CreateStore(item_arr, cur_item_arr);
                                                    ctx_->getBuilder().CreateStore(ctx_->LLVM_ZERO, loop_cnt);
                                                    ctx_->getBuilder().CreateBr(for_each_pitem);
                                                });

    ctx_->getBuilder().SetInsertPoint(for_each_pitem);
    auto max_cnt_alloc = ctx_->getBuilder().CreateAlloca(ctx_->int64Ty);
    ctx_->getBuilder().CreateStore(ctx_->MAX_PITEM_CNT, max_cnt_alloc);
    auto foreach_pitem_body = ctx_->while_loop_condition(fct_, loop_cnt, max_cnt_alloc, ctx_->WHILE_COND::LT,
                                                         next_prop_it, [&](BasicBlock *body, BasicBlock *epilog) {
                auto idx = ctx_->getBuilder().CreateLoad(loop_cnt);
                auto item = ctx_->getBuilder().CreateInBoundsGEP(cur_item_arr, {ctx_->LLVM_ZERO, idx});
                ctx_->getBuilder().CreateStore(ctx_->getBuilder().CreateLoad(item), cur_item);
                auto item_key = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateStructGEP(item, 1));
                auto key_cmp = ctx_->getBuilder().CreateICmp(CmpInst::ICMP_EQ, item_key, pcode, "item.key_eq_pkey");
                ctx_->getBuilder().CreateCondBr(key_cmp, pitem_found, for_each_increment);
            });

    ctx_->getBuilder().SetInsertPoint(for_each_increment);
    {
        auto idx = ctx_->getBuilder().CreateLoad(loop_cnt);
        auto idxpp = ctx_->getBuilder().CreateAdd(idx, ctx_->LLVM_ONE);
        ctx_->getBuilder().CreateStore(idxpp, loop_cnt);
        ctx_->getBuilder().CreateBr(foreach_pitem_body);
    }

    ctx_->getBuilder().SetInsertPoint(next_prop_it);
    {
// get next prop -> GEP
        auto p_set = ctx_->getBuilder().CreateLoad(cur_pset);
        auto next_pset_id = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateStructGEP(p_set, 0));
        ctx_->getBuilder().CreateStore(next_pset_id, plist_id_alloc);
        ctx_->getBuilder().CreateBr(loop_body);
    }

    ctx_->getBuilder().SetInsertPoint(pitem_found);
    auto pitem = ctx_->getBuilder().CreateLoad(cur_item);
    gen_vals_[opd_cnt] = alloc("pitem_" + key->key_, ctx_->pitemTy, pitem);

    auto epilog = add_bb("epilog_" + std::to_string(opd_cnt));
    ctx_->getBuilder().CreateBr(epilog);
    ctx_->getBuilder().SetInsertPoint(epilog);
    opd_cnt++;
}

void fep_visitor::visit(int rank, std::shared_ptr<str_token> str) {
    str->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), str);
    auto dcode = ctx_->get_dcode(str->str_);
    gen_vals_[opd_cnt] = alloc("int_" + std::to_string(dcode), ctx_->int64Ty, ConstantInt::get(ctx_->int64Ty, dcode));

    auto epilog = add_bb("epilog_" + std::to_string(opd_cnt));
    ctx_->getBuilder().CreateBr(epilog);
    ctx_->getBuilder().SetInsertPoint(epilog);
    opd_cnt++;
}

void fep_visitor::visit(int rank, std::shared_ptr<fct_call> fct) {
    fct->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), fct);

    auto fct_raw = ConstantInt::get(ctx_->int64Ty, (int64_t )fct->fct_);
    auto fct_ptr = ctx_->getBuilder().CreateIntToPtr(fct_raw, ctx_->int64PtrTy);
    auto fct_callee_type = FunctionType::get(ctx_->boolTy, {ctx_->int64PtrTy}, false);
    auto fct_callee = ctx_->getBuilder().CreateBitCast(fct_ptr, fct_callee_type->getPointerTo());

    auto res = ctx_->getBuilder().CreateCall(fct_callee_type, fct_callee, {roi_});

    ctx_->getBuilder().CreateCondBr(res, next_, end_);
}

void fep_visitor::visit(int rank, std::shared_ptr<eq_predicate> eq)  {
    eq->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), eq);

    auto opd_it = expr_stack.begin();
    auto rhs_it = expr_stack.begin() + 1;
    auto lhs_it = expr_stack.begin() + 2;

    auto cmp_alloc = add_bb("cmp_eq_" + std::to_string(eq->opd_num));
    ctx_->getBuilder().CreateBr(cmp_alloc);
    ctx_->getBuilder().SetInsertPoint(cmp_alloc);
    auto pitem = gen_vals_[(*lhs_it)->opd_num];
    auto rhs_alloc = gen_vals_[(*rhs_it)->opd_num];

    auto value_arr = ctx_->getBuilder().CreateStructGEP(pitem, 0);
    switch ((*rhs_it)->ftype_) {
        case FOP_TYPE::INT: {
            auto val_rhs = ctx_->getBuilder().CreateLoad(rhs_alloc);
            auto int_value = ctx_->getBuilder().CreateLoad(
                    ctx_->getBuilder().CreateBitCast(value_arr, ctx_->int64PtrTy));
            auto cmp_pitem = ctx_->getBuilder().CreateICmpEQ(int_value, val_rhs);
            gen_vals_[opd_cnt] = alloc("cmp_eq_res_" + std::to_string(eq->opd_num), ctx_->boolTy, cmp_pitem);
        }
            break;
        case FOP_TYPE::DOUBLE: {
            auto val_rhs = ctx_->getBuilder().CreateLoad(rhs_alloc);
            auto int_value = ctx_->getBuilder().CreateLoad(ctx_->getBuilder().CreateBitCast(value_arr, ctx_->doubleTy));
            auto cmp_pitem = ctx_->getBuilder().CreateFCmpOEQ(int_value, val_rhs);
            gen_vals_[opd_cnt] = alloc("cmp_eq_res_" + std::to_string(eq->opd_num), ctx_->boolTy, cmp_pitem);
        }

            break;
    }

    auto epilog = add_bb("epilog_" + std::to_string(opd_cnt));

    if (rank == 0) {
        auto ptr = gen_vals_[opd_cnt];
        auto cond = ctx_->getBuilder().CreateLoad(ptr);
        ctx_->getBuilder().CreateCondBr(cond, next_, end_);
    } else {
        ctx_->getBuilder().CreateBr(epilog);
        ctx_->getBuilder().SetInsertPoint(epilog);
    }
    opd_cnt++;
}

void fep_visitor::visit(int rank, std::shared_ptr<and_predicate> andpr) {
    andpr->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), andpr);

    auto bopd_it = expr_stack.begin();
    auto rhs_it = expr_stack.begin() + 1;
    auto cit = expr_stack.begin() + 1;
    auto offset = 0;

    while ((*cit)->ftype_ == FOP_TYPE::BOOL_OP) {
        offset++;
        cit--;
    }

    auto rhs_pos = std::pow(2, 2 + offset);
    auto lhs_it = expr_stack.begin() + rhs_pos;

    auto cmp_or_bb = add_bb("cmp_and_" + std::to_string(andpr->opd_num));
    ctx_->getBuilder().CreateBr(cmp_or_bb);
    ctx_->getBuilder().SetInsertPoint(cmp_or_bb);
    auto lhs = ctx_->getBuilder().CreateLoad(gen_vals_[(*lhs_it)->opd_num]);
    auto rval = gen_vals_[(*rhs_it)->opd_num];
    auto rhs = ctx_->getBuilder().CreateLoad(rval);
    auto cmp_eq = ctx_->getBuilder().CreateAnd(lhs, rhs);
    gen_vals_[opd_cnt] = alloc("cmp_and_res_" + std::to_string(andpr->opd_num), ctx_->boolTy, cmp_eq);

    auto epilog = add_bb("epilog_" + std::to_string(opd_cnt));

    if (rank == 0) {
        auto ptr = gen_vals_[opd_cnt];
        auto cond = ctx_->getBuilder().CreateLoad(ptr);
        ctx_->getBuilder().CreateCondBr(cond, next_, end_);
    } else {
        ctx_->getBuilder().CreateBr(epilog);
        ctx_->getBuilder().SetInsertPoint(epilog);
    }
    opd_cnt++;
}

void fep_visitor::visit(int rank, std::shared_ptr<or_predicate> orpr) {
    orpr->opd_num = opd_cnt;
    expr_stack.insert(expr_stack.begin(), orpr);

    auto bopd_it = expr_stack.begin();
    auto rhs_it = expr_stack.begin() + 1;
    auto cit = expr_stack.begin() + 1;
    auto offset = 0;

    while ((*cit)->ftype_ == FOP_TYPE::BOOL_OP) {
        offset++;
        cit--;
    }

    auto rhs_pos = std::pow(2, 2 + offset);
    auto lhs_it = expr_stack.begin() + rhs_pos;

    auto cmp_and_bb = add_bb("cmp_or_" + std::to_string(orpr->opd_num));
    ctx_->getBuilder().CreateBr(cmp_and_bb);
    ctx_->getBuilder().SetInsertPoint(cmp_and_bb);
    auto lhs = ctx_->getBuilder().CreateLoad(gen_vals_[(*lhs_it)->opd_num]);
    auto rhs = ctx_->getBuilder().CreateLoad(gen_vals_[(*rhs_it)->opd_num]);
    auto cmp_eq = ctx_->getBuilder().CreateOr(lhs, rhs);
    gen_vals_[opd_cnt] = alloc("cmp_or_res_" + std::to_string(orpr->opd_num), ctx_->boolTy, cmp_eq);

    auto epilog = add_bb("epilog_" + std::to_string(opd_cnt));
    if (rank == 0) {
        auto ptr = gen_vals_[opd_cnt];
        auto cond = ctx_->getBuilder().CreateLoad(ptr);
        ctx_->getBuilder().CreateCondBr(cond, next_, end_);
    } else {
        ctx_->getBuilder().CreateBr(epilog);
        ctx_->getBuilder().SetInsertPoint(epilog);
    }
    opd_cnt++;
}

Value * fep_visitor::alloc(std::string name, Type *type, Value *val) {
    if (alloc_stack.find(name) == alloc_stack.end()) {
        alloc_stack[name] = ctx_->getBuilder().CreateAlloca(type);
        if (val) {
            ctx_->getBuilder().CreateStore(val, alloc_stack[name]);
        }
    }
    return alloc_stack[name];
}

BasicBlock * fep_visitor::add_bb(std::string name) {
    bbs_[name] = BasicBlock::Create(ctx_->getModule().getContext(), name, fct_);
    return bbs_[name];
}