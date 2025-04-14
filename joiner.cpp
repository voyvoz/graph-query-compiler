#include "joiner.hpp"

joiner::joiner() {

}

std::map<int, qr_tuple> joiner::mat_tuple_ = {};
std::map<int, input> joiner::rhs_input_ = {};
std::mutex joiner::materialize_mutex;

void joiner::materialize_rhs(int jid, qr_tuple *qr) {
    std::lock_guard<std::mutex> lck(materialize_mutex);
    rhs_input_[jid].push_back(*qr);
    qr->clear();
}