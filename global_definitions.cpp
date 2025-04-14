#include "global_definitions.hpp"
#include "joiner.hpp"

boost::barrier pipeline_barrier(24);

extern "C" chunked_vec<node, NODE_CHUNK_SIZE>::range_iter *get_vec_begin(node_list *vec, size_t first, size_t last) {
    //pipeline_barrier.wait();

    return new chunked_vec<node, NODE_CHUNK_SIZE>::range_iter(vec->as_vec(), first, last);
}

extern "C" chunked_vec<node, NODE_CHUNK_SIZE>::range_iter *get_vec_next(chunked_vec<node, NODE_CHUNK_SIZE>::range_iter *it) {
    //std::cout << "Get next" << std::endl;
    return &it->operator++();;
}

extern "C" bool vec_end_reached(node_list &vec, chunked_vec<node, NODE_CHUNK_SIZE>::range_iter *it) {
    //std::cout << "test end" << std::endl;
    return !it->operator bool();
}

chunked_vec<relationship, RSHIP_CHUNK_SIZE>::iter get_vec_begin_r(relationship_list &vec) {
    return vec.as_vec().begin();
}

chunked_vec<relationship, RSHIP_CHUNK_SIZE>::iter *get_vec_next_r(chunked_vec<relationship, RSHIP_CHUNK_SIZE>::iter *it) {
    //std::cout << "Next called" << std::endl;
    //std::cout << "it get" << std::endl;
    return &it->operator++();
}

bool vec_end_reached_r(relationship_list &vec, chunked_vec<relationship, RSHIP_CHUNK_SIZE>::iter it) {
    //std::cout << "vec_end_reached: " << std::endl;
    return !(it != vec.as_vec().end());
}

extern "C" dcode_t dict_lookup_label(graph_db *gdb, char *label) {
    return gdb->get_code(label);
}

extern "C" node *get_node_from_it(chunked_vec<node, NODE_CHUNK_SIZE>::range_iter *it) {
    return &it->operator*();
}

extern "C" relationship *get_rship_from_it(chunked_vec<relationship, RSHIP_CHUNK_SIZE>::iter *it) {
    //std::cout << "get rship" << std::endl;
    return &it->operator*();
}

extern "C" chunked_vec<node, NODE_CHUNK_SIZE> *gdb_get_nodes(graph_db *gdb) {
    return &gdb->get_nodes()->as_vec();
}

extern "C" chunked_vec<relationship, RSHIP_CHUNK_SIZE> *gdb_get_rships(graph_db *gdb) {
    return &gdb->get_relationships()->as_vec();
}

extern "C" void test_ints(uint64_t a, uint64_t b) {
    std::cout << "A: " << a << " B: " << b << "EQ: " << (a == b) << std::endl;
}

extern "C" relationship *rship_by_id(graph_db *gdb, offset_t id) {
    return &gdb->rship_by_id(id);
}

extern "C" node *node_by_id(graph_db *gdb, offset_t id) {
    return &gdb->node_by_id(id);
}

extern "C" dcode_t gdb_get_dcode(graph_db *gdb, char *property) {
    //std::cout << "Search for property: " << property  << " with code: " << gdb->get_code(property) << std::endl;
    return gdb->get_code(property);
}

extern "C" const property_set *pset_get_item_at(graph_db *gdb, offset_t id) {
    //std::cout << "Get property at: " << id << std::endl;
    return &gdb->get_node_properties()->get(id);
}

thread_local std::map<int, std::string> str_result;
thread_local std::map<int, uint64_t> uint_result;
thread_local std::map<int, boost::posix_time::ptime> time_result;
thread_local int str_res_ctr = 0;

thread_local std::map<int, node_description> descs;
thread_local std::map<int, rship_description> rdescs;

std::map<int, std::function<std::string(graph_db*, int*)>> con_map;

extern "C" xid_t get_tx(transaction_ptr tx) {
    return tx->xid();
}

extern "C" node * get_valid_node(graph_db *gdb, node * n, transaction_ptr tx) {
    return &gdb->get_valid_node_version(*n, tx->xid());
}

extern "C" char* get_str_property(const properties_t &p, const std::string &key) {
    auto it = p.find(key);
    if (it == p.end()) {
        spdlog::info("unknown property: {}", key);
        throw unknown_property();
    }
    auto prop_heap = new std::string;
    *prop_heap = boost::any_cast<std::string>(it->second);
    auto ret = const_cast<char*>(reinterpret_cast<const char*>(prop_heap->c_str()));
    return ret;
}

