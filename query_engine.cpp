#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/InitLLVM.h>
#include <memory>
#include "query_engine.hpp"
#include "interpreter.hpp"
#include "qid_generator.hpp"
#include "codegen_inline.hpp"

using namespace std::placeholders;

std::unique_ptr<p_jit> query_engine::initializeJitCompiler() {
    InitLLVM LLVM();
    InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
    ExitOnError exitOnError;
    return std::make_unique<p_jit>(exitOnError);
}

query_engine::query_engine(graph_db_ptr graph, unsigned int thread_num, unsigned cv_range) 
    : thread_num_(thread_num), 
    ctx_(PContext(graph)), 
    jit_(initializeJitCompiler()), 
    arg_counter(1u),
    graph_(graph), 
    compiled_(false), 
    complete_(false) {

    ctx_.getModule().setDataLayout(jit_->getDataLayout());

    auto insert_nd = [&] (graph_db* gdb, int *ptr) -> std::string {
        auto n = (node*)ptr;
        return gdb->get_node_description(n->id()).to_string();
    };

    auto insert_rd = [&] (graph_db* gdb, int *ptr) -> std::string {
        auto r = (relationship*)ptr;
        return gdb->get_rship_description(r->id()).to_string();
    };

    auto insert_int = [&] (graph_db* gdb, int *ptr) -> std::string {
        return std::to_string(*ptr);
    };

    auto insert_str = [&] (graph_db* gdb, int *ptr) -> std::string {
        return str_result[*ptr];
    };

    auto insert_uint = [&] (graph_db* gdb, int *ptr) -> std::string {
        return std::to_string(uint_result[*ptr]);
    }; 

    con_map[0] = insert_nd;
    con_map[1] = insert_rd;
    con_map[2] = insert_int;
    con_map[4] = insert_str;
    con_map[8] = insert_uint;
}

query_engine::~query_engine() {
    if(compile_th.joinable())
        compile_th.join();
}

void query_engine::generate(std::shared_ptr<base_op> query, bool parallel) {
    parallel_ = parallel;
    cur_query_ = query;
    compile_th = std::thread(compile_task(*this, ctx_, *jit_.get(), query));
    if(!parallel) {
        compile_th.join();
    }
    /*codegen_visitor cv(ctx_);
    query->codegen(cv);
    jit.addModule(ctx_.moveModule());*/

}

using call_map = std::array<int*, 32>;

void query_engine::cleanup() {
    start_.clear();
    operator_names_.clear();
    operator_functions_.clear();
    finish_.clear();
    type_vec_.clear();
    complete_.store(false);
    compiled_.store(false);
    type_vec_.clear();
}

void query_engine::prepare() {
    for(auto i = 1u; i < start_.size(); i++) {
        type_vec_[i].insert(type_vec_[i].end(), type_vec_[i-1].begin(), type_vec_[i-1].end());
    }
}

void query_engine::extract_arg(std::shared_ptr<base_op> op) {
    switch(op->type_) {
        case qop_type::scan: {
            auto s_op = std::dynamic_pointer_cast<scan_op>(op);
            query_args.arg(arg_counter++, s_op->label_);
            if(s_op->indexed_) {
                    //TODO: 2nd index argument
            }
            break;
        }
        case qop_type::foreach_rship: {
            auto fe_op = std::dynamic_pointer_cast<foreach_rship_op>(op);
            query_args.arg(arg_counter++, fe_op->label_);
            break;
        }
        case qop_type::expand: {
            auto exp_op = std::dynamic_pointer_cast<expand_op>(op);
            query_args.arg(arg_counter++, exp_op->label_);
            break;
        }
        case qop_type::filter: {
            //TODO: all filter arguments
            break;
        }
        case qop_type::create: {
            //TODO: all create arguments
            break;
        }
        case qop_type::collect:
        case qop_type::cross_join:
        case qop_type::hash_join:
        case qop_type::left_join:
        case qop_type::limit:
        case qop_type::nest_loop_join:
        case qop_type::none:
        case qop_type::project:
        case qop_type::sort:
        case qop_type::any:
        default:
            return;
    }
}


void query_engine::run(result_set * rs, std::vector<uint64_t*> args, bool cleanup_query) {
    //prepare();

    auto tx = graph_->begin_transaction();
    current_transaction_ = tx;
    if(parallel_) {
        unsigned int op_start = 1;
        arg_builder args;
        interprete_visitor iv(graph_, args, rs);
        cur_query_->codegen(iv, op_start, true);
        iv.start();
        compile_th.join();
        std::cout << "NO COMPILED " << rs->data.size() << std::endl;
    }
    
    int i = 0;

    //std::vector<int> offsets;
    //offsets.resize(start_.size());
    //offsets[0] = type_vec_[0].size();

    //int offset = 0;
    auto start_idx = start_.size()-1;

    for(i = start_idx; i >= 0; i--) {
        start_[i](graph_.get(), 0, graph_->get_nodes()->num_chunks(), tx, 1, &type_vec_[start_idx], rs, nullptr, finish_[0], 0, args.data());
        //offset += offsets[i];
    }
    graph_->commit_transaction();


    if(cleanup_query)
        cleanup();
}

void query_engine::run(result_set * rs) {
    run(rs, query_args.args);
}

std::map<int, std::vector<consumer_fct_type>> query_engine::operator_functions_ = {};
std::map<int, finish_fct_type> query_engine::finish_ = {};

