#include <set>
#include <iostream>
#include <boost/variant.hpp>

#include "config.h"
#include "graph_db.hpp"
#include "graph_pool.hpp"

#include "qoperator.hpp"
#include "queryc.hpp"
#include "query.hpp"

const std::string test_path = poseidon::gPmemPath + "jit_qcomp";

#ifdef USE_PMDK

#define PMEMOBJ_POOL_SIZE ((unsigned long long)(1024 * 1024 * 40000ull))

namespace nvm = pmem::obj;

nvm::pool_base prepare_pool() {
	nvm::pool_base pop;
	if (access(test_path.c_str(), F_OK) != 0) {
    	pop = nvm::pool_base::create(test_path, "jit_qcomp", PMEMOBJ_POOL_SIZE);
  	} else {
    	pop = nvm::pool_base::open(test_path, "jit_qcomp");
  	}

	//auto pop = nvm::pool_base::create(test_path, "", PMEMOBJ_POOL_SIZE);
	return pop;
}
#endif

int main() {
#ifdef USE_PMDK
	auto pop = prepare_pool();
	graph_db_ptr graph;
	nvm::transaction::run(pop, [&] { graph = p_make_ptr<graph_db>(); });
#else
  auto pool = graph_pool::create(test_path);
  auto graph = pool->create_graph("jit_qcomp");
#endif


	auto tx = graph->begin_transaction();

	int PERSONS = 100;
	int NO_PERSONS = 42;
	for (int i = 0; i < PERSONS; i++) {
		auto p = graph->add_node("Person",
				{{"name", boost::any(std::string("John Doe")+std::to_string(i))},
				{"age", boost::any(42)},
				{"id", boost::any(i)},
				{"num", boost::any(uint64_t(1234567890123412)+uint64_t(i))},
				{"dummy1", boost::any(std::string("Dummy"))},
				{"dummy2", boost::any(1.2345)}},
				false);
		auto b = graph->add_node("Book",
				{{"title", boost::any(std::string("Title"))},
				{"Age", boost::any(42)},
				{"id", boost::any(i)}},
				false);
		auto x = graph->add_relationship(p, b, ":HAS_READ", {}, false);
		auto y = graph->add_relationship(b, p, ":HAS_READ", {}, false);
	}

	graph->commit_transaction();

	auto THREAD_NUM = 4;
	auto chunks = graph->get_nodes()->num_chunks();
	auto cv_range = chunks / THREAD_NUM;

	query_engine queryEngine(graph, THREAD_NUM, cv_range);

	p_ptr<dict> dct;
#ifdef USE_PMDK
	nvm::transaction::run(pop, [&] {
#endif
		dct = p_make_ptr<dict>();
#ifdef USE_PMDK
	});
#endif

    queryc qlc;
	
	//algebra_optr op = qlc.compile_to_plan("Project([$0.name:string, $0.num:uint64], NodeScan('Person'))");

	auto r_expr = Scan("Book", End());

    auto l_expr = Scan("Person", Join(JOIN_OP::LEFT_OUTER, {0, 0}, 
                        Project({{0, "name", FTYPE::STRING}, {0, "age", FTYPE::INT}, {0, "num", FTYPE::UINT64}
                                  /*{3, "title", FTYPE::STRING}, {3, "Age", FTYPE::INT}, {0, "id", FTYPE::INT}, {0, "name", FTYPE::STRING}*/}, 
							Collect()), r_expr));

	auto sort_fct = [&](const qr_tuple &q1, const qr_tuple &q2) -> bool {
		return boost::get<uint64_t>(q1[2]) < boost::get<uint64_t>(q2[2]); 
	};

	auto fev = Scan("Person", Collect());
	scan_task::callee_ = &scan_task::scan;	
	queryEngine.generate(fev, true);
	
	arg_builder ab;
	ab.arg(1, "Person");
	ab.arg(2, ":HAS_READ");
	ab.arg(3, "Book");
	ab.arg(4, "Book");
	ab.arg(5, ":HAS_READ");
	ab.arg(6, "Person");

	result_set rs;

  	auto js = std::chrono::steady_clock::now();
	queryEngine.run(&rs, ab.args);
	auto je = std::chrono::steady_clock::now();

	std::cout << rs.data.size() << std::endl;
	  std::cout << "JIT: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(je -
                                                                     js)
                   .count()
            << " ms" << std::endl;

#ifdef USE_PMDK
	//nvm::transaction::run(pop, [&] { nvm::delete_persistent<graph_db>(graph); });
	pop.close();
	//remove(test_path.c_str());
#endif
	return 0;
}