void apply_pexpr_node(graph_db *gdb, const char *key, FTYPE val_type, int *qr, int *ret) {
    // cast the query result to a node
    auto n = (node*)qr;

    // try to find the description in the thread local result memory
    // if it is not present, generate the description and write it to the thread local memory
    if(descs.find(n->id()) == descs.end())
        descs[n->id()] = gdb->get_node_description(n->id());
    auto nd = descs[n->id()];

    switch(val_type) {
        case FTYPE::INT: { // an integer type can be directly written to the memory
            *ret = get_property<int>(nd.properties, key).value();
            break;
        }
        case FTYPE::UINT64: { // store the uint64 result in thread local memory and return the result id to the caller
            uint_result[str_res_ctr] = get_property<uint64_t>(nd.properties, key).value(); 
            *ret = str_res_ctr++;
            break;
        }
        case FTYPE::STRING: { // store the string in thread local memory and return the result id to the caller
            str_result[str_res_ctr] = boost::any_cast<std::string>(nd.properties[std::string(key)]);
            *ret = str_res_ctr++;
            break;
        }
        case FTYPE::TIME: // TODO: wip
        case FTYPE::DATE: { // store the time object in thread local memory and return the result id to the caller
            time_result[str_res_ctr] = get_property<boost::posix_time::ptime>(nd.properties, key).value();
            *ret = str_res_ctr++;
            break;
        }
        case FTYPE::DOUBLE:
        case FTYPE::BOOLEAN:
        default:
            break;   
    }
}

void apply_pexpr_rship(graph_db *gdb, const char *key, FTYPE val_type, int *qr, int *ret) {
    auto r = (relationship*)qr;
    if(rdescs.find(r->id()) == rdescs.end())
        rdescs[r->id()] = gdb->get_rship_description(r->id());
    auto rd = rdescs[r->id()];

    switch(val_type) {
        case FTYPE::INT: { // an integer type can be directly written to the memory
            *ret = get_property<int>(rd.properties, key).value();
            break;
        }
        case FTYPE::UINT64: { // store the uint64 result in thread local memory and return the result id to the caller
            break;
        }
        case FTYPE::STRING: { // store the string in thread local memory and return the result id to the caller
            str_result[str_res_ctr] = boost::any_cast<std::string>(rd.properties[std::string(key)]);;
            *ret = str_res_ctr++;
            break;
        }
        case FTYPE::TIME: // TODO: wip
        case FTYPE::DATE: { // store the time object in thread local memory and return the result id to the caller
            time_result[str_res_ctr] = get_property<boost::posix_time::ptime>(rd.properties, key).value();
            *ret = str_res_ctr++;
            break;
        }
        case FTYPE::DOUBLE:
        case FTYPE::BOOLEAN:
        default:
            break;   
    }
}

std::mutex prj_mutex;

extern "C" void apply_pexpr(graph_db *gdb, const char *key, FTYPE val_type, int *qr, int idx, std::vector<int> types, int *ret) {
    std::lock_guard<std::mutex> lock(prj_mutex);
    if(types.at(idx) == 0) { // is node
        apply_pexpr_node(gdb, key, val_type, qr, ret);

    } else if(types.at(idx) == 1) { // is rship
        apply_pexpr_rship(gdb, key, val_type, qr, ret);
    } else if(types.at(idx) == 2) { // is rship
        ret = qr;
    }
}

extern "C" const char* lookup_dc(graph_db *gdb, dcode_t dc) {
    return gdb->get_string(dc);
}

extern "C" node* create_node(graph_db *gdb, char *label, properties_t *props) {
    auto node_id = gdb->add_node(std::string(label), *props, true);
    return &gdb->node_by_id(node_id);
}

extern "C" relationship* create_rship(graph_db *gdb, char *label, node *n1, node *n2, properties_t *props) {
    auto rid = gdb->add_relationship(n1->id(), n2->id(), label, *props, true);
    return &gdb->rship_by_id(rid);
}

extern "C" void foreach_variable_from(graph_db *gdb, dcode_t label, int min, int max, consumer_fct_type consumer,
                                      int oid, int **qr, int *rs, int size, int *ty, int **call_map_arg, int offset) {
    auto prev_pos = size + offset;
    auto insert_pos = prev_pos + 1;
    auto n = (node*)qr[prev_pos];
    auto nsize = size++;
    *qr[0]++;
    gdb->foreach_variable_from_relationship_of_node(*n, label, min, max, [&](relationship &r) {
        qr[insert_pos] = (int*)&r;
        consumer(gdb, oid, qr, rs, nsize, ty, call_map_arg, offset);
    });
}

extern "C" void foreach_variable_to(graph_db *gdb, dcode_t label, int min, int max, consumer_fct_type consumer,
                                      int oid, int **qr, int *rs, int size, int *ty, int **call_map_arg, int offset) {
    auto prev_pos = size + offset;
    auto insert_pos = prev_pos + 1;
    auto n = (node*)qr[prev_pos];
    auto nsize = size++;
    *qr[0]++;
    gdb->foreach_variable_to_relationship_of_node(*n, label, min, max, [&](relationship &r) {
        qr[insert_pos] = (int*)&r;
        consumer(gdb, oid, qr, rs, nsize, ty, call_map_arg, offset);
    });
}