bool has_join(algebra_optr expr) {
    auto cur = expr;

    bool join_found = false;

    while(cur->inputs_.size() > 0) {
        if(cur->type_ == qop_type::cross_join ||
           cur->type_ == qop_type::left_join) {
            join_found = true;
            break;
        }
        cur = cur->inputs_[0];
    }

    return join_found;
}

void query_engine::run_parallel(result_set * rs, arg_builder & args, unsigned thread_num) {
    if(!has_join(cur_query_)) {
        interprete_visitor iv(graph_, args, rs);
        auto start = 1u;
        cur_query_->codegen(iv, start, false);
        std::thread t1([&] {
	    if(compile_th.joinable())
            	compile_th.join();
	
	    //prepare();
            task_callee_ = [&](transaction_ptr tx, graph_db *gdb, std::size_t first, std::size_t last, graph_db::node_consumer_func consumer) {
                current_transaction_ = tx;
                start_[0](gdb, first, last, tx, 1, &type_vec_[0], rs, nullptr, finish_[0], 0, args.args.data());
            };

            scan_task::callee_ = task_callee_;
            compiled_.store(true);
        });
        iv.start();
        t1.join();
    } else {
    auto chunksz = graph_->get_nodes()->num_chunks() / thread_num;
    auto max_chunksz = graph_->get_nodes()->num_chunks();
    if(compile_th.joinable())
        compile_th.join();
	//prepare();
	type_vec_[0].push_back(7);
        std::vector<std::thread> query_threads;
        auto cur_start = 0;
        auto end = chunksz - 1;
        auto tx = graph_->begin_transaction();
        for(auto i = 0u; i < thread_num; i++) {
            current_transaction_ = tx;
            if(i == thread_num - 1)
                end = max_chunksz;
            else
                end = cur_start + chunksz - 1;
            query_threads.push_back(std::thread([&, cur_start, end]{
                current_transaction_ = tx;
                start_[0](graph_.get(), cur_start, end, current_transaction_, 1, &type_vec_[0], rs, nullptr, finish_[0], 0, args.args.data());
            }));
            cur_start += chunksz;
        }

        for(auto & t : query_threads) {
            t.join();
        }
        graph_->commit_transaction();
    }
}


compile_task::compile_task(query_engine & qeng, PContext &ctx, p_jit &jit, std::shared_ptr<base_op> query) 
    : ctx_(ctx), jit_(jit), query_(query), qeng_(qeng) {}

int query_cnt = 0;

void compile_task::operator()() {
    //0. obtain the query identifier for the LLVM module
    qid_generator qd;
    unsigned int op_start = 1;
    unsigned int cg_start = 1;
    query_->codegen(qd, op_start, true);

    auto internal_qid = qd.qid+std::to_string(query_cnt);
    query_cnt++;

    //1. generate LLVM IR code & compile into machine code
    bool inlined = true;
    codegen_inline_visitor cv(ctx_, internal_qid);
    query_->codegen(cv, cg_start, inlined);

    //2. add LLVM module with IR to jit for compilation
    ctx_.getModule().setModuleIdentifier(internal_qid);
    jit_.get_exit()(jit_.addModule(ctx_.moveModule()));

    ctx_.createNewModule();
    ctx_.getModule().setDataLayout(jit_.getDataLayout());

    //3. extract all operator names from query and generate type vec
    //first function name == scan => start function
    auto query_id = 0;
    auto cur_op = query_;
    std::string finish_name = "default_finish_"+internal_qid;
    std::vector<int> type_vec;
    std::vector<algebra_optr> recur_list;
    while(!cur_op->inputs_.empty() || !recur_list.empty()) {
        qeng_.extract_arg(cur_op);
        if(cur_op->inputs_.empty()) {
            cur_op = recur_list.back();
            recur_list.pop_back();
            //query_id++;
        }

        if(cur_op->type_ == qop_type::sort) {
            finish_name = cur_op->name_;
            cur_op = cur_op->inputs_[0];
            continue;
        }
        if(cur_op->type_ == qop_type::cross_join || cur_op->type_ == qop_type::left_join) {
            recur_list.push_back(cur_op->inputs_[1]);
        }

        if(cur_op->produced_type_ != -1)
            qeng_.type_vec_[query_id].push_back(cur_op->produced_type_);

        if(!cur_op->name_.empty())
            qeng_.operator_names_[query_id].push_back(cur_op->name_);
        cur_op = cur_op->inputs_[0];
    }
    if(cur_op->type_ == qop_type::none && !cur_op->name_.empty()) {
        qeng_.operator_names_[query_id].push_back(cur_op->name_);
    }
    query_id = 0;

    //3. get all generated function pointers
    for(int q = 0; q <= query_id; q++) {
        for(auto i = 1u; i < qeng_.operator_names_[q].size(); i++) {
            auto op_name = qeng_.operator_names_[q].at(i);
            auto fc = jit_.getFunctionRaw<consumer_fct_type>(op_name);
            if(fc)
                query_engine::operator_functions_[q].push_back(*fc);
        }
    }

    for(int q = 0; q <= query_id; q++) {
        auto finish_fc = jit_.getFunctionRaw<finish_fct_type>(finish_name);
        if(finish_fc)
            qeng_.finish_[q] = *finish_fc;

        auto start_fc = jit_.getFunctionRaw<start_ty>(qeng_.operator_names_[q].at(0));
        if(start_fc)
            qeng_.start_[q] = *start_fc;
    }
}
