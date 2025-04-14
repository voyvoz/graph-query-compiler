#ifndef POSEIDON_CORE_QUERY_ENGINE_HPP
#define POSEIDON_CORE_QUERY_ENGINE_HPP

#include "joiner.hpp"
#include "p_jit.hpp"
#include "p_context.hpp"

class base_op;


class query_engine;

struct compile_task {
    compile_task(query_engine & qeng, PContext &ctx, p_jit &jit, std::shared_ptr<base_op> query);

    void operator()();

    PContext &ctx_;
    p_jit &jit_;
    std::shared_ptr<base_op> query_;
    query_engine &qeng_;
};

struct arg_builder {
    std::vector<uint64_t> int_args;
    std::vector<std::string> string_args;
    std::vector<properties_t> prop_args;
    std::vector<uint64_t*> args;

    arg_builder() : int_args(64), string_args(64), prop_args(64), args(64) {}
    void arg(int op_id, std::string arg) {
        string_args[op_id] = arg;
        args[op_id] = (uint64_t*)(string_args[op_id].c_str());
    }
    void arg(int op_id, uint64_t arg) {
        int_args[op_id] = arg;
        args[op_id] = (uint64_t*)&(int_args[op_id]);
    }

    void arg(int op_id, properties_t & props) {
        prop_args[op_id] = props;
        args[op_id] = (uint64_t*)&(prop_args[op_id]);
    }

};

class query_engine {
    unsigned thread_num_;
    PContext ctx_;
    std::unique_ptr<p_jit> jit_;

    std::thread sched_th;
    std::thread compile_th;

    transaction_ptr cur_tx_;
    std::shared_ptr<base_op> cur_query_;
    arg_builder query_args;
    unsigned arg_counter;

    graph_db_ptr graph_;
public: 
    query_engine(graph_db_ptr graph, unsigned int thread_num, unsigned cv_range);
    ~query_engine();

    static std::unique_ptr<p_jit> initializeJitCompiler();

    void generate(std::shared_ptr<base_op> query, bool parallel = true);

    void prepare();

    void run(result_set * rs);
    void run(result_set * rs, std::vector<uint64_t*> args, bool cleanup_query = true);

    void run_parallel(result_set * rs, arg_builder & args, unsigned thread_num);

    void add_joiner(unsigned thread_id);

    void cleanup();

    void extract_arg(std::shared_ptr<base_op> op);

    bool parallel_;
    static int thread_iter_range;
    std::atomic<bool> compiled_;
    std::atomic<bool> complete_;
    std::map<int, start_ty> start_;

    std::map<int, std::vector<std::string>> operator_names_;
    std::map<int, std::vector<int>> type_vec_;
    static std::map<int, std::vector<consumer_fct_type>> operator_functions_;
    static std::map<int, finish_fct_type> finish_;


    std::function<void(transaction_ptr tx, graph_db *gdb, std::size_t first, std::size_t last, graph_db::node_consumer_func consumer)> task_callee_;

    std::vector<char*> strs;
};

#endif //POSEIDON_CORE_QUERY_ENGINE_HPP
