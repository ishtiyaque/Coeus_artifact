#pragma once

#include "pir.hpp"
#include <map>
#include <memory>
#include <vector>
#include "pir_client.hpp"
#include <pthread.h>
#include <chrono>

using namespace std;
using namespace seal;
using namespace seal::util;


#define MIN(a,b) ((a) < (b)) ? (a) : (b)

class ThreadArgument;
void *pir(void *arg);
void *expand_thread(void *arg);


class PIRServer {
  public:
    PIRServer(const seal::EncryptionParameters &params, const PirParams &pir_params, int _num_threads=1  );

    // NOTE: server takes over ownership of db and frees it when it exits.
    // Caller cannot free db
    void set_database(std::unique_ptr<std::vector<seal::Plaintext>> &&db);
    void set_database(const std::unique_ptr<const std::uint8_t[]> &bytes, std::uint64_t ele_num, std::uint64_t ele_size);
    void preprocess_database();

    std::vector<seal::Ciphertext> expand_query(
            const seal::Ciphertext &encrypted, std::uint32_t m, uint32_t client_id);

    PirReply generate_reply(PirQuery query, std::uint32_t client_id);

    void set_galois_key(std::uint32_t client_id, seal::GaloisKeys galkey);

    uint64_t expansion_time;
    uint64_t query_ntt_time;
    uint64_t inter_db_ntt_time;
    uint64_t mult_time;
    uint64_t add_time;
    uint64_t inv_ntt_time;
    uint64_t inter_db_construction_time;
    int num_thread;

    pthread_t *threads;
    ThreadArgument *args;

    vector<uint64_t> nvec;

    //vector<Plaintext> *cur = db_.get();
    vector<Plaintext> intermediate_plain; 
    //Plaintext *intermediate_plain;
    //vector<Ciphertext> final_result;
    vector<vector<Ciphertext>> partial_results;
    Ciphertext  *intermediateCtxts;
    vector< vector<Ciphertext> > expanded_query; 

    pthread_mutex_t request_received_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;

    pthread_barrier_t first_phase_barrier;
    pthread_barrier_t second_phase_barrier;



    bool request_received;


  //private:
    seal::EncryptionParameters params_; // SEAL parameters
    PirParams pir_params_;              // PIR parameters
    std::unique_ptr<Database> db_;
    bool is_db_preprocessed_;
    std::map<int, seal::GaloisKeys> galoisKeys_;
    std::unique_ptr<seal::Evaluator> evaluator_;

    void decompose_to_plaintexts_ptr(const seal::Ciphertext &encrypted, seal::Plaintext *plain_ptr, int logt);
    std::vector<seal::Plaintext> decompose_to_plaintexts(const seal::Ciphertext &encrypted);
    void multiply_power_of_X(const seal::Ciphertext &encrypted, seal::Ciphertext &destination,
                             std::uint32_t index);
};


class ThreadArgument{
public:
    PIRServer *server;
    int thread_id;
};

class ExpandArgument {
public:
    PIRServer *server;
    int thread_id;
    vector<Ciphertext> query;
    
};