thread_local qr_tuple tp;

std::mutex mat_reg_mut;
extern "C" void mat_reg_value(graph_db *gdb, int *reg, int type) {
    std::lock_guard<std::mutex> lck(mat_reg_mut);
    if(type == 2) {
        int res = std::stoi(con_map[type](gdb, reg));
        tp.push_back(res);
    } else if(type == 5 || type == 6) {
        tp.push_back(time_result[*reg]);
    } else if(type == 8) {
        tp.push_back(uint64_t(std::stoull(con_map[type](gdb, reg))));
    } else {
        tp.push_back(con_map[type](gdb, reg));
    }
}

std::mutex ct_mut;
extern "C" void collect_tuple(result_set *rs, bool print) {
    std::lock_guard<std::mutex> lck(ct_mut);
    rs->data.push_back(tp);

    if(print) {
        std::cout << "{";
        auto my_visitor = boost::hana::overload(
            [&](node *n) { /*os << gdb->get_node_description(*n); */ },
            [&](relationship *r) { /* os << gdb->get_relationship_label(*r); */ },
            [&](int i) { std::cout << i; }, [&](double d) { std::cout << d; },
            [&](const std::string &s) { std::cout << s; }, [&](uint64_t ll) { std::cout << ll; },
            [&](null_t n) { std::cout << "NULL"; },
            [&](boost::posix_time::ptime dt) { std::cout << dt; }); 

        auto i = 0u;
        for (const auto &qr : tp) {
            boost::apply_visitor(my_visitor, qr);
            if (++i < tp.size())
                std::cout << ", ";
        }
        std::cout << "}" << std::endl;
    }
    tp.clear();
}

thread_local qr_tuple mat_tuple;

extern "C" qr_tuple *obtain_mat_tuple() {
    //auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    //return &joiner::mat_tuple_[tid];
    return &mat_tuple;
}

extern "C" void mat_node(node *n, qr_tuple *qr) {
    qr->push_back(n);
}

extern "C" void mat_rship(relationship *r, qr_tuple *qr) {
    qr->push_back(r);
}

extern "C" void collect_tuple_join(int jid, qr_tuple *qr) {
    joiner::materialize_rhs(jid, qr);
}

extern "C" qr_tuple *get_join_tp_at(int jid, int pos) {
    return &joiner::rhs_input_[jid].at(pos);
}

extern "C" node *get_node_res_at(qr_tuple *tuple, int pos) {
    return boost::get<node*>(tuple->at(pos));
}

extern "C" relationship *get_rship_res_at(qr_tuple *tuple, int pos) {
    return boost::get<relationship*>(tuple->at(pos));
}

extern "C" int get_mat_res_size(int jid) {
    return joiner::rhs_input_[jid].size();
}

extern "C" node *index_get_node(graph_db *gdb, char *label, char *prop, uint64_t value) {
    auto idx = gdb->get_index(std::string(label), std::string(prop));

    node *n_ptr;
    auto found = false;
    gdb->index_lookup(idx, value, [&](auto& n) {
        found = true;
        n_ptr = &n;
    });
    return n_ptr;
}

extern "C" int count_potential_o_hop(graph_db *gdb, offset_t rship_id) {
    auto cnt = 0;
    while(rship_id != UNKNOWN) {
        cnt++;
        rship_id = gdb->rship_by_id(rship_id).next_src_rship;
    }
    return cnt;
}


extern "C" std::list<std::pair<relationship::id_t, std::size_t>> retrieve_fev_queue() {
    return std::list<std::pair<relationship::id_t, std::size_t>>();
}

extern "C" void insert_fev_rship(std::list<std::pair<relationship::id_t, std::size_t>> &queue, relationship::id_t rid, std::size_t hop) {
    queue.push_back(std::make_pair(rid, hop));
}

extern "C" bool fev_queue_empty(std::list<std::pair<relationship::id_t, std::size_t>> &queue) {
    return queue.empty();
}

thread_local std::vector<relationship*> fev_rship_list;
thread_local std::vector<relationship*>::iterator fev_list_iter;

extern "C" void foreach_from_variable_rship(graph_db *gdb, dcode_t lcode, node *n, std::size_t min, std::size_t max) {
    gdb->foreach_variable_from_relationship_of_node(*n, lcode, min, max, [&](relationship &r) {
        fev_rship_list.push_back(&r);
    });
    fev_list_iter = fev_rship_list.begin();
}

 extern "C" relationship *get_next_rship_fev() {
    auto rship = *fev_list_iter;
    fev_list_iter++;
    return rship;
}

extern "C" bool fev_list_end() {
    auto is_end = fev_list_iter == fev_rship_list.end();
    if(is_end)
        fev_rship_list.clear();
    return is_end;
}
