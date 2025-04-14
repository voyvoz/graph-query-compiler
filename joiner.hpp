#ifndef POSEIDON_CORE_JOINER_HPP
#define POSEIDON_CORE_JOINER_HPP

#include <vector>
#include "global_definitions.hpp"
using input = std::vector<qr_tuple>;

class joiner {
public:
    joiner();

    static std::map<int, qr_tuple> mat_tuple_;
    static std::map<int, input> rhs_input_;

    static std::mutex materialize_mutex;
    static void materialize_rhs(int jid, qr_tuple *qr);
};


#endif //POSEIDON_CORE_JOINER_HPP
