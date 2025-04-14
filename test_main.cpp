#include <iostream>

#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"

#include "config.h"
#include "graph_pool.hpp"

#include "p_context.hpp"
#include "JitFromScratch.hpp"
#include "qoperator.hpp"
#include "interpreter.hpp"

#include "qop.hpp"
#include "query.hpp"

using namespace llvm;

graph_db_ptr graph1();
graph_db_ptr graph2();

double calc_avg(std::vector<double> &rt) {
    return std::accumulate(rt.begin(), rt.end(), 0.0) / rt.size();
}

double internal_is1(graph_db_ptr graph) {

    std::vector<uint64_t> personIds =
            {26388279115622, 2199023280088, 19791209323074, 15393162816482, 15393162855246,
             19791209363124, 28587302347895, 10995116305049, 4398046515646, 2199023282192,
             30786325588998, 2199023301366, 19791209333857, 24189255835727, 26388279139007,
             24189255855187, 17592186088378, 6597069782503, 32985348854773, 21990232576546,
             26388279080215, 21990232627526, 15393162855372, 2199023261717, 10995116348358,
             6597069771464, 17592186084059, 15393162854260, 4398046543803, 71623, 32985348891012,
             6597069786049, 30786325626045, 8796093025994, 8796093041362, 13194139568647,
             28587302324409, 10995116329599, 17592186068239, 30786325621884, 13194139605452,
             4398046511596, 13194139597106, 13194139540307, 32985348862047, 28587302352600,
             17592186070889, 8796093026236, 17592186094267, 32985348870938, 21990232600629,
             2199023283094};
    std::vector<double> runtimes(personIds.size());
    result_set rs;
    for(auto i = 0u; i < personIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Person")
                .property("id",
                          [&](auto &c) { return c.equal(personIds[i]); })
                .from_relationships(":isLocatedIn")
                .to_node("Place")
                .project({PExpr_(0, builtin::string_property(res, "firstName")),
                          PExpr_(0, builtin::string_property(res, "lastName")),
                          PExpr_(0, builtin::pr_date(res, "birthday")),
                          PExpr_(0, builtin::string_property(res, "locationIP")),
                          PExpr_(0, builtin::string_property(res, "browserUsed")),
                          PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(0, builtin::string_property(res, "gender")),
                          PExpr_(0, builtin::ptime_property(res, "creationDate")) })
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is2(graph_db_ptr graph) {

    std::vector<uint64_t> personIds =
            {10995116338469, 21990232591571, 19791209356794, 13194139598683, 68301,
             4398046529914, 21990232626534, 4398046537991, 10995116305964, 30786325620353,
             17592186073576, 58850, 8796093024620, 19791209367505, 2199023275285, 17592186110043,
             10995116287120, 13628, 4398046565656, 6597069772434, 13194139576699, 4398046549865,
             13194139533618, 6597069828622, 15393162790221, 2199023292961, 21990232588012,
             19791209359083, 26388279077904, 15393162809380, 8796093033086, 4398046562129,
             4398046568995, 34565, 6597069822324, 19791209342401, 17592186056186, 15393162814685,
             10995116293981, 15393162815221, 30786325632464, 8796093035558, 26388279073187,
             21990232584928, 4398046512001, 4398046563886, 6597069820569, 13194139565644,
             17592186099787, 10995116282803, 15393162838932, 6597069825699};

    std::vector<double> runtimes(personIds.size());
    result_set rs;
    for(auto i = 0u; i < personIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Person")
                .property( "id",
                           [&](auto &p) { return p.equal(personIds[i]); })
                .to_relationships(":hasCreator")
                .from_node("Post")
                .project({PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(2, !builtin::string_property(res, "content").empty() ?
                                    builtin::string_property(res, "content") : builtin::string_property(res, "imageFile")),
                          PExpr_(2, builtin::ptime_property(res, "creationDate")),
                          PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(0, builtin::uint64_property(res, "id")),
                          PExpr_(0, builtin::string_property(res, "firstName")),
                          PExpr_(0, builtin::string_property(res, "lastName")) })
                .orderby([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                    if (boost::get<boost::posix_time::ptime>(qr1[2]) == boost::get<boost::posix_time::ptime>(qr2[2]))
                        return boost::get<uint64_t>(qr1[0]) > boost::get<uint64_t>(qr2[0]);
                    return boost::get<boost::posix_time::ptime>(qr1[2]) > boost::get<boost::posix_time::ptime>(qr2[2]); })
                .limit(10)
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is3(graph_db_ptr graph) {

    std::vector<uint64_t> personIds =
            {2199023269673, 2199023326667, 10995116286165, 4398046514225, 2199023268992,
             17592186092889, 2199023304089, 36915, 15393162822969, 10995116295447,
             2199023272508, 13194139546280, 62739, 10995116303770, 2199023281253,
             15393162792543, 10995116308879, 4398046579349, 13194139594779, 21990232589465,
             38953, 4398046581131, 6597069804810, 4398046535151, 15393162825201, 13194139576334,
             4398046544758, 45360, 16163, 19791209356202, 10995116299448, 2199023317693,
             13194139576572, 15393162802523, 15393162840727, 15393162814789, 18103, 4831,
             30786325627738, 6597069803008, 6597069769310, 13194139597867, 2199023271926,
             2199023281226, 4398046518021, 9598, 13194139585832, 10995116280587, 10995116328660,
             28587302392195, 4398046558180, 42321};

    std::vector<double> runtimes(personIds.size());
    result_set rs;
    for(auto i = 0u; i < personIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Person")
                .property( "id",
                           [&](auto &p) { return p.equal(personIds[i]); })
                .to_relationships(":hasCreator")
                .from_node("Post")
                .project({PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(2, !builtin::string_property(res, "content").empty() ?
                                    builtin::string_property(res, "content") : builtin::string_property(res, "imageFile")),
                          PExpr_(2, builtin::ptime_property(res, "creationDate")),
                          PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(0, builtin::uint64_property(res, "id")),
                          PExpr_(0, builtin::string_property(res, "firstName")),
                          PExpr_(0, builtin::string_property(res, "lastName")) })
                .orderby([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                    if (boost::get<boost::posix_time::ptime>(qr1[2]) == boost::get<boost::posix_time::ptime>(qr2[2]))
                        return boost::get<uint64_t>(qr1[0]) > boost::get<uint64_t>(qr2[0]);
                    return boost::get<boost::posix_time::ptime>(qr1[2]) > boost::get<boost::posix_time::ptime>(qr2[2]); })
                .limit(10)
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is4(graph_db_ptr graph) {

    std::vector<uint64_t> personIds =
            { 4398067251028,  2199043946640,  2199037876061,  6597090750692, 4398075704871,
              7696589408397,  3298550269571,  4398061999667,  7696621883870,  7696591812715,
              2748817450668,  5497571035089,  4398055319228, 8246355344067,  8246362564011,
              1649294789710,  7146856770100,  3848300633506,  8246347114898,  5497580138363,
              7146845964022,  7146861667978,  7146850822113,  7146851976510,  7146847600481,
              7146833549023,  7146835916936,  2199031335329,  4947834453085,  1649269718480,
              2748802751725,  2748809026462,  7146849576015,  6597073593366,  6047349141655,
              3848325558344,  5497560701738,  6597099501373,  7146837110089,  5497569013304,
              8246358234322,  549765328557,  5497595205987,  3848298086950,  2199040160941,
              7696586176598,  8246365612544,  3298562064542,  7146853212564, 8246366297574,
              4947814411455,  7146847681997};

    std::vector<double> runtimes(personIds.size());
    result_set rs;
    for(auto i = 0u; i < personIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Post")
                .property( "id",
                           [&](auto &p) { return p.equal(personIds[i]); })
                .project({PExpr_(0, builtin::ptime_property(res, "creationDate")),
                          PExpr_(0, !builtin::string_property(res, "content").empty() ?
                                    builtin::string_property(res, "content") : builtin::string_property(res, "imageFile")) })
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is5(graph_db_ptr graph) {

    std::vector<uint64_t> commentIds =
            {4398085711669, 5497581697867, 8796113056132, 3298560907184, 4947822939393,
             4398070249301, 3848312245560, 1649279381402, 1649272063101, 7696601345746,
             1649270862758, 2199043244454, 3848330137206, 3848320714563, 7696599764981,
             4398049076779, 3848331029634, 4947830627212, 8246345917057, 4398048176041,
             3298555146483, 8246355037778, 3298554625423, 3848299782253, 6597072683829,
             7146838886436, 4947839031063, 7146830729841, 4398085377321, 7146848658314,
             6597080192231, 8246349659145, 4947810290408, 1099551056576, 6597095818102,
             3298544696882, 1099518992300, 4398056527889, 8246365341325, 7146831228474,
             6597103467760, 2748786865138, 7146845163780, 2199036369137, 3848292238383,
             6047353276351, 5497561930650, 6047316621496, 3848291103926, 2199025590666,
             4398048989441, 3298543737354};

    std::vector<double> runtimes(commentIds.size());
    result_set rs;
    for(auto i = 0u; i < commentIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Comment")
                .property( "id",
                           [&](auto &p) { return p.equal(commentIds[i]); })
                .from_relationships(":hasCreator")
                .to_node("Person")
                .project({PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(2, builtin::string_property(res, "firstName")),
                          PExpr_(2, builtin::string_property(res, "lastName")) })
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is6(graph_db_ptr graph) {

    std::vector<uint64_t> postIds =
            {6047348850997, 5497564365221, 3848329967558, 4398047599412, 6597105769994,
             4947821105898, 7696607613655, 5497597471779, 5497565124879, 7696587911380,
             7696592464558, 6047339895007, 7146837287657, 7696592802214, 6047333509576,
             6047318254791, 3298568940677, 4947820585024, 8246339385108, 3298562096579,
             7696617491509, 5497569775062, 8246366523162, 3298574334235, 4947820310842,
             4398085624920, 7696594106627, 5497583929848, 8246345580501, 8796110519993,
             1649283295282, 2199051023689, 6047318362421, 6047314584628, 7696589261990,
             6047344893312, 3298550621323, 6597092944738, 6047321024153, 4398054812559,
             4398071972522, 3848293901086, 8246355392461, 2748800645630, 1649267888051,
             7146857571848, 6597093766004, 4947837595380, 8246344422919, 7696611410177,
             5497583687275, 2748794674509};

    std::vector<double> runtimes(postIds.size());
    result_set rs;
    for(auto i = 0u; i < postIds.size(); i++) {
        auto q = query(graph)
                .all_nodes("Post")
                .property( "id",
                           [&](auto &p) { return p.equal(postIds[i]); })
                .to_relationships(":containerOf")
                .from_node("Forum")
                .from_relationships(":hasModerator")
                .to_node("Person")
                .project({PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(2, builtin::string_property(res, "title")),
                          PExpr_(4, builtin::uint64_property(res, "id")),
                          PExpr_(4, builtin::string_property(res, "firstName")),
                          PExpr_(4, builtin::string_property(res, "lastName")) })
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        q.start();
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}

double internal_is7(graph_db_ptr graph) {

    std::vector<uint64_t> postIds =
            {7146846240480, 5497578410380, 5497572128778, 3298557786474, 3298564869730,
             6597085241772, 5497562407626, 7146865899861, 2199063215958, 3298543129796,
             7696585937397, 3848303485145, 7696584725469, 8246372882931, 4624321,
             7696598517932, 6597097715505, 5497574002790, 2748783314629, 4947806953871,
             2199027045720, 8246351169549, 2199045162022, 4398062238469, 3298555317270,
             1649290760770, 3848327767348, 8246366635007, 8246346947901, 1099532289463,
             3848300985206, 5497581299875, 5497579358934, 2199063595159, 7696607737987,
             6047353121515, 7696590312905, 5497593006914, 6597089610781, 7146832026578,
             7696595067279, 1099545270694, 5497564138318, 1099537597905, 6047325524027,
             3298545496742, 5497572456680, 3848302106259, 2748794743559, 7146846003719,
             8246346437108, 7696607465071};

    std::vector<double> runtimes(postIds.size());
    result_set rs;
    for(auto i = 0u; i < postIds.size(); i++) {
        auto q1 = query(graph)
                .property( "id",
                           [&](auto &p) { return p.equal(postIds[i]); })
                .from_relationships(":hasCreator")
                .to_node("Person");

        auto q2 = query(graph)
                .all_nodes("Comment")
                .property( "id",
                           [&](auto &p) { return p.equal(postIds[i]); })
                .to_relationships(":replyOf")
                .from_node("Comment")
                .from_relationships(":hasCreator")
                .to_node("Person")
                .outerjoin({4, 2}, q1)
                .project({PExpr_(2, builtin::uint64_property(res, "id")),
                          PExpr_(2, builtin::string_property(res, "content")),
                          PExpr_(2, builtin::ptime_property(res, "creationDate")),
                          PExpr_(4, builtin::uint64_property(res, "id")),
                          PExpr_(4, builtin::string_property(res, "firstName")),
                          PExpr_(4, builtin::string_property(res, "lastName")),
                          PExpr_(8, builtin::string_rep(res) == "[0]{}" ?
                                    std::string("false") : std::string("true")) })
                .orderby([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                    if (boost::get<boost::posix_time::ptime>(qr1[2]) == boost::get<boost::posix_time::ptime>(qr2[2]))
                        return boost::get<uint64_t>(qr1[0]) > boost::get<uint64_t>(qr2[0]);
                    return boost::get<boost::posix_time::ptime>(qr1[2]) < boost::get<boost::posix_time::ptime>(qr2[2]); })
                .collect(rs);

        auto t1 = std::chrono::system_clock::now();
        query::start({&q1, &q2});
        auto t2 = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        runtimes[i] = duration.count();
    }
    return calc_avg(runtimes);
}


algebra_optr create_is1_query();
algebra_optr create_is2_1_query();
algebra_optr create_is2_2_query();
algebra_optr create_is3_query();
algebra_optr create_is4_query();
algebra_optr create_is5_query();
algebra_optr create_is6_1_query();
algebra_optr create_is6_2_query();
algebra_optr create_is7_1_query();

algebra_optr create_iu1_query();
algebra_optr create_iu2_query();
algebra_optr create_iu3_query();
algebra_optr create_iu4_query();
algebra_optr create_iu5_query();
algebra_optr create_iu6_query();
algebra_optr create_iu8_query();


arg_builder create_args_q1() {
    arg_builder qargs;
    qargs.arg(1, "Person");
    qargs.arg(2, 933);
    qargs.arg(3, ":isLocatedIn");
    qargs.arg(4, "Place");
    return qargs;
}

arg_builder create_args_q2_1() {
    arg_builder qargs;
    qargs.arg(1, "Person");
    qargs.arg(2, 65);
    qargs.arg(3, ":hasCreator");
    qargs.arg(5, "Post");
    return qargs;
}

arg_builder create_args_q2_2() {
    arg_builder qargs;
    qargs.arg(1, "Person");
    qargs.arg(2, 65);
    qargs.arg(3, ":hasCreator");
    qargs.arg(4, 10);
    qargs.arg(5, "Post");
    return qargs;
}

arg_builder create_args_q3() {
    arg_builder qargs;
    qargs.arg(1, "Person");
    qargs.arg(2, 65);
    qargs.arg(3, ":KNOWS");
    qargs.arg(4, "Person");
    return qargs;
}

arg_builder create_args_q4() {
    arg_builder qargs;
    qargs.arg(1, "Post");
    qargs.arg(2, 65);
    return qargs;
}

arg_builder create_args_q5() {
    arg_builder qargs;
    qargs.arg(1, "Comment");
    qargs.arg(2, 65);
    qargs.arg(3, ":hasCreator");
    return qargs;
}

arg_builder create_args_q6_1() {
    arg_builder qargs;
    qargs.arg(1, "Post");
    qargs.arg(2, 65);
    qargs.arg(3, ":containerOf");
    qargs.arg(4, "Forum");
    qargs.arg(5, ":hasModerator");
    qargs.arg(6, "Person");
    return qargs;
}

arg_builder create_args_q6_2() {
    arg_builder qargs;
    qargs.arg(1, "Post");
    qargs.arg(2, 65);
    qargs.arg(3, ":replyOf");
    qargs.arg(4, "Post");
    qargs.arg(5, ":containerOf");
    qargs.arg(6, "Forum");
    qargs.arg(7, ":hasModerator");
    qargs.arg(8, "Person");
    return qargs;
}

arg_builder create_args_q7() {
    arg_builder qargs;
    qargs.arg(1, "Comment");
    qargs.arg(2, 16492676);
    qargs.arg(3, ":replyOf");
    qargs.arg(4, "Comment");
    qargs.arg(5, ":hasCreator");
    qargs.arg(6, "Person");
    qargs.arg(8, "Comment");
    qargs.arg(9, 16492676);
    qargs.arg(10, ":hasCreator");
    qargs.arg(11, "Person");
    return qargs;
}

std::pair<double,double> run_ldbc_is1(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> personIds =
    {26388279115622, 933, 2199023280088, 19791209323074, 15393162816482, 15393162855246,
        19791209363124, 28587302347895, 10995116305049, 4398046515646, 2199023282192,
        30786325588998, 2199023301366, 19791209333857, 24189255835727, 26388279139007,
        24189255855187, 17592186088378, 6597069782503, 32985348854773, 21990232576546,
        26388279080215, 21990232627526, 15393162855372, 2199023261717, 10995116348358,
        6597069771464, 17592186084059, 15393162854260, 4398046543803, 71623, 32985348891012,
        6597069786049, 30786325626045, 8796093025994, 8796093041362, 13194139568647,
        28587302324409, 10995116329599, 17592186068239, 30786325621884, 13194139605452,
        4398046511596, 13194139597106, 13194139540307, 32985348862047, 28587302352600,
        17592186070889, 8796093026236, 17592186094267, 32985348870938, 21990232600629,
        2199023283094};

    std::vector<double> runtimes(personIds.size());

    auto expr = create_is1_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
    jit_start).count();

    for(auto i = 0u; i < personIds.size(); i++) {
        result_set rs;
        auto args = create_args_q1();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], personIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is2_1(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> personIds =
            {10995116338469, 21990232591571, 19791209356794, 13194139598683, 68301,
             4398046529914, 21990232626534, 4398046537991, 10995116305964, 30786325620353,
             17592186073576, 58850, 8796093024620, 19791209367505, 2199023275285, 17592186110043,
             10995116287120, 13628, 4398046565656, 6597069772434, 13194139576699, 4398046549865,
             13194139533618, 6597069828622, 15393162790221, 2199023292961, 21990232588012,
             19791209359083, 26388279077904, 15393162809380, 8796093033086, 4398046562129,
             4398046568995, 34565, 6597069822324, 19791209342401, 17592186056186, 15393162814685,
             10995116293981, 15393162815221, 30786325632464, 8796093035558, 26388279073187,
             21990232584928, 4398046512001, 4398046563886, 6597069820569, 13194139565644,
             17592186099787, 10995116282803, 15393162838932, 6597069825699};

    std::vector<double> runtimes(personIds.size());

    auto expr = create_is2_1_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < personIds.size(); i++) {
        result_set rs;
        auto args = create_args_q2_1();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], personIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is2_2(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> personIds =
            {10995116338469, 21990232591571, 19791209356794, 13194139598683, 68301,
             4398046529914, 21990232626534, 4398046537991, 10995116305964, 30786325620353,
             17592186073576, 58850, 8796093024620, 19791209367505, 2199023275285, 17592186110043,
             10995116287120, 13628, 4398046565656, 6597069772434, 13194139576699, 4398046549865,
             13194139533618, 6597069828622, 15393162790221, 2199023292961, 21990232588012,
             19791209359083, 26388279077904, 15393162809380, 8796093033086, 4398046562129,
             4398046568995, 34565, 6597069822324, 19791209342401, 17592186056186, 15393162814685,
             10995116293981, 15393162815221, 30786325632464, 8796093035558, 26388279073187,
             21990232584928, 4398046512001, 4398046563886, 6597069820569, 13194139565644,
             17592186099787, 10995116282803, 15393162838932, 6597069825699};

    std::vector<double> runtimes(personIds.size());

    auto expr = create_is2_2_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < personIds.size(); i++) {
        result_set rs;
        auto args = create_args_q2_2();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], personIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is3(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> personIds =
            {2199023269673, 2199023326667, 10995116286165, 4398046514225, 2199023268992,
             17592186092889, 2199023304089, 36915, 15393162822969, 10995116295447,
             2199023272508, 13194139546280, 62739, 10995116303770, 2199023281253,
             15393162792543, 10995116308879, 4398046579349, 13194139594779, 21990232589465,
             38953, 4398046581131, 6597069804810, 4398046535151, 15393162825201, 13194139576334,
             4398046544758, 45360, 16163, 19791209356202, 10995116299448, 2199023317693,
             13194139576572, 15393162802523, 15393162840727, 15393162814789, 18103, 4831,
             30786325627738, 6597069803008, 6597069769310, 13194139597867, 2199023271926,
             2199023281226, 4398046518021, 9598, 13194139585832, 10995116280587, 10995116328660,
             28587302392195, 4398046558180, 42321};

    std::vector<double> runtimes(personIds.size());

    auto expr = create_is3_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < personIds.size(); i++) {
        result_set rs;
        auto args = create_args_q3();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], personIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is4(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> commentIds =
            {7146837074657,  6597103956627,  3848301802195,  7146837499403,  5497571658687,
             7146859489686,  5497570252406,  3848303243210,  5497572320002,  5497579913092,
             2199023675683,  3298567083043,  3298545915339, 6047340077013,  2748810494285,
             3848304697939,  2748793590070,  7146836614405,  8246374073295,  5497575170470,
             7696615613480,  6047342625473, 7146840311129,  5497592913303,  3848302626541,
             549763541252, 7696594987644,  7696616449636,  1649275891208,  8246351688273,
             6597091918954,  7696618491904,  7146831583655,  5497569640572,  6597077682727,
             5497567744687,  4398049179630,  4398073102203,  7146860736359, 4947832362234,
             4398067346974,  7146840849688,  4398083584372,  7146840360913,  3848306595564,
             6047319356807, 8796112244667,  7146849550683,  6047352098252,  7696611407007,
             7146833587239,  5497569176501};

    std::vector<double> runtimes(commentIds.size());

    auto expr = create_is4_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < commentIds.size(); i++) {
        result_set rs;
        auto args = create_args_q4();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], commentIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is5(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> commentIds =
            {4398085711669, 5497581697867, 8796113056132, 3298560907184, 4947822939393,
             4398070249301, 3848312245560, 1649279381402, 1649272063101, 7696601345746,
             1649270862758, 2199043244454, 3848330137206, 3848320714563, 7696599764981,
             4398049076779, 3848331029634, 4947830627212, 8246345917057, 4398048176041,
             3298555146483, 8246355037778, 3298554625423, 3848299782253, 6597072683829,
             7146838886436, 4947839031063, 7146830729841, 4398085377321, 7146848658314,
             6597080192231, 8246349659145, 4947810290408, 1099551056576, 6597095818102,
             3298544696882, 1099518992300, 4398056527889, 8246365341325, 7146831228474,
             6597103467760, 2748786865138, 7146845163780, 2199036369137, 3848292238383,
             6047353276351, 5497561930650, 6047316621496, 3848291103926, 2199025590666,
             4398048989441, 3298543737354};

    std::vector<double> runtimes(commentIds.size());

    auto expr = create_is5_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < commentIds.size(); i++) {
        result_set rs;
        auto args = create_args_q5();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], commentIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is6_1(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> commentIds =
            {7146848797209, 6047342252109, 8246377297601, 1099513403506, 1649297650520,
             8246341221876, 8246367373329, 8246342269850, 8246340778158, 6047344491668,
             8796115187591, 7696619489960, 1099521487215, 6047326425751, 8246351033697,
             8246365196867, 8246343099694, 6047350744524, 5497593227720, 2748790628283,
             2748802395151, 3298557085552, 7696621423568, 7696613855335, 8246359053718,
             7146844090065, 8246352454414, 5497577976061, 4947829453022, 6597092286565,
             7146831749672, 6597075028453, 8246340918420, 3848318579211, 6597085643326,
             3298542288861, 7696613107298, 6597087895623, 6597088834693, 8246358246770,
             7696610350084, 7696589652492, 3298570627344, 8246352772973, 8246347750453,
             8246342872094, 6597093391156, 7146827520774, 4947816404504, 7146854504754,
             8246373280377, 8246367726351};

    std::vector<double> runtimes(commentIds.size());

    auto expr = create_is6_1_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < commentIds.size(); i++) {
        result_set rs;
        auto args = create_args_q6_1();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], commentIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is6_2(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> commentIds =
            {7146848797209, 6047342252109, 8246377297601, 1099513403506, 1649297650520,
             8246341221876, 8246367373329, 8246342269850, 8246340778158, 6047344491668,
             8796115187591, 7696619489960, 1099521487215, 6047326425751, 8246351033697,
             8246365196867, 8246343099694, 6047350744524, 5497593227720, 2748790628283,
             2748802395151, 3298557085552, 7696621423568, 7696613855335, 8246359053718,
             7146844090065, 8246352454414, 5497577976061, 4947829453022, 6597092286565,
             7146831749672, 6597075028453, 8246340918420, 3848318579211, 6597085643326,
             3298542288861, 7696613107298, 6597087895623, 6597088834693, 8246358246770,
             7696610350084, 7696589652492, 3298570627344, 8246352772973, 8246347750453,
             8246342872094, 6597093391156, 7146827520774, 4947816404504, 7146854504754,
             8246373280377, 8246367726351};

    std::vector<double> runtimes(commentIds.size());

    auto expr = create_is6_2_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < commentIds.size(); i++) {
        result_set rs;
        auto args = create_args_q6_2();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], commentIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}

std::pair<double,double> run_ldbc_is7(JitFromScratch &jit, graph_db_ptr gdb, query_engine & qeng) {
    std::vector<uint64_t> commentIds =
            {7146846240480, 5497578410380, 5497572128778, 3298557786474, 3298564869730,
             6597085241772, 5497562407626, 7146865899861, 2199063215958, 3298543129796,
             7696585937397, 3848303485145, 7696584725469, 8246372882931, 4624321,
             7696598517932, 6597097715505, 5497574002790, 2748783314629, 4947806953871,
             2199027045720, 8246351169549, 2199045162022, 4398062238469, 3298555317270,
             1649290760770, 3848327767348, 8246366635007, 8246346947901, 1099532289463,
             3848300985206, 5497581299875, 5497579358934, 2199063595159, 7696607737987,
             6047353121515, 7696590312905, 5497593006914, 6597089610781, 7146832026578,
             7696595067279, 1099545270694, 5497564138318, 1099537597905, 6047325524027,
             3298545496742, 5497572456680, 3848302106259, 2748794743559, 7146846003719,
             8246346437108, 7696607465071};

    std::vector<double> runtimes(commentIds.size());

    auto expr = create_is7_1_query();

    auto jit_start = std::chrono::system_clock::now();
    qeng.generate(jit, expr, false);
    auto jit_end = std::chrono::system_clock::now();
    auto comp = std::chrono::duration_cast<std::chrono::microseconds>(jit_end -
                                                                      jit_start).count();

    for(auto i = 0u; i < commentIds.size(); i++) {
        result_set rs;
        auto args = create_args_q7();
        qeng.prepare(jit, gdb);
        auto start_qp = std::chrono::steady_clock::now();

        auto tx = gdb->begin_transaction();
        qeng.start_[0](gdb.get(), 0, gdb->get_nodes()->num_chunks(), tx, 1, &qeng.type_vec_[0], &rs, nullptr, qeng.finish_[0], commentIds[i], args.args.data());
        gdb->commit_transaction();

        auto end_qp = std::chrono::steady_clock::now();
        runtimes[i] = std::chrono::duration_cast<std::chrono::microseconds>(end_qp -
                                                                            start_qp).count();
        std::cout << rs << std::endl;
        joiner::rhs_input_.clear();
    }
    qeng.cleanup();
    return {comp, calc_avg(runtimes)};
}


void run_benchmarks(JitFromScratch & Jit, graph_db_ptr graph, query_engine & queryEngine) {
    auto is1 = run_ldbc_is1(Jit, graph, queryEngine);
    std::cout << "IS1 compilation: " << is1.first << " runtime: " << is1.second << std::endl;
    auto is2_1 = run_ldbc_is2_1(Jit, graph, queryEngine);
    std::cout << "IS2_1 compilation: " << is2_1.first << " runtime: " << is2_1.second << std::endl;
    auto is2_2 = run_ldbc_is2_2(Jit, graph, queryEngine);
    std::cout << "IS2_2 compilation: " << is2_2.first << " runtime: " << is2_2.second << std::endl;
    auto is3 = run_ldbc_is3(Jit, graph, queryEngine);
    std::cout << "IS3 compilation: " << is3.first << " runtime: " << is3.second << std::endl;
    auto is4 = run_ldbc_is4(Jit, graph, queryEngine);
    std::cout << "IS4 compilation: " << is4.first << " runtime: " << is4.second << std::endl;
    auto is5 = run_ldbc_is5(Jit, graph, queryEngine);
    std::cout << "IS5 compilation: " << is5.first << " runtime: " << is5.second << std::endl;
    auto is6_1 = run_ldbc_is6_1(Jit, graph, queryEngine);
    std::cout << "IS6_1 compilation: " << is6_1.first << " runtime: " << is6_1.second << std::endl;
    auto is6_2 = run_ldbc_is6_2(Jit, graph, queryEngine);
    std::cout << "IS6_2 compilation: " << is6_2.first << " runtime: " << is6_2.second << std::endl;
    auto is7_1 = run_ldbc_is7(Jit, graph, queryEngine);
    std::cout << "IS7_1 compilation: " << is7_1.first << " runtime: " << is7_1.second << std::endl;
}


int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv);

    InitLLVM X(argc, argv);
    ExitOnError exitOnError;
    exitOnError.setBanner("JIT");
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto graph = graph1();
    Collector::gdb = graph.get();

    PContext ctx(graph);
    JitFromScratch Jit(exitOnError);
    ctx.getModule().setDataLayout(Jit.getDataLayout());

    auto THREAD_NUM = 4;
    auto chunks = graph->get_nodes()->num_chunks();
    auto cv_range = chunks / THREAD_NUM;

    query_engine queryEngine(ctx, THREAD_NUM, cv_range);


    auto expr = create_is7_1_query();

    auto t1 = std::chrono::system_clock::now();
    queryEngine.generate(Jit, expr, false);
    auto t2 = std::chrono::system_clock::now();

    queryEngine.prepare(Jit, graph);

    std::vector<uint64_t> ids = {7146846240480, 16492676, 16492676, 16492676, 16492676123, 16492676, 7146846240480, 16492676, 16492676, 16492676, 7146846240480, 16492676, 16492676};


    std::vector<uint64_t> runtimes;

    for(auto & x : ids) {
        result_set rs;
        arg_builder qargs;
        qargs.arg(1, "Comment");
        qargs.arg(2, x);
        std::cout << "1ADDR IS:" << *(uint64_t*)(qargs.args[2]) << std::endl;
        qargs.arg(3, ":replyOf");
        qargs.arg(4, "Comment");
        qargs.arg(5, ":hasCreator");
        qargs.arg(6, "Person");
        qargs.arg(8, "Comment");
        qargs.arg(9, x);
        std::cout << "2ADDR IS:" << *(uint64_t*)(qargs.args[2]) << std::endl;
        qargs.arg(10, ":hasCreator");
        qargs.arg(11, "Person");

        auto tj1 = std::chrono::system_clock::now();
        auto tx = graph->begin_transaction();
        queryEngine.start_[0](graph.get(), 0, graph->get_nodes()->num_chunks(), tx, 1, &queryEngine.type_vec_[0], &rs, nullptr, queryEngine.finish_[0], x, qargs.args.data());
        graph->commit_transaction();
        auto tj2 = std::chrono::system_clock::now();
        auto jduration = std::chrono::duration_cast<std::chrono::microseconds>(tj2 - tj1);
        runtimes.push_back(jduration.count());
        printf("Total JIT time: %ld\n", jduration.count());
        std::cout << rs << std::endl;
        rs.data.clear();
        joiner::rhs_input_.clear();
    }
    queryEngine.cleanup();
    run_benchmarks(Jit, graph, queryEngine);

    std::cout << "START INTERNAL" << std::endl;
    result_set r;
    interprete_visitor iv(graph);
    //expr->codegen(iv, 1, true);
    auto t7 = std::chrono::system_clock::now();
    auto tx = graph->begin_transaction();
    scan_task::callee_ = &scan_task::scan;
namespace pj = builtin;

    auto commentId = 16492676;

    auto q1 = query(graph)
            .all_nodes("Comment")
            .property( "id",
                         [&](auto &c) { return c.equal(commentId); })
            .from_relationships(":hasCreator")
            .to_node("Person");

    auto q2 = query(graph)
            .all_nodes("Comment")
            .property("id",
                         [&](auto &c) { return c.equal(commentId); })
            .to_relationships(":replyOf")
            .from_node("Comment")
            .from_relationships(":hasCreator")
            .to_node("Person")
            .outerjoin({4, 2}, q1)
            .project({PExpr_(2, pj::int_property(res, "id")),
                      PExpr_(2, pj::string_property(res, "content")),
                      PExpr_(2, pj::int_to_dtimestring(pj::int_property(res, "creationDate"))),
                      PExpr_(4, pj::int_property(res, "id")),
                      PExpr_(4, pj::string_property(res, "firstName")),
                      PExpr_(4, pj::string_property(res, "lastName")),
                      PExpr_(8, res.type() == typeid(rship_description) ?
                                std::string("true") : std::string("false")) })
            .orderby([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                auto t1 = pj::dtimestring_to_int(boost::get<std::string>(qr1[2]));
                auto t2 = pj::dtimestring_to_int(boost::get<std::string>(qr2[2]));
                if(t1 == t2)
                    return boost::get<int>(qr1[3]) < boost::get<int>(qr2[3]);
                return t1 > t2; })
            .collect(r);

    query::start({&q1, &q2});
    graph->commit_transaction();

    auto t8 = std::chrono::system_clock::now();
    //std::cout << r<< std::endl;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    //printf("Total code generation time: %d\n", duration.count());
    duration = std::chrono::duration_cast<std::chrono::microseconds>(t8 - t7);
    printf("Total internal time: %d\n", duration.count());
    return 0;
}

graph_db_ptr graph1() {
    auto pool = graph_pool::create("/mnt/pmem0/jit_ldbc/is1", 1024*1024*900);
    auto graph = pool->create_graph("my_graph");
    auto gdb = graph.get();

    auto tx = graph->begin_transaction();
    for(int i = 0; i < 1; i++) {
        auto mahinda = graph->add_node(
                // id|firstName|lastName|gender|birthday|creationDate|locationIP|browserUsed
                // 933|Mahinda|Perera|male|1989-12-03|2010-02-14T15:32:10.447+0000|119.235.7.103|Firefox
                "Person",
                {{"id",         boost::any(933)},
                 {"firstName",  boost::any(std::string("Mahinda"))},
                 {"lastName",   boost::any(std::string("Perera"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1989-12-03"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-02-14 15:32:10.447"))},
                 {"locationIP", boost::any(std::string("119.235.7.103"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto meera = graph->add_node(
                // 10027|Meera|Rao|female|1982-12-08|2010-01-22T19:59:59.221+0000|49.249.98.96|Firefox
                "Person",
                {{"id",         boost::any(10027)},
                 {"firstName",  boost::any(std::string("Meera"))},
                 {"lastName",   boost::any(std::string("Rao"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1982-12-08"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-01-22 19:59:59.221"))},
                 {"locationIP", boost::any(std::string("49.249.98.96"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto baruch = graph->add_node(
                // 4139|Baruch|Dego|male|1987-10-25|2010-01-28T01:38:17.824+0000|213.55.127.9|Internet Explorer
                "Person",
                {{"id",         boost::any(4139)},
                 {"firstName",  boost::any(std::string("Baruch"))},
                 {"lastName",   boost::any(std::string("Dego"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1987-10-25"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-01-28 01:38:17.824"))},
                 {"locationIP", boost::any(std::string("213.55.127.9"))},
                 {"browser",    boost::any(std::string("Internet Explorer"))}});
        auto fritz = graph->add_node(
                // 6597069777240|Fritz|Muller|female|1987-12-01|2010-08-24T20:13:46.569+0000|46.19.159.176|Safari
                "Person",
                {{"id",         boost::any(65970697)},
                 {"firstName",  boost::any(std::string("Fritz"))},
                 {"lastName",   boost::any(std::string("Muller"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1987-12-01"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-08-24 20:13:46.569"))},
                 {"locationIP", boost::any(std::string("46.19.159.176"))},
                 {"browser",    boost::any(std::string("Safari"))}});
        auto andrei = graph->add_node(
                // 10995116284808|Andrei|Condariuc|male|1982-02-04|2010-12-26T14:40:36.649+0000|92.39.58.88|Chrome
                "Person",
                {{"id",         boost::any(10995116)},
                 {"firstName",  boost::any(std::string("Andrei"))},
                 {"lastName",   boost::any(std::string("Condariuc"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1982-02-04"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-12-26 14:40:36.649"))},
                 {"locationIP", boost::any(std::string("92.39.58.88"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto ottoR = graph->add_node(
                // 32985348838375|Otto|Richter|male|1980-11-22|2012-07-12T03:11:27.663+0000|204.79.128.176|Firefox
                "Person",
                {{"id",         boost::any(838375)},
                 {"firstName",  boost::any(std::string("Otto"))},
                 {"lastName",   boost::any(std::string("Richter"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1980-11-22"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-07-12 03:11:27.663"))},
                 {"locationIP", boost::any(std::string("204.79.128.176"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto ottoB = graph->add_node(
                // 32985348833579|Otto|Becker|male|1989-09-23|2012-09-03T07:26:57.953+0000|31.211.182.228|Safari
                "Person",
                {{"id",         boost::any(833579)},
                 {"firstName",  boost::any(std::string("Otto"))},
                 {"lastName",   boost::any(std::string("Becker"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1989-09-23"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-09-03 07:26:57.953"))},
                 {"locationIP", boost::any(std::string("31.211.182.228"))},
                 {"browser",    boost::any(std::string("Safari"))}});
        auto hoChi = graph->add_node(
                // 4194|Hồ Chí|Do|male|1988-10-14|2010-02-15T00:46:17.657+0000|103.2.223.188|Internet Explorer
                "Person",
                {{"id",         boost::any(4194)},
                 {"firstName",  boost::any(std::string("Hồ Chí"))},
                 {"lastName",   boost::any(std::string("Do"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1988-10-14"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-02-15 00:46:17.657"))},
                 {"locationIP", boost::any(std::string("103.2.223.188"))},
                 {"browser",    boost::any(std::string("Internet Explorer"))}});
        auto lomana = graph->add_node(
                // 15393162795439|Lomana Trésor|Kanam|male|1986-09-22|2011-04-02T23:53:29.932+0000|41.76.137.230|Chrome
                "Person",
                {{"id",         boost::any(15393)},
                 {"firstName",  boost::any(std::string("Lomana Trésor"))},
                 {"lastName",   boost::any(std::string("Kanam"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1986-09-22"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-04-02 23:53:29.932"))},
                 {"locationIP", boost::any(std::string("41.76.137.230"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto amin = graph->add_node(
                // 19791209307382|Amin|Kamkar|male|1989-05-24|2011-08-30T05:41:09.519+0000|81.28.60.168|Internet Explorer
                "Person",
                {{"id",         boost::any(19791)},
                 {"firstName",  boost::any(std::string("Amin"))},
                 {"lastName",   boost::any(std::string("Kamkar"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1989-05-24"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-08-30 05:41:09.519"))},
                 {"locationIP", boost::any(std::string("81.28.60.168"))},
                 {"browser",    boost::any(std::string("Internet Explorer"))}});
        auto silva = graph->add_node(
                // 1564|Emperor of Brazil|Silva|female|1984-10-22|2010-01-02T06:04:55.320+0000|192.223.88.63|Chrome
                "Person",
                {{"id",         boost::any(1564)},
                 {"firstName",  boost::any(std::string("Emperor of Brazil"))},
                 {"lastName",   boost::any(std::string("Silva"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1984-10-22"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-01-02 06:04:55.320"))},
                 {"locationIP", boost::any(std::string("192.223.88.63"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto ivan = graph->add_node(
                // 10995116283243|Ivan Ignatyevich|Aleksandrov|male|1990-01-03|2010-12-25T08:07:55.284+0000|91.149.169.27|Chrome
                "Person",
                {{"id",         boost::any(1043)},
                 {"firstName",  boost::any(std::string("Ivan Ignatyevich"))},
                 {"lastName",   boost::any(std::string("Aleksandrov"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1990-01-03"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-12-25 08:07:55.284"))},
                 {"locationIP", boost::any(std::string("91.149.169.27"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto bingbing = graph->add_node(
                // 15393162790796|Bingbing|Xu|female|1987-09-19|2011-04-28T23:16:44.375+0000|14.196.249.198|Firefox
                "Person",
                {{"id",         boost::any(90796)},
                 {"firstName",  boost::any(std::string("Bingbing"))},
                 {"lastName",   boost::any(std::string("Xu"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1987-09-19"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-04-28 23:16:44.375"))},
                 {"locationIP", boost::any(std::string("14.196.249.198"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto Gobi = graph->add_node(
                // id|name|url|type
                // 129|Gobichettipalayam|http://dbpedia.org/resource/Gobichettipalayam|city
                "Place",
                {
                        {"id",   boost::any(129)},
                        {"name", boost::any(std::string("Gobichettipalayam"))},
                        {"url",  boost::any(std::string(
                                "http://dbpedia.org/resource/Gobichettipalayam"))},
                        {"type", boost::any(std::string("city"))},
                });
        auto Kelaniya = graph->add_node(
                // 1353|Kelaniya|http://dbpedia.org/resource/Kelaniya|city
                "Place",
                {
                        {"id",   boost::any(1353)},
                        {"name", boost::any(std::string("Kelaniya"))},
                        {"url",
                                 boost::any(std::string("http://dbpedia.org/resource/Kelaniya"))},
                        {"type", boost::any(std::string("city"))},
                });
        auto artux = graph->add_node(
                // 505|Artux|http://dbpedia.org/resource/Artux|city
                "Place",
                {
                        {"id",   boost::any(505)},
                        {"name", boost::any(std::string("Artux"))},
                        {"url",
                                 boost::any(std::string("http://dbpedia.org/resource/Artux"))},
                        {"type", boost::any(std::string("city"))},
                });
        auto germany = graph->add_node(
                // 50|Germany|http://dbpedia.org/resource/Germany|country
                "Place",
                {
                        {"id",   boost::any(50)},
                        {"name", boost::any(std::string("Germany"))},
                        {"url",
                                 boost::any(std::string("http://dbpedia.org/resource/Germany"))},
                        {"type", boost::any(std::string("country"))},
                });
        auto belarus = graph->add_node(
                // 63|Belarus|http://dbpedia.org/resource/Belarus|country
                "Place",
                {
                        {"id",   boost::any(63)},
                        {"name", boost::any(std::string("Belarus"))},
                        {"url",
                                 boost::any(std::string("http://dbpedia.org/resource/Belarus"))},
                        {"type", boost::any(std::string("country"))},
                });
        auto post_13743895 = graph->add_node(
                // id|imageFile|creationDate|locationIP|browserUsed|language|content|length
                // 1374389534791|photo1374389534791.jpg|2011-10-05T14:38:36.019+0000|119.235.7.103|Firefox|||0
                "Post",
                {
                        {"id",         boost::any(13743895)}, /* boost::any(std::string("1374389534791"))} */
                        {"imageFile",  boost::any(std::string("photo1374389534791.jpg"))}, /* String[0..1] */
                        {"creationDate",
                                       boost::any(builtin::dtimestring_to_int("2011-10-05 14:38:36.019"))},
                        {"locationIP", boost::any(std::string("119.235.7.103"))},
                        {"browser",    boost::any(std::string("Firefox"))},
                        {"language",   boost::any(std::string("uz"))}, /* String[0..1] */
                        {"content",    boost::any(std::string(""))},
                        {"length",     boost::any(0)}
                });
        auto post_3627 = graph->add_node(
                /* 2061587303627||2012-08-20T09:51:28.275+0000|14.205.203.83|Firefox|uz|About Alexander I of Russia,
                    lexander tried to introduce liberal reforms, while in the second half|98 */
                "Post",
                {
                        {"id",         boost::any(3627)}, /* boost::any(std::string("2061587303627"))} */
                        {"imageFile",  boost::any(std::string(""))}, /* String[0..1] */
                        {"creationDate",
                                       boost::any(builtin::dtimestring_to_int("2012-08-20 09:51:28.275"))},
                        {"locationIP", boost::any(std::string("14.205.203.83"))},
                        {"browser",    boost::any(std::string("Firefox"))},
                        {"language",   boost::any(std::string("uz"))}, /* String[0..1] */
                        {"content",    boost::any(std::string("About Alexander I of Russia, "
                                                              "lexander tried to introduce liberal reforms, while in the second half"))},
                        {"length",     boost::any(98)}
                });
        auto post_16492674 = graph->add_node(
                /* 1649267442210||2012-01-09T07:50:59.110+0000|192.147.218.174|Internet Explorer|tk|About Louis I of Hungary, dwig der Große,
                      Bulgarian: Лудвиг I, Serbian: Лајош I Анжујски, Czech: Ludvík I. Veliký, Li|117 */
                "Post",
                {{"id",         boost::any(16492674)}, //{"id", boost::any(std::string("1649267442210"))},
                        //{"type", boost::any(std::string("post"))},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"language",   boost::any(std::string("tk"))}, //String[0..1]
                 {"browser",    boost::any(std::string("Internet Explorer"))},
                 {"locationIP", boost::any(std::string("192.147.218.174"))},
                 {"content",    boost::any(std::string("About Louis I of Hungary, dwig der Große, Bulgarian: "
                                                       "Лудвиг I, Serbian: Лајош I Анжујски, Czech: Ludvík I. Veliký, Li"))},
                 {"length",     boost::any(117)},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-01-09 08:05:28.922"))}
                });
        auto comment_12362343 = graph->add_node(
                // id|creationDate|locationIP|browserUsed|content|length
                // 1236950581249|2011-08-17T14:26:59.961+0000|92.39.58.88|Chrome|yes|3
                "Comment",
                {
                        {"id",           boost::any(12362343)},
                        //{"type", boost::any(std::string("comment"))},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2011-08-17 14:26:59.961"))},
                        {"locationIP",   boost::any(std::string("92.39.58.88"))},
                        {"browser",      boost::any(std::string("Chrome"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment_16492675 = graph->add_node(
                /* 1649267442211|2012-01-09T08:05:28.922+0000|91.149.169.27|Chrome|About Louis I of Hungary, rchs of the Late Middle Ages,
                      extending terrAbout Louis XIII |87 */
                "Comment",
                {
                        {"id",           boost::any(164926756)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2011-10-05 14:38:36.019"))},
                        {"locationIP",   boost::any(std::string("91.149.169.27"))},
                        {"browser",      boost::any(std::string("Chrome"))},
                        {"content",      boost::any(
                                std::string("About Louis I of Hungary, rchs of the Late Middle Ages, "
                                            "extending terrAbout Louis XIII "))},
                        {"length",       boost::any(87)}
                });
        boost::posix_time::ptime pt{ boost::gregorian::date{2014, 5, 12},
                                     boost::posix_time::time_duration{12, 0, 0}};
        auto comment_16492676 = graph->add_node(
                /* 1649267442212|2012-01-10T03:24:33.368+0000|14.196.249.198|Firefox|About Bruce Lee,  sources, in the spirit of his personal
                      martial arts philosophy, whic|86 */
                "Comment",
                {
                        {"id",           boost::any(uint64_t(7146846240480))},
                        {"creationDate", builtin::dtimestring_to_int("2011-10-05 14:38:36.019")},
                        {"locationIP",   boost::any(std::string("14.196.249.198"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Bruce Lee,  sources, in the spirit of "
                                                                "his personal martial arts philosophy, whic"))},
                        {"length",       boost::any(86)}
                });
        auto comment_16492677 = graph->add_node(
                /* 1649267442213|2012-01-10T14:57:10.420+0000|81.28.60.168|Internet Explorer|I see|5 */
                "Comment",
                {
                        {"id",           boost::any(16492677)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2012-01-10 14:57:10.420"))},
                        {"locationIP",   boost::any(std::string("81.28.60.168"))},
                        {"browser",      boost::any(std::string("Internet Explorer"))},
                        {"content",      boost::any(std::string("I see"))},
                        {"length",       boost::any(5)}
                });
        auto comment_1642250 = graph->add_node(
                /* 1649267442250|2012-01-19T11:39:51.385+0000|85.154.120.237|Firefox|About Louis I of Hungary,
                    ittle lasting political results. Louis is theAbout Union of Sou|89  */
                "Comment",
                {
                        {"id",           boost::any(1642250)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2012-01-19 11:39:51.385"))},
                        {"locationIP",   boost::any(std::string("85.154.120.237"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(
                                std::string("Firefox|About Louis I of Hungary, ittle lasting political results. "
                                            "Louis is theAbout Union of Sou"))},
                        {"length",       boost::any(89)}
                });
        auto comment_1642217 = graph->add_node(
                /* 1649267442217|2012-01-10T06:31:18.533+0000|41.76.137.230|Chrome|maybe|5
            */
                "Comment",
                {
                        {"id",           boost::any(1642217)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2012-01-10 06:31:18.533"))},
                        {"locationIP",   boost::any(std::string("41.76.137.230"))},
                        {"browser",      boost::any(std::string("Chrome"))},
                        {"content",      boost::any(std::string("maybe"))},
                        {"length",       boost::any(5)}
                });
        auto forum_37 = graph->add_node(
                // id|title|creationDate
                // 37|Wall of Hồ Chí Do|2010-02-15T00:46:27.657+0000
                "Forum",
                {{"id",    boost::any(37)},
                 {"title", boost::any(std::string("Wall of Hồ Chí Do"))},
                 {"creationDate",
                           boost::any(builtin::dtimestring_to_int("2010-02-15 00:46:27.657"))}
                });
        auto forum_71489 = graph->add_node(
                // 549755871489|Group for Alexander_I_of_Russia in Umeå|2010-09-21T16:25:35.425+0000
                "Forum",
                {{"id",    boost::any(71489)},
                 {"title", boost::any(std::string("Group for Alexander_I_of_Russia in Umeå"))},
                 {"creationDate",
                           boost::any(builtin::dtimestring_to_int("2010-09-21 16:25:35.425"))}
                });
        auto tag_206 = graph->add_node(
                // id|name|url
                // 206|Charlemagne|http://dbpedia.org/resource/Charlemagne
                "Tag",
                {{"id",   boost::any(206)},
                 {"name", boost::any(std::string("Charlemagne"))},
                 {"url",
                          boost::any(std::string("http://dbpedia.org/resource/Charlemagne"))}
                });
        auto tag_61 = graph->add_node(
                // 61|Kevin_Rudd|http://dbpedia.org/resource/Kevin_Rudd
                "Tag",
                {{"id",   boost::any(61)},
                 {"name", boost::any(std::string("Kevin_Rudd"))},
                 {"url",
                          boost::any(std::string("http://dbpedia.org/resource/Kevin_Rudd"))}
                });
        auto tag_1679 = graph->add_node(
                // 1679|Alexander_I_of_Russia|http://dbpedia.org/resource/Alexander_I_of_Russia
                "Tag",
                {{"id",   boost::any(1679)},
                 {"name", boost::any(std::string("Alexander_I_of_Russia"))},
                 {"url",
                          boost::any(std::string("http://dbpedia.org/resource/Alexander_I_of_Russia"))}
                });
        auto uni_2213 = graph->add_node(
                // id|type|name|url
                // 2213|university|Anhui_University_of_Science_and_Technology|
                // http://dbpedia.org/resource/Anhui_University_of_Science_and_Technology
                "Organisation",
                {{"id",   boost::any(2213)},
                 {"type", boost::any(std::string("university"))},
                 {"name", boost::any(std::string("Anhui_University_of_Science_and_Technology"))},
                 {"url",
                          boost::any(std::string(
                                  "http://dbpedia.org/resource/Anhui_University_of_Science_and_Technology"))}
                });
        auto company_915 = graph->add_node(
                // id|type|name|url
                // 915|company|Chang'an_Airlines|http://dbpedia.org/resource/Chang'an_Airlines
                "Organisation",
                {{"id",   boost::any(915)},
                 {"type", boost::any(std::string("company"))},
                 {"name", boost::any(std::string("Chang'an_Airlines"))},
                 {"url",
                          boost::any(std::string("http://dbpedia.org/resource/Chang'an_Airlines"))}
                });


        /**
         * Relationships for query interactive short #1
         */
        graph->add_relationship(mahinda, Kelaniya, ":isLocatedIn", {});
        graph->add_relationship(baruch, Gobi, ":isLocatedIn", {});
        graph->add_relationship(mahinda, baruch, ":KNOWS", {
                {"creationDate",   boost::any(builtin::dtimestring_to_int("2010-03-13 07:37:21.718"))},
                {"dummy_property", boost::any(std::string("dummy_1"))}});
        graph->add_relationship(mahinda, fritz, ":KNOWS", {
                {"creationDate",   boost::any(builtin::dtimestring_to_int("2010-09-20 09:42:43.187"))},
                {"dummy_property", boost::any(std::string("dummy_2"))}});
        graph->add_relationship(mahinda, andrei, ":KNOWS", {
                {"creationDate",   boost::any(builtin::dtimestring_to_int("2011-01-02 06:43:41.955"))},
                {"dummy_property", boost::any(std::string("dummy_3"))}});
        graph->add_relationship(mahinda, ottoB, ":KNOWS", {
                {"creationDate",   boost::any(builtin::dtimestring_to_int("2012-09-07 01:11:30.195"))},
                {"dummy_property", boost::any(std::string("dummy_4"))}});
        graph->add_relationship(mahinda, ottoR, ":KNOWS", {
                {"creationDate", /* testing date order */ boost::any(
                        builtin::dtimestring_to_int("2012-09-07 01:11:30.195"))},
                {"dummy_property",                        boost::any(std::string("dummy_5"))}});
        graph->add_relationship(comment_12362343, andrei, ":hasCreator", {});
        graph->add_relationship(forum_37, post_16492674, ":containerOf", {});
        graph->add_relationship(forum_37, hoChi, ":hasModerator", {});
        graph->add_relationship(comment_16492675, post_16492674, ":replyOf", {});
        graph->add_relationship(comment_16492676, comment_16492675, ":replyOf", {});
        graph->add_relationship(comment_16492677, comment_16492676, ":replyOf", {});
        graph->add_relationship(comment_1642217, comment_16492676, ":replyOf", {});
        graph->add_relationship(comment_1642217, lomana, ":hasCreator", {});
        graph->add_relationship(comment_16492677, amin, ":hasCreator", {});
        graph->add_relationship(comment_16492676, bingbing, ":hasCreator", {});
        graph->add_relationship(lomana, bingbing, ":KNOWS", {});
    }
    for(int i = 0; i < 1000; i++) {

        auto ravalomanana = graph->add_node(
                // 65|Marc|Ravalomanana|female|1989-06-15|2010-02-26T23:17:18.465+0000|41.204.119.20|Firefox
                "Person",
                {{"id",         boost::any(65)},
                 {"firstName",  boost::any(std::string("Marc"))},
                 {"lastName",   boost::any(std::string("Ravalomanana"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1989-06-15"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-02-26 23:17:18.465"))},
                 {"locationIP", boost::any(std::string("41.204.119.20"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_1 = graph->add_node(
                // 19791209302379|Muhammad|Iqbal|female|1983-09-13|2011-08-14T03:06:21.524+0000|202.14.71.199|Chrome
                "Person",
                {{"id",         boost::any(1379)},
                 {"firstName",  boost::any(std::string("Muhammad"))},
                 {"lastName",   boost::any(std::string("Iqbal"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-09-13"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-08-14 03:06:21.524"))},
                 {"locationIP", boost::any(std::string("202.14.71.199"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_2 = graph->add_node(
                // 17592186055291|Wei|Li|female|1986-09-24|2011-05-10T20:09:44.151+0000|1.4.4.26|Chrome
                "Person",
                {{"id",         boost::any(1291)},
                 {"firstName",  boost::any(std::string("Wei"))},
                 {"lastName",   boost::any(std::string("Li"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1986-09-24"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-05-10 20:09:44.151"))},
                 {"locationIP", boost::any(std::string("1.4.4.26"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_3 = graph->add_node(
                // 15393162799121|Karl|Beran|male|1983-05-30|2011-04-02T00:14:40.528+0000|31.130.85.235|Chrome
                "Person",
                {{"id",         boost::any(1121)},
                 {"firstName",  boost::any(std::string("Karl"))},
                 {"lastName",   boost::any(std::string("Beran"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-05-30"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-04-02 00:14:40.528"))},
                 {"locationIP", boost::any(std::string("31.130.85.235"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_4 = graph->add_node(
                // 8796093028680|Rahul|Singh|female|1981-12-29|2010-10-09T07:08:12.913+0000|61.17.209.13|Firefox
                "Person",
                {{"id",         boost::any(1680)},
                 {"firstName",  boost::any(std::string("Rahul"))},
                 {"lastName",   boost::any(std::string("Singh"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1981-12-2"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-10-09 07:08:12.913"))},
                 {"locationIP", boost::any(std::string("61.17.209.13"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_5 = graph->add_node(
                // 19791209302377|John|Smith|male|1983-08-31|2011-08-10T15:59:24.890+0000|24.245.233.94|Firefox
                "Person",
                {{"id",         boost::any(1377)},
                 {"firstName",  boost::any(std::string("John"))},
                 {"lastName",   boost::any(std::string("Smith"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-08-31"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-08-10 15:59:24.890"))},
                 {"locationIP", boost::any(std::string("24.245.233.94"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_6 = graph->add_node(
                // 10995116278350|Abdul|Aman|male|1982-05-24|2010-11-17T00:16:33.065+0000|180.222.141.92|Internet Explorer
                "Person",
                {{"id",         boost::any(1350)},
                 {"firstName",  boost::any(std::string("Abdul"))},
                 {"lastName",   boost::any(std::string("Aman"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1982-05-24"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-11-17 00:16:33.065"))},
                 {"locationIP", boost::any(std::string("180.222.141.92"))},
                 {"browser",    boost::any(std::string("Internet Explorer"))}});
        auto person2_7 = graph->add_node(
                // 4398046514661|Rajiv|Singh|male|1983-02-17|2010-05-13T06:57:29.021+0000|49.46.196.167|Firefox
                "Person",
                {{"id",         boost::any(1661)},
                 {"firstName",  boost::any(std::string("Rajiv"))},
                 {"lastName",   boost::any(std::string("Singh"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-02-17"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-05-13 06:57:29.021"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_8 = graph->add_node(
                // 10995116278353|Otto|Muller|male|1988-10-28|2010-12-19T22:06:54.592+0000|204.79.148.6|Firefox
                "Person",
                {{"id",         boost::any(18353)},
                 {"firstName",  boost::any(std::string("Otto"))},
                 {"lastName",   boost::any(std::string("Muller"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1988-10-28"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-12-19 22:06:54.592"))},
                 {"locationIP", boost::any(std::string("204.79.148.6"))},
                 {"browser",    boost::any(std::string("Firefox"))}});


        auto post2_1 = graph->add_node(
                // id|imageFile|creationDate|locationIP|browserUsed|language|content|length
                /* 1374390164863||2011-10-17T05:40:34.561+0000|41.204.119.20|Firefox|uz|About Paul Keres,  in
                    the Candidates' Tournament on four consecutive occasions. Due to these and other strong results,
                    many chess historians consider Keres the strongest player never to be|188 */
                "Post",
                {{"id",         boost::any(1863)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-17 05:40:34.561"))},
                 {"locationIP", boost::any(std::string("41.204.119.20"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Paul Keres,  in the "
                                                       "Candidates' Tournament on four consecutive occasions. Due to these "
                                                       "and other strong results, many chess historians consider Keres the strongest player never to be"))},
                 {"length",     boost::any(188)}
                });
        auto post2_2 = graph->add_node(
                /* 1649268071976||2012-01-14T09:41:00.992+0000|24.245.233.94|Firefox|uz|About Paul Keres, hampionship "
                  match against champion Alexander Alekhine, but the match never took place due to World War|120 */
                "Post",
                {{"id",         boost::any(1976)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-01-14 09:41:00.992"))},
                 {"locationIP", boost::any(std::string("24.245.233.94"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Paul Keres, hampionship "
                                                       "match against champion Alexander Alekhine, but the match never took place due to World War"))},
                 {"length",     boost::any(120)}
                });
        auto post2_3 = graph->add_node(
                /* 1374390165125||2011-10-16T15:05:23.955+0000|204.79.148.6|Firefox|uz|About Otto von Bismarck, onsible
                for the unifiAbout Muammar Gaddafi, e styled himself as LAbout Pete T|102 */
                "Post",
                {{"id",         boost::any(1125)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-16 15:05:23.955"))},
                 {"locationIP", boost::any(std::string("204.79.148.6"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Otto von Bismarck, onsible "
                                                       "for the unifiAbout Muammar Gaddafi, e styled himself as LAbout Pete T"))},
                 {"length",     boost::any(188)}
                });
        auto post2_4 = graph->add_node(
                /* 1374390165164||2011-10-16T23:30:53.955+0000|1.4.4.26|Chrome|uz|About Muammar Gaddafi, June 1942Sirte,
                Libya Died 20 October 2About James Joyce, he Jesuit schools Clongowes and BelvedeAbout Laurence Olivier,  and
                British drama. He was the first arAbout Osa|192 */
                "Post",
                {{"id",         boost::any(1164)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-16 23:30:53.955"))},
                 {"locationIP", boost::any(std::string("1.4.4.26"))},
                 {"browser",    boost::any(std::string("Chrome"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Muammar Gaddafi, June 1942Sirte, "
                                                       "Libya Died 20 October 2About James Joyce, he Jesuit schools Clongowes and BelvedeAbout Laurence Olivier,  and "
                                                       "British drama. He was the first arAbout Osa"))},
                 {"length",     boost::any(192)}
                });
        auto post2_5 = graph->add_node(
                /* 1786712928767||2012-03-29T11:17:50.625+0000|31.130.85.235|Chrome|ar|About Catherine the
                Great, (2 May  1729 – 17 November  1796), was the most renowned and th|90 */
                "Post",
                {{"id",         boost::any(1767)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-03-29 11:17:50.625"))},
                 {"locationIP", boost::any(std::string("31.130.85.235"))},
                 {"browser",    boost::any(std::string("Chrome"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Catherine the "
                                                       "Great, (2 May  1729 – 17 November  1796), was the most renowned and th"))},
                 {"length",     boost::any(90)}
                });
        auto post2_6 = graph->add_node(
                /* 1099518161705||2011-06-24T22:26:11.884+0000|61.17.209.13|Firefox|ar|About Catherine the Great,
                e largest share. In the east, Russia started to colonise Al|86 */
                "Post",
                {{"id",         boost::any(1705)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-06-24 22:26:11.884"))},
                 {"locationIP", boost::any(std::string("61.17.209.13"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Catherine the Great, "
                                                       "e largest share. In the east, Russia started to colonise Al"))},
                 {"length",     boost::any(86)}
                });
        auto post2_7 = graph->add_node(
                /* 1374396068816||2011-09-27T05:59:43.468+0000|49.46.196.167|Firefox|ar|About Fernando González,
                y Chile's best tennis player oAbout Vichy France,  (GPRF). Most |89 */
                "Post",
                {{"id",         boost::any(1816)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-27 05:59:43.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, "
                                                       "y Chile's best tennis player oAbout Vichy France,  (GPRF). Most "))},
                 {"length",     boost::any(89)}
                });
        auto post2_8 = graph->add_node(
                /* 1374396068820||2011-09-26T16:39:28.468+0000|49.46.196.167|Firefox|ar|About Fernando González, ian Open,
                losing tAbout Mary, Queen of Scots, ter. In 1558, she About David, to p|106 */
                "Post",
                {{"id",         boost::any(1816)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-26 16:39:28.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, ian Open, "
                                                       "losing tAbout Mary, Queen of Scots, ter. In 1558, she About David, to p"))},
                 {"length",     boost::any(106)}
                });
        auto post2_9 = graph->add_node(
                /* 1374396068835||2011-09-27T09:25:13.468+0000|49.46.196.167|Firefox|ar|About Fernando González,
                en, Marat SafiAbout Cole Porter, u Under My SkiAbout Ray Bradbury, 953) and for tA|107 */
                "Post",
                {{"id",         boost::any(1835)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-27 09:25:13.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, "
                                                       "en, Marat SafiAbout Cole Porter, u Under My SkiAbout Ray Bradbury, 953) and for tA"))},
                 {"length",     boost::any(107)}
                });
        auto comment2_1 = graph->add_node(
                /* 1374390164865|2011-10-17T09:17:43.567+0000|41.204.119.20|Firefox|About Paul Keres, Alexander Alekhine,
                but the match never toAbout Birth of a Prince|83 */
                "Comment",
                {
                        {"id",           boost::any(1865)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-01-17 09:17:43.567"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Paul Keres, Alexander Alekhine, "
                                                                "but the match never toAbout Birth of a Prince"))},
                        {"length",       boost::any(83)}
                });
        auto comment2_2 = graph->add_node(
                /* 1374390164877|2011-10-17T10:59:33.177+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1877)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-2-17 10:59:33.177"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_3 = graph->add_node(
                /* 1649268071978|2012-01-14T16:57:46.045+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1978)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-03-14 16:57:46.045"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_4 = graph->add_node(
                /* 1374390165126|2011-10-16T21:16:03.354+0000|41.204.119.20|Firefox|roflol|6 */
                "Comment",
                {
                        {"id",           boost::any(1126)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-04-16 21:16:03.354"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("roflol"))},
                        {"length",       boost::any(6)}
                });
        auto comment2_5 = graph->add_node(
                /* 1374390165171|2011-10-17T19:37:26.339+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1171)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-05-17 19:37:26.339"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_6 = graph->add_node(
                /* 1786712928768|2012-03-29T17:57:51.844+0000|41.204.119.20|Firefox|LOL|3 */
                "Comment",
                {
                        {"id",           boost::any(1768)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-06-29 17:57:51.844"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("LOL"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_7 = graph->add_node(
                /* 1099518161711|2011-06-25T07:54:01.976+0000|41.204.119.20|Firefox|no|2 */
                "Comment",
                {
                        {"id",           boost::any(1711)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-07-25 07:54:01.976"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("no"))},
                        {"length",       boost::any(2)}
                });
        auto comment2_8 = graph->add_node(
                /* 1099518161722|2011-06-25T12:56:57.280+0000|41.204.119.20|Firefox|LOL|3 */
                "Comment",
                {
                        {"id",           boost::any(1722)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-08-25 12:56:57.280"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("LOL"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_9 = graph->add_node(
                /* 1374396068819|2011-09-27T09:41:01.413+0000|41.204.119.20|Firefox|About Fernando González,
                er from Chile. He is kAbout George W. Bush, 04 for a description oAbout Vichy Franc|108 */
                "Comment",
                {
                        {"id",           boost::any(1819)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-09-27 09:41:01.413"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "er from Chile. He is kAbout George W. Bush, 04 for a description oAbout Vichy Franc"))},
                        {"length",       boost::any(108)}
                });
        auto comment2_10 = graph->add_node(
                /* 1374396068821|2011-09-26T23:46:18.580+0000|41.76.205.156|Firefox|About Fernando González,
                les at Athens 2004About Ronald Reagan, st in films and laAbou|86 */
                "Comment",
                {
                        {"id",           boost::any(1821)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-10-26 23:46:18.580"))},
                        {"locationIP",   boost::any(std::string("41.76.205.156"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "les at Athens 2004About Ronald Reagan, st in films and laAbou"))},
                        {"length",       boost::any(86)}
                });
        auto comment2_11 = graph->add_node(
                /* 1374396068827|2011-09-26T17:09:07.283+0000|41.204.119.20|Firefox|About Fernando González,
                Safin, and Pete SAbout Mary, Queen of Scots,  had previously cAbout Edward the Confessor, isintegration of Abo|135 */
                "Comment",
                {
                        {"id",           boost::any(1827)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-11-26 17:09:07.283"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "Safin, and Pete SAbout Mary, Queen of Scots,  had previously cAbout Edward the Confessor, isintegration of Abo"))},
                        {"length",       boost::any(135)}
                });
        auto comment2_12 = graph->add_node(
                /* 1374396068837|2011-09-27T11:32:19.336+0000|41.204.119.20|Firefox|maybe|5 */
                "Comment",
                {
                        {"id",           boost::any(1837)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-12-27 11:32:19.336"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("maybe"))},
                        {"length",       boost::any(5)}
                });
        graph->add_relationship(comment2_1, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_2, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_3, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_4, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_5, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_6, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_7, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_8, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_9, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_10, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_11, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_12, ravalomanana, ":hasCreator", {});

        graph->add_relationship(comment2_1, post2_1, ":replyOf", {});
        graph->add_relationship(comment2_2, post2_1, ":replyOf", {});
        graph->add_relationship(comment2_3, post2_2, ":replyOf", {});
        graph->add_relationship(comment2_4, post2_3, ":replyOf", {});
        graph->add_relationship(comment2_5, post2_4, ":replyOf", {});
        graph->add_relationship(comment2_6, post2_5, ":replyOf", {});
        graph->add_relationship(comment2_7, post2_6, ":replyOf", {});
        graph->add_relationship(comment2_8, post2_6, ":replyOf", {});
        graph->add_relationship(comment2_9, post2_7, ":replyOf", {});
        graph->add_relationship(comment2_10, post2_8, ":replyOf", {});
        graph->add_relationship(comment2_11, post2_8, ":replyOf", {});
        graph->add_relationship(comment2_12, post2_9, ":replyOf", {});

        graph->add_relationship(post2_1, ravalomanana, ":hasCreator", {});
        graph->add_relationship(post2_2, person2_5, ":hasCreator", {});
        graph->add_relationship(post2_3, person2_8, ":hasCreator", {});
        graph->add_relationship(post2_4, person2_2, ":hasCreator", {});
        graph->add_relationship(post2_5, person2_3, ":hasCreator", {});
        graph->add_relationship(post2_6, person2_4, ":hasCreator", {});
        graph->add_relationship(post2_7, person2_7, ":hasCreator", {});
        graph->add_relationship(post2_8, person2_7, ":hasCreator", {});
        graph->add_relationship(post2_9, person2_7, ":hasCreator", {});
    }
    graph->commit_transaction();

    return graph;
}
graph_db_ptr graph2() {
    auto pool = graph_pool::create("/mnt/pmem0/test", 1024*1024*900);
    auto graph = pool->create_graph("my_grap");
    auto gdb = graph.get();

    auto tx = graph->begin_transaction();

    for(int i = 0; i < 1000; i++) {

        auto ravalomanana = graph->add_node(
                // 65|Marc|Ravalomanana|female|1989-06-15|2010-02-26T23:17:18.465+0000|41.204.119.20|Firefox
                "Person",
                {{"id",         boost::any(65)},
                 {"firstName",  boost::any(std::string("Marc"))},
                 {"lastName",   boost::any(std::string("Ravalomanana"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1989-06-15"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-02-26 23:17:18.465"))},
                 {"locationIP", boost::any(std::string("41.204.119.20"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_1 = graph->add_node(
                // 19791209302379|Muhammad|Iqbal|female|1983-09-13|2011-08-14T03:06:21.524+0000|202.14.71.199|Chrome
                "Person",
                {{"id",         boost::any(1379)},
                 {"firstName",  boost::any(std::string("Muhammad"))},
                 {"lastName",   boost::any(std::string("Iqbal"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-09-13"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-08-14 03:06:21.524"))},
                 {"locationIP", boost::any(std::string("202.14.71.199"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_2 = graph->add_node(
                // 17592186055291|Wei|Li|female|1986-09-24|2011-05-10T20:09:44.151+0000|1.4.4.26|Chrome
                "Person",
                {{"id",         boost::any(1291)},
                 {"firstName",  boost::any(std::string("Wei"))},
                 {"lastName",   boost::any(std::string("Li"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1986-09-24"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-05-10 20:09:44.151"))},
                 {"locationIP", boost::any(std::string("1.4.4.26"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_3 = graph->add_node(
                // 15393162799121|Karl|Beran|male|1983-05-30|2011-04-02T00:14:40.528+0000|31.130.85.235|Chrome
                "Person",
                {{"id",         boost::any(1121)},
                 {"firstName",  boost::any(std::string("Karl"))},
                 {"lastName",   boost::any(std::string("Beran"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-05-30"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-04-02 00:14:40.528"))},
                 {"locationIP", boost::any(std::string("31.130.85.235"))},
                 {"browser",    boost::any(std::string("Chrome"))}});
        auto person2_4 = graph->add_node(
                // 8796093028680|Rahul|Singh|female|1981-12-29|2010-10-09T07:08:12.913+0000|61.17.209.13|Firefox
                "Person",
                {{"id",         boost::any(1680)},
                 {"firstName",  boost::any(std::string("Rahul"))},
                 {"lastName",   boost::any(std::string("Singh"))},
                 {"gender",     boost::any(std::string("female"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1981-12-2"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-10-09 07:08:12.913"))},
                 {"locationIP", boost::any(std::string("61.17.209.13"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_5 = graph->add_node(
                // 19791209302377|John|Smith|male|1983-08-31|2011-08-10T15:59:24.890+0000|24.245.233.94|Firefox
                "Person",
                {{"id",         boost::any(1377)},
                 {"firstName",  boost::any(std::string("John"))},
                 {"lastName",   boost::any(std::string("Smith"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-08-31"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-08-10 15:59:24.890"))},
                 {"locationIP", boost::any(std::string("24.245.233.94"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_6 = graph->add_node(
                // 10995116278350|Abdul|Aman|male|1982-05-24|2010-11-17T00:16:33.065+0000|180.222.141.92|Internet Explorer
                "Person",
                {{"id",         boost::any(1350)},
                 {"firstName",  boost::any(std::string("Abdul"))},
                 {"lastName",   boost::any(std::string("Aman"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1982-05-24"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-11-17 00:16:33.065"))},
                 {"locationIP", boost::any(std::string("180.222.141.92"))},
                 {"browser",    boost::any(std::string("Internet Explorer"))}});
        auto person2_7 = graph->add_node(
                // 4398046514661|Rajiv|Singh|male|1983-02-17|2010-05-13T06:57:29.021+0000|49.46.196.167|Firefox
                "Person",
                {{"id",         boost::any(1661)},
                 {"firstName",  boost::any(std::string("Rajiv"))},
                 {"lastName",   boost::any(std::string("Singh"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1983-02-17"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-05-13 06:57:29.021"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))}});
        auto person2_8 = graph->add_node(
                // 10995116278353|Otto|Muller|male|1988-10-28|2010-12-19T22:06:54.592+0000|204.79.148.6|Firefox
                "Person",
                {{"id",         boost::any(18353)},
                 {"firstName",  boost::any(std::string("Otto"))},
                 {"lastName",   boost::any(std::string("Muller"))},
                 {"gender",     boost::any(std::string("male"))},
                 {"birthday",   boost::any(builtin::datestring_to_int("1988-10-28"))},
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2010-12-19 22:06:54.592"))},
                 {"locationIP", boost::any(std::string("204.79.148.6"))},
                 {"browser",    boost::any(std::string("Firefox"))}});


        auto post2_1 = graph->add_node(
                // id|imageFile|creationDate|locationIP|browserUsed|language|content|length
                /* 1374390164863||2011-10-17T05:40:34.561+0000|41.204.119.20|Firefox|uz|About Paul Keres,  in
                    the Candidates' Tournament on four consecutive occasions. Due to these and other strong results,
                    many chess historians consider Keres the strongest player never to be|188 */
                "Post",
                {{"id",         boost::any(1863)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-17 05:40:34.561"))},
                 {"locationIP", boost::any(std::string("41.204.119.20"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Paul Keres,  in the "
                                                       "Candidates' Tournament on four consecutive occasions. Due to these "
                                                       "and other strong results, many chess historians consider Keres the strongest player never to be"))},
                 {"length",     boost::any(188)}
                });
        auto post2_2 = graph->add_node(
                /* 1649268071976||2012-01-14T09:41:00.992+0000|24.245.233.94|Firefox|uz|About Paul Keres, hampionship "
                  match against champion Alexander Alekhine, but the match never took place due to World War|120 */
                "Post",
                {{"id",         boost::any(1976)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-01-14 09:41:00.992"))},
                 {"locationIP", boost::any(std::string("24.245.233.94"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Paul Keres, hampionship "
                                                       "match against champion Alexander Alekhine, but the match never took place due to World War"))},
                 {"length",     boost::any(120)}
                });
        auto post2_3 = graph->add_node(
                /* 1374390165125||2011-10-16T15:05:23.955+0000|204.79.148.6|Firefox|uz|About Otto von Bismarck, onsible
                for the unifiAbout Muammar Gaddafi, e styled himself as LAbout Pete T|102 */
                "Post",
                {{"id",         boost::any(1125)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-16 15:05:23.955"))},
                 {"locationIP", boost::any(std::string("204.79.148.6"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Otto von Bismarck, onsible "
                                                       "for the unifiAbout Muammar Gaddafi, e styled himself as LAbout Pete T"))},
                 {"length",     boost::any(188)}
                });
        auto post2_4 = graph->add_node(
                /* 1374390165164||2011-10-16T23:30:53.955+0000|1.4.4.26|Chrome|uz|About Muammar Gaddafi, June 1942Sirte,
                Libya Died 20 October 2About James Joyce, he Jesuit schools Clongowes and BelvedeAbout Laurence Olivier,  and
                British drama. He was the first arAbout Osa|192 */
                "Post",
                {{"id",         boost::any(1164)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-10-16 23:30:53.955"))},
                 {"locationIP", boost::any(std::string("1.4.4.26"))},
                 {"browser",    boost::any(std::string("Chrome"))},
                 {"language",   boost::any(std::string("\"uz\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Muammar Gaddafi, June 1942Sirte, "
                                                       "Libya Died 20 October 2About James Joyce, he Jesuit schools Clongowes and BelvedeAbout Laurence Olivier,  and "
                                                       "British drama. He was the first arAbout Osa"))},
                 {"length",     boost::any(192)}
                });
        auto post2_5 = graph->add_node(
                /* 1786712928767||2012-03-29T11:17:50.625+0000|31.130.85.235|Chrome|ar|About Catherine the
                Great, (2 May  1729 – 17 November  1796), was the most renowned and th|90 */
                "Post",
                {{"id",         boost::any(1767)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2012-03-29 11:17:50.625"))},
                 {"locationIP", boost::any(std::string("31.130.85.235"))},
                 {"browser",    boost::any(std::string("Chrome"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Catherine the "
                                                       "Great, (2 May  1729 – 17 November  1796), was the most renowned and th"))},
                 {"length",     boost::any(90)}
                });
        auto post2_6 = graph->add_node(
                /* 1099518161705||2011-06-24T22:26:11.884+0000|61.17.209.13|Firefox|ar|About Catherine the Great,
                e largest share. In the east, Russia started to colonise Al|86 */
                "Post",
                {{"id",         boost::any(1705)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-06-24 22:26:11.884"))},
                 {"locationIP", boost::any(std::string("61.17.209.13"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Catherine the Great, "
                                                       "e largest share. In the east, Russia started to colonise Al"))},
                 {"length",     boost::any(86)}
                });
        auto post2_7 = graph->add_node(
                /* 1374396068816||2011-09-27T05:59:43.468+0000|49.46.196.167|Firefox|ar|About Fernando González,
                y Chile's best tennis player oAbout Vichy France,  (GPRF). Most |89 */
                "Post",
                {{"id",         boost::any(1816)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-27 05:59:43.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, "
                                                       "y Chile's best tennis player oAbout Vichy France,  (GPRF). Most "))},
                 {"length",     boost::any(89)}
                });
        auto post2_8 = graph->add_node(
                /* 1374396068820||2011-09-26T16:39:28.468+0000|49.46.196.167|Firefox|ar|About Fernando González, ian Open,
                losing tAbout Mary, Queen of Scots, ter. In 1558, she About David, to p|106 */
                "Post",
                {{"id",         boost::any(1816)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-26 16:39:28.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, ian Open, "
                                                       "losing tAbout Mary, Queen of Scots, ter. In 1558, she About David, to p"))},
                 {"length",     boost::any(106)}
                });
        auto post2_9 = graph->add_node(
                /* 1374396068835||2011-09-27T09:25:13.468+0000|49.46.196.167|Firefox|ar|About Fernando González,
                en, Marat SafiAbout Cole Porter, u Under My SkiAbout Ray Bradbury, 953) and for tA|107 */
                "Post",
                {{"id",         boost::any(1835)},
                 {"imageFile",  boost::any(std::string(""))}, //String[0..1]
                 {"creationDate",
                                boost::any(builtin::dtimestring_to_int("2011-09-27 09:25:13.468"))},
                 {"locationIP", boost::any(std::string("49.46.196.167"))},
                 {"browser",    boost::any(std::string("Firefox"))},
                 {"language",   boost::any(std::string("\"ar\""))}, //String[0..1]
                 {"content",    boost::any(std::string("About Fernando González, "
                                                       "en, Marat SafiAbout Cole Porter, u Under My SkiAbout Ray Bradbury, 953) and for tA"))},
                 {"length",     boost::any(107)}
                });
        auto comment2_1 = graph->add_node(
                /* 1374390164865|2011-10-17T09:17:43.567+0000|41.204.119.20|Firefox|About Paul Keres, Alexander Alekhine,
                but the match never toAbout Birth of a Prince|83 */
                "Comment",
                {
                        {"id",           boost::any(1865)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-01-17 09:17:43.567"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Paul Keres, Alexander Alekhine, "
                                                                "but the match never toAbout Birth of a Prince"))},
                        {"length",       boost::any(83)}
                });
        auto comment2_2 = graph->add_node(
                /* 1374390164877|2011-10-17T10:59:33.177+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1877)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-2-17 10:59:33.177"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_3 = graph->add_node(
                /* 1649268071978|2012-01-14T16:57:46.045+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1978)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-03-14 16:57:46.045"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_4 = graph->add_node(
                /* 1374390165126|2011-10-16T21:16:03.354+0000|41.204.119.20|Firefox|roflol|6 */
                "Comment",
                {
                        {"id",           boost::any(1126)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-04-16 21:16:03.354"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("roflol"))},
                        {"length",       boost::any(6)}
                });
        auto comment2_5 = graph->add_node(
                /* 1374390165171|2011-10-17T19:37:26.339+0000|41.204.119.20|Firefox|yes|3 */
                "Comment",
                {
                        {"id",           boost::any(1171)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-05-17 19:37:26.339"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("yes"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_6 = graph->add_node(
                /* 1786712928768|2012-03-29T17:57:51.844+0000|41.204.119.20|Firefox|LOL|3 */
                "Comment",
                {
                        {"id",           boost::any(1768)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-06-29 17:57:51.844"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("LOL"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_7 = graph->add_node(
                /* 1099518161711|2011-06-25T07:54:01.976+0000|41.204.119.20|Firefox|no|2 */
                "Comment",
                {
                        {"id",           boost::any(1711)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-07-25 07:54:01.976"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("no"))},
                        {"length",       boost::any(2)}
                });
        auto comment2_8 = graph->add_node(
                /* 1099518161722|2011-06-25T12:56:57.280+0000|41.204.119.20|Firefox|LOL|3 */
                "Comment",
                {
                        {"id",           boost::any(1722)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-08-25 12:56:57.280"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("LOL"))},
                        {"length",       boost::any(3)}
                });
        auto comment2_9 = graph->add_node(
                /* 1374396068819|2011-09-27T09:41:01.413+0000|41.204.119.20|Firefox|About Fernando González,
                er from Chile. He is kAbout George W. Bush, 04 for a description oAbout Vichy Franc|108 */
                "Comment",
                {
                        {"id",           boost::any(1819)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-09-27 09:41:01.413"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "er from Chile. He is kAbout George W. Bush, 04 for a description oAbout Vichy Franc"))},
                        {"length",       boost::any(108)}
                });
        auto comment2_10 = graph->add_node(
                /* 1374396068821|2011-09-26T23:46:18.580+0000|41.76.205.156|Firefox|About Fernando González,
                les at Athens 2004About Ronald Reagan, st in films and laAbou|86 */
                "Comment",
                {
                        {"id",           boost::any(1821)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-10-26 23:46:18.580"))},
                        {"locationIP",   boost::any(std::string("41.76.205.156"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "les at Athens 2004About Ronald Reagan, st in films and laAbou"))},
                        {"length",       boost::any(86)}
                });
        auto comment2_11 = graph->add_node(
                /* 1374396068827|2011-09-26T17:09:07.283+0000|41.204.119.20|Firefox|About Fernando González,
                Safin, and Pete SAbout Mary, Queen of Scots,  had previously cAbout Edward the Confessor, isintegration of Abo|135 */
                "Comment",
                {
                        {"id",           boost::any(1827)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-11-26 17:09:07.283"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("About Fernando González, "
                                                                "Safin, and Pete SAbout Mary, Queen of Scots,  had previously cAbout Edward the Confessor, isintegration of Abo"))},
                        {"length",       boost::any(135)}
                });
        auto comment2_12 = graph->add_node(
                /* 1374396068837|2011-09-27T11:32:19.336+0000|41.204.119.20|Firefox|maybe|5 */
                "Comment",
                {
                        {"id",           boost::any(1837)},
                        {"creationDate", boost::any(builtin::dtimestring_to_int("2013-12-27 11:32:19.336"))},
                        {"locationIP",   boost::any(std::string("41.204.119.20"))},
                        {"browser",      boost::any(std::string("Firefox"))},
                        {"content",      boost::any(std::string("maybe"))},
                        {"length",       boost::any(5)}
                });
        graph->add_relationship(comment2_1, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_2, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_3, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_4, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_5, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_6, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_7, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_8, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_9, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_10, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_11, ravalomanana, ":hasCreator", {});
        graph->add_relationship(comment2_12, ravalomanana, ":hasCreator", {});

        graph->add_relationship(comment2_1, post2_1, ":replyOf", {});
        graph->add_relationship(comment2_2, post2_1, ":replyOf", {});
        graph->add_relationship(comment2_3, post2_2, ":replyOf", {});
        graph->add_relationship(comment2_4, post2_3, ":replyOf", {});
        graph->add_relationship(comment2_5, post2_4, ":replyOf", {});
        graph->add_relationship(comment2_6, post2_5, ":replyOf", {});
        graph->add_relationship(comment2_7, post2_6, ":replyOf", {});
        graph->add_relationship(comment2_8, post2_6, ":replyOf", {});
        graph->add_relationship(comment2_9, post2_7, ":replyOf", {});
        graph->add_relationship(comment2_10, post2_8, ":replyOf", {});
        graph->add_relationship(comment2_11, post2_8, ":replyOf", {});
        graph->add_relationship(comment2_12, post2_9, ":replyOf", {});

        graph->add_relationship(post2_1, ravalomanana, ":hasCreator", {});
        graph->add_relationship(post2_2, person2_5, ":hasCreator", {});
        graph->add_relationship(post2_3, person2_8, ":hasCreator", {});
        graph->add_relationship(post2_4, person2_2, ":hasCreator", {});
        graph->add_relationship(post2_5, person2_3, ":hasCreator", {});
        graph->add_relationship(post2_6, person2_4, ":hasCreator", {});
        graph->add_relationship(post2_7, person2_7, ":hasCreator", {});
        graph->add_relationship(post2_8, person2_7, ":hasCreator", {});
        graph->add_relationship(post2_9, person2_7, ":hasCreator", {});
    }
    graph->commit_transaction();

    return graph;
}

algebra_optr create_is1_query() {
    result_set rs;
    auto expr = Scan(&rs, "Person",
                     Filter(EQ(Key("id"), Int(933)),
                            ForeachRship(RSHIP_DIR::FROM, {}, ":isLocatedIn",
                                         Expand(EXPAND::OUT, "Place",
                                                Project({{0,"firstName",FTYPE::STRING}, {0,"lastName",FTYPE::STRING}, {0,"locationIP",FTYPE::STRING},
                                                         {0,"browser",FTYPE::STRING}, {2,"id",FTYPE::INT}, {0,"gender",FTYPE::STRING}},
                                                        Collect(rs))))));
    return expr;
}

algebra_optr create_is2_1_query() {
    auto personId = 65;
    auto maxHops = 3;
    result_set rs;

    auto expr = Scan(&rs, "Person",
                     Filter(EQ(Key("id"), Int(65)),
                            ForeachRship(RSHIP_DIR::TO, {}, ":hasCreator",
                                         Limit(10,
                                               Expand(EXPAND::IN, "Comment",
                                                      ForeachRship(RSHIP_DIR::FROM, {}, ":replyOf",
                                                                   Expand(EXPAND::OUT, "Post",
                                                                          ForeachRship(RSHIP_DIR::FROM, {}, ":hasCreator",
                                                                                       Expand(EXPAND::OUT, "Person",
                                                                                              Project({{2, "id", FTYPE::INT}, {2, "content", FTYPE::STRING}, {2, "creationDate", FTYPE::INT}, {4, "id", FTYPE::INT},
                                                                                                       {6, "id", FTYPE::INT}, {6, "firstName", FTYPE::STRING}, {6, "lastName", FTYPE::STRING}},
                                                                                                      Sort([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                                                                                                               if(boost::get<int>(qr1[2]) == boost::get<int>(qr2[2]))
                                                                                                                   return boost::get<int>(qr1[0]) > boost::get<int>(qr2[0]);
                                                                                                               return boost::get<int>(qr1[2]) > boost::get<int>(qr2[2]); },
                                                                                                           Collect(rs))))))))))));
    return expr;
}

algebra_optr create_is2_2_query() {
    auto personId = 65;
    auto maxHops = 3;
    result_set rs;



    auto expr = Scan(&rs, "Person",
            Filter(EQ(Key("id"), Int(personId)),
                    ForeachRship(RSHIP_DIR::TO, {}, ":hasCreator",
                            Limit(10,
                                Expand(EXPAND::IN, "Post",
                                        Project({{2, "id", FTYPE::INT}, {2, "content", FTYPE::STRING}, {2, "creationDate", FTYPE::INT},
                                                 {2, "id", FTYPE::INT}, {0, "id", FTYPE::INT}, {0, "firstName", FTYPE::STRING}, {0, "lastName", FTYPE::STRING}},
                                                Sort([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                                                    if(boost::get<int>(qr1[2]) == boost::get<int>(qr2[2]))
                                                        return boost::get<int>(qr1[0]) > boost::get<int>(qr2[0]);
                                                    return boost::get<int>(qr1[2]) > boost::get<int>(qr2[2]); }, Collect(rs))))))));

    return expr;
}

algebra_optr create_is3_query() {
    auto personId = 933;
    result_set rs;



    auto expr = Scan(&rs, "Person",
                     Filter(EQ(Key("id"), Int(personId)),
                            ForeachRship(RSHIP_DIR::FROM, {}, ":KNOWS",
                                Expand(EXPAND::OUT, "Person",
                                    Project({{2, "id", FTYPE::INT}, {2, "firstName", FTYPE::STRING},
                                                    {2, "lastName", FTYPE::STRING}, {1,"creationDate", FTYPE::INT}},
                                                            Sort([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                                                                     auto t1 = boost::get<int>(qr1[3]);
                                                                     auto t2 = boost::get<int>(qr2[3]);
                                                                     if(t1 == t2)
                                                                         return boost::get<int>(qr1[0]) < boost::get<int>(qr2[0]);
                                                                     return t1 > t2; },
                                                                    Collect(rs)))))));
    return expr;
}

algebra_optr create_is4_query() {
    auto postId = 13743895;
    result_set rs;



    auto expr = Scan(&rs, "Post",
                     Filter(EQ(Key("id"), Int(postId)),
                            Project({{0,"creationDate",FTYPE::INT}, {0, "content", FTYPE::STRING}},
                                    Collect(rs))));
    return expr;
}

algebra_optr create_is5_query() {
    auto commentId = 12362343;
    result_set rs;



    auto expr = Scan(&rs, "Comment",
                     Filter(EQ(Key("id"), Int(commentId)),
                            ForeachRship(RSHIP_DIR::FROM, {}, ":hasCreator",
                                Expand(EXPAND::OUT, "Person",
                                    Project({{2,"id",FTYPE::INT}, {2,"firstName",FTYPE::STRING}, {2,"lastName",FTYPE::STRING}},
                                        Collect(rs))))));
    return expr;
}

algebra_optr create_is6_1_query() {
    auto commentId = 16492677;
    auto postID = 16492674;
    auto maxHops = 3;
    result_set rs;

    auto expr = Scan(&rs, "Post",
            Filter(EQ(Key("id"), Int(postID)),
                    ForeachRship(RSHIP_DIR::TO, {}, ":containerOf",
                            Expand(EXPAND::IN, "Forum",
                                    ForeachRship(RSHIP_DIR::FROM, {}, ":hasModerator",
                                            Expand(EXPAND::OUT, "Person",
                                                    Project({{2, "id", FTYPE::INT}, {2, "title", FTYPE::STRING}, {4, "id", FTYPE::INT},
                                                             {4, "firstName", FTYPE::STRING}, {4, "lastName", FTYPE::STRING}},
                                                            Collect(rs))))))));

    return expr;
}

algebra_optr create_is6_2_query() {
    auto commentId = 16492677;
    auto postID = 16492674;
    auto maxHops = 3;
    result_set rs;

    auto expr = Scan(&rs, "Post",
                     Filter(EQ(Key("id"), Int(commentId)),
                            ForeachRship(RSHIP_DIR::FROM, {}, ":replyOf",
                                         Expand(EXPAND::OUT, "Post",
                                                ForeachRship(RSHIP_DIR::TO, {}, ":containerOf",
                                                             Expand(EXPAND::IN, "Forum",
                                                                    ForeachRship(RSHIP_DIR::FROM, {}, ":hasModerator",
                                                                            Expand(EXPAND::OUT, "Person",
                                                                    Project({{4, "id", FTYPE::INT}, {4, "title", FTYPE::STRING}, {6, "id", FTYPE::INT},
                                                                             {6, "firstName", FTYPE::STRING}, {6, "lastName", FTYPE::STRING}},
                                                                            Collect(rs))))))))));

    return expr;
}

algebra_optr create_is7_1_query() {
    auto commentId = 16492676;
    result_set rs;
    auto expr1 = Scan(&rs, "Comment",
                      Filter(EQ(Key("id"),Int(commentId)),
                             ForeachRship(RSHIP_DIR::FROM, {}, ":hasCreator",
                                          Expand(EXPAND::OUT, "Person", End()))));

    auto expr2 = Scan(&rs, "Comment",
            Filter(EQ(Key("id"), Int(commentId)),
                    ForeachRship(RSHIP_DIR::TO, {}, ":replyOf",
                            Expand(EXPAND::IN, "Comment",
                                    ForeachRship(RSHIP_DIR::FROM, {}, ":hasCreator",
                                            Expand(EXPAND::OUT, "Person",
                                                    Join(JOIN_OP::LEFT_OUTER, {4, 2},
                                                            Project({{2, "id", FTYPE::INT}, {2, "content", FTYPE::STRING},
                                                                   /*  {2, "creationDate", FTYPE::DATE},*/ {4, "id", FTYPE::INT},
                                                                     {4, "firstName", FTYPE::STRING}, {4, "lastName", FTYPE::STRING}, {8,"",FTYPE::BOOLEAN}},
                                                                    Sort([&](const qr_tuple &qr1, const qr_tuple &qr2) {
                                                                       /* if (boost::get<boost::posix_time::ptime>(qr1[2]) == boost::get<boost::posix_time::ptime>(qr2[2])) */
                                                                            return boost::get<uint64_t>(qr1[0]) > boost::get<uint64_t>(qr2[0]);
                                                                       /* return boost::get<boost::posix_time::ptime>(qr1[2]) < boost::get<boost::posix_time::ptime>(qr2[2]); */}, Collect(rs))), expr1)))))));

    return expr2;
}
