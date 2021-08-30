#include "pir_server.hpp"
#include "pir_client.hpp"

using namespace std;
using namespace seal;
using namespace seal::util;

PIRServer::PIRServer(const EncryptionParameters &params, const PirParams &pir_params, int _num_threads) : params_(params),
                                                                                                          pir_params_(pir_params),
                                                                                                          is_db_preprocessed_(false)
{
    auto context = SEALContext::Create(params, false);
    evaluator_ = make_unique<Evaluator>(context);
    expansion_time = 0;
    query_ntt_time = 0;
    inter_db_ntt_time = 0;
    mult_time = 0;
    add_time = 0;
    inv_ntt_time = 0;
    inter_db_construction_time = 0;

    num_thread = _num_threads;

    request_received=false;

    pthread_barrier_init(&first_phase_barrier,NULL, num_thread);
    pthread_barrier_init(&second_phase_barrier,NULL, num_thread);

    vector<Ciphertext> dummy;
    for(int i = 0; i < num_thread;i++) {
        partial_results.push_back(dummy);
    }

    threads = new pthread_t[num_thread];
    args = new ThreadArgument[num_thread];

    for(int i = 0; i < num_thread;i++) {
        args[i].thread_id = i;
        args[i].server = this;
        if(pthread_create(&threads[i], NULL, pir, (void *)&args[i])) {
            printf("Error creating thread\n");
            exit(1);
        }

    }
}

void PIRServer::preprocess_database()
{
    if (!is_db_preprocessed_)
    {

        for (uint32_t i = 0; i < db_->size(); i++)
        {
            evaluator_->transform_to_ntt_inplace(
                db_->operator[](i), params_.parms_id());
        }

        is_db_preprocessed_ = true;
    }
}

// Server takes over ownership of db and will free it when it exits
void PIRServer::set_database(unique_ptr<vector<Plaintext>> &&db)
{
    if (!db)
    {
        throw invalid_argument("db cannot be null");
    }

    db_ = move(db);
    is_db_preprocessed_ = false;
}

void PIRServer::set_database(const std::unique_ptr<const std::uint8_t[]> &bytes,
                             uint64_t ele_num, uint64_t ele_size)
{

    uint32_t logt = floor(log2(params_.plain_modulus().value()));
    uint32_t N = params_.poly_modulus_degree();

    // number of FV plaintexts needed to represent all elements
    uint64_t total = plaintexts_per_db(logt, N, ele_num, ele_size);

    // number of FV plaintexts needed to create the d-dimensional matrix
    uint64_t prod = 1;
    for (uint32_t i = 0; i < pir_params_.nvec.size(); i++)
    {
        prod *= pir_params_.nvec[i];
    }
    uint64_t matrix_plaintexts = prod;
    assert(total <= matrix_plaintexts);

    auto result = make_unique<vector<Plaintext>>();
    result->reserve(matrix_plaintexts);

    uint64_t ele_per_ptxt = elements_per_ptxt(logt, N, ele_size);
    uint64_t bytes_per_ptxt = ele_per_ptxt * ele_size;

    uint64_t db_size = ele_num * ele_size;

    uint64_t coeff_per_ptxt = ele_per_ptxt * coefficients_per_element(logt, ele_size);
    assert(coeff_per_ptxt <= N);

    //cout << "Server: total number of FV plaintext = " << total << endl;
    //cout << "Server: elements packed into each plaintext " << ele_per_ptxt << endl;

    uint32_t offset = 0;

    for (uint64_t i = 0; i < total; i++)
    {

        uint64_t process_bytes = 0;

        if (db_size <= offset)
        {
            break;
        }
        else if (db_size < offset + bytes_per_ptxt)
        {
            process_bytes = db_size - offset;
        }
        else
        {
            process_bytes = bytes_per_ptxt;
        }

        // Get the coefficients of the elements that will be packed in plaintext i
        vector<uint64_t> coefficients = bytes_to_coeffs(logt, bytes.get() + offset, process_bytes);
        offset += process_bytes;

        uint64_t used = coefficients.size();

        assert(used <= coeff_per_ptxt);

        // Pad the rest with 1s
        for (uint64_t j = 0; j < (N - used); j++)
        {
            coefficients.push_back(1);
        }

        Plaintext plain;
        vector_to_plaintext(coefficients, plain);
        // cout << i << "-th encoded plaintext = " << plain.to_string() << endl;
        result->push_back(move(plain));
    }

    // Add padding to make database a matrix
    uint64_t current_plaintexts = result->size();
    assert(current_plaintexts <= total);

#ifdef DEBUG
    cout << "adding: " << matrix_plaintexts - current_plaintexts
         << " FV plaintexts of padding (equivalent to: "
         << (matrix_plaintexts - current_plaintexts) * elements_per_ptxt(logtp, N, ele_size)
         << " elements)" << endl;
#endif

    vector<uint64_t> padding(N, 1);

    for (uint64_t i = 0; i < (matrix_plaintexts - current_plaintexts); i++)
    {
        Plaintext plain;
        vector_to_plaintext(padding, plain);
        result->push_back(plain);
    }

    set_database(move(result));
}

void PIRServer::set_galois_key(std::uint32_t client_id, seal::GaloisKeys galkey)
{
    galkey.parms_id() = params_.parms_id();
    galoisKeys_[client_id] = galkey;
}







PirReply PIRServer::generate_reply(PirQuery query, uint32_t client_id)
{

    nvec = pir_params_.nvec;
    auto coeff_count = params_.poly_modulus_degree();


    for(int i = 0; i < num_thread;i++) {
        partial_results[i] = vector<Ciphertext>(pir_params_.expansion_ratio);
    }
    intermediateCtxts = new Ciphertext[nvec[1]];
    // vector< vector<Ciphertext> > expanded_query;
    auto pool = MemoryManager::GetPool();


    //intermediate_plain.reserve(pir_params_.expansion_ratio * nvec[1]);
    Plaintext pt;
    for(int i = 0; i < pir_params_.expansion_ratio * nvec[1];i++) {
        intermediate_plain.emplace_back(pt);           // Allocating enough space
    }

    expanded_query = vector< vector<Ciphertext> >(nvec.size());
    int N = params_.poly_modulus_degree();

    int logt = floor(log2(params_.plain_modulus().value()));

    auto time_pre_e = chrono::high_resolution_clock::now();

    ExpandArgument exp_arg[nvec.size()];
    pthread_t expand_threads[nvec.size()];
 
    for(int i = 0; i < nvec.size();i++) {
        exp_arg[i].thread_id = i;
        exp_arg[i].server = this;
        exp_arg[i].query = query[i];
        if(pthread_create(&expand_threads[i], NULL, expand_thread, (void *)&exp_arg[i])) {
            printf("Error creating thread\n");
            exit(1);
        }

    }
    for(int i = 0;i<nvec.size();i++) {
        pthread_join(expand_threads[i], NULL);
    }
    auto time_post_e = chrono::high_resolution_clock::now();
    expansion_time += chrono::duration_cast<chrono::microseconds>(time_post_e - time_pre_e).count();


    auto time_pre_s = chrono::high_resolution_clock::now();
    for (int i = 0; i < expanded_query.size(); i++)
    {
        for (uint32_t jj = 0; jj < expanded_query[i].size(); jj++)
        {
            evaluator_->transform_to_ntt_inplace(expanded_query[i][jj]);
        }
    }
    auto time_post_s = chrono::high_resolution_clock::now();
    query_ntt_time += chrono::duration_cast<chrono::microseconds>(time_post_s - time_pre_s).count();

    pthread_mutex_lock(&request_received_lock);
    request_received = true;
    pthread_cond_broadcast(&request_received_cond);
    pthread_mutex_unlock(&request_received_lock);

    for(int i = 0;i<num_thread;i++) {
        pthread_join(threads[i], NULL);
    }
 
    return partial_results[0];
}

void *pir(void *arg)
{
    int my_id = ((ThreadArgument *)arg)->thread_id;
    PIRServer *server = ((ThreadArgument *)arg)->server;
    

    //vector<uint64_t> nvec = server->nvec;

    int logt = floor(log2(server->params_.plain_modulus().value()));

    while (!server->request_received) {
        pthread_mutex_lock(&server->request_received_lock);
        pthread_cond_wait(&server->request_received_cond, &server->request_received_lock);
        pthread_mutex_unlock(&server->request_received_lock);
    }

vector<Plaintext> *current_db = server->db_.get();
    int client_factor = floor((double)server->nvec[1] /server->num_thread);

    int start_id = my_id * client_factor;
    int remaining = server->nvec[1] % server->num_thread;
    if (remaining > my_id)
    {
        start_id += my_id;
        client_factor++;
    }
    else
    {
        start_id += remaining;
    }
    int end_id = MIN((start_id + client_factor), (server->nvec[1]));
    //printf("thread %d start %d end %d \n",my_id,start_id, end_id);

    auto tempplain = util::allocate<Plaintext>(
        server->pir_params_.expansion_ratio * client_factor,
        MemoryManager::GetPool(), server->params_.poly_modulus_degree());


    Ciphertext temp;

    for (uint64_t k = start_id; k < end_id; k++)
    {
        server->evaluator_->multiply_plain(server->expanded_query[0][0], (*current_db)[k], server->intermediateCtxts[k]);
        for (uint64_t j = 1; j < server->nvec[0]; j++)
        {
            server->evaluator_->multiply_plain(server->expanded_query[0][j], (*current_db)[k + j * server->nvec[1]], temp);
            server->evaluator_->add_inplace(server->intermediateCtxts[k], temp); // Adds to first component.
        }
        server->evaluator_->transform_from_ntt_inplace(server->intermediateCtxts[k]);
        server->decompose_to_plaintexts_ptr(server->intermediateCtxts[k],
                                    tempplain.get() + (k-start_id) * server->pir_params_.expansion_ratio, logt);
        for (uint32_t jj = 0; jj < server->pir_params_.expansion_ratio; jj++)
        {
            //auto offset = rr * server->pir_params_.expansion_ratio + jj;
            server->intermediate_plain[k*server->pir_params_.expansion_ratio + jj] =(tempplain[(k-start_id) * server->pir_params_.expansion_ratio + jj]);
            server->evaluator_->transform_to_ntt_inplace(server->intermediate_plain[k*server->pir_params_.expansion_ratio + jj], server->params_.parms_id());
        }

    }
    //printf("thread %d finished first step\n",my_id);

        pthread_barrier_wait(&server->first_phase_barrier);


   current_db = &server->intermediate_plain;

    for (uint64_t k = 0; k < server->pir_params_.expansion_ratio; k++)
    {
        server->evaluator_->multiply_plain(server->expanded_query[1][start_id], (*current_db)[k], server->partial_results[my_id][k]);
        for (uint64_t j = start_id+1; j < end_id; j++)
        {
            server->evaluator_->multiply_plain(server->expanded_query[1][j], (*current_db)[k + j * server->pir_params_.expansion_ratio], temp);
            server->evaluator_->add_inplace(server->partial_results[my_id][k], temp); // Adds to first component.
        }

    }

    pthread_barrier_wait(&server->second_phase_barrier);
    if(my_id >= server->pir_params_.expansion_ratio) {
        return NULL;
    }
    if(server->num_thread >= server->pir_params_.expansion_ratio) {
        start_id = my_id;
        end_id = start_id+1;
    } else {
        client_factor = floor((double)server->pir_params_.expansion_ratio /server->num_thread);
        start_id = my_id * client_factor;
        remaining = server->pir_params_.expansion_ratio % server->num_thread;
        if (remaining > my_id)
        {
            start_id += my_id;
            client_factor++;
        }
        else
        {
            start_id += remaining;
        }
        end_id = start_id+client_factor;

    }

    for(int i = start_id; i < end_id;i++) {
        for(int j = 1;j<server->num_thread;j++) {
            server->evaluator_->add_inplace(server->partial_results[0][i],server->partial_results[j][i]);
        }
        server->evaluator_->transform_from_ntt_inplace(server->partial_results[0][i]);
    }
    return NULL;
}

void *expand_thread(void *arg) {
    int my_id = ((ExpandArgument*)arg)->thread_id;
    PIRServer *server = ((ExpandArgument*)arg)->server;
    vector<Ciphertext> query = ((ExpandArgument*)arg)->query;

    vector<Ciphertext> single_dim_exp_query;

    uint64_t n_i = server->nvec[my_id];
    //cout << "Server: n_i = " << n_i << endl;
    //cout << "Server: expanding " << query.size() << " query ctxts" << endl;
    uint64_t N = server->params_.poly_modulus_degree();
    for (uint32_t j = 0; j < query.size(); j++)
    {
        uint64_t total = N;
        if (j == query.size() - 1)
        {
            total = ((n_i - 1) % N) + 1;
        }
        //cout << "total " << total << endl;
        //cout << "-- expanding one query ctxt into " << total << " ctxts " << endl;
        vector<Ciphertext> expanded_query_part = server->expand_query(query[j], total, 0); // client_id 0 hard coded
        single_dim_exp_query.insert(single_dim_exp_query.end(), std::make_move_iterator(expanded_query_part.begin()),
                                    std::make_move_iterator(expanded_query_part.end()));
        expanded_query_part.clear();
    }
    server->expanded_query[my_id] = single_dim_exp_query;
    //cout << "Server: expansion done " << endl;
    if (server->expanded_query[my_id].size() != n_i)
    {
        cout << " size mismatch!!! " << server->expanded_query.size() << ", " << n_i << endl;
    }

    return NULL;

}
inline vector<Ciphertext> PIRServer::expand_query(const Ciphertext &encrypted, uint32_t m,
                                                  uint32_t client_id)
{

#ifdef DEBUG
    uint64_t plainMod = params_.plain_modulus().value();
    cout << "PIRServer side plain modulus = " << plainMod << endl;
#endif

    GaloisKeys &galkey = galoisKeys_[client_id];

    // Assume that m is a power of 2. If not, round it to the next power of 2.
    uint32_t logm = ceil(log2((uint32_t)m));
    //cout << "m " << m << endl;
    //cout << "logm " << logm << endl;
    Plaintext two("2");

    vector<int> galois_elts;
    auto n = params_.poly_modulus_degree();
    if (logm > ceil(log2(n)))
    {
        throw logic_error("m > n is not allowed.");
    }
    for (int i = 0; i < ceil(log2(n)); i++)
    {
        galois_elts.push_back((n + exponentiate_uint64(2, i)) / exponentiate_uint64(2, i));
    }
    // cout<<"Printing galois elts from expand";
    // for(int i = 0;i<galois_elts.size();i++) {
    //     cout<<galois_elts[i]<<"\t";
    // }
    // cout<<endl<<endl;

    vector<Ciphertext> temp;
    temp.push_back(encrypted);
    Ciphertext tempctxt;
    Ciphertext tempctxt_rotated;
    Ciphertext tempctxt_shifted;
    Ciphertext tempctxt_rotatedshifted;

    for (uint32_t i = 0; i < logm - 1; i++)
    {
        vector<Ciphertext> newtemp(temp.size() << 1);
        // temp[a] = (j0 = a (mod 2**i) ? ) : Enc(x^{j0 - a}) else Enc(0).  With
        // some scaling....
        int index_raw = (n << 1) - (1 << i);
        int index = (index_raw * galois_elts[i]) % (n << 1);

        for (uint32_t a = 0; a < temp.size(); a++)
        {
            //cout<<"calling apply_galois, i = "<<i<<" galois_elts[i] "<<galois_elts[i]<<endl;
            evaluator_->apply_galois(temp[a], galois_elts[i], galkey, tempctxt_rotated);

            //cout << "rotate " << client.decryptor_->invariant_noise_budget(tempctxt_rotated) << ", ";

            evaluator_->add(temp[a], tempctxt_rotated, newtemp[a]);
            multiply_power_of_X(temp[a], tempctxt_shifted, index_raw);

            //cout << "mul by x^pow: " << client.decryptor_->invariant_noise_budget(tempctxt_shifted) << ", ";

            multiply_power_of_X(tempctxt_rotated, tempctxt_rotatedshifted, index);

            // cout << "mul by x^pow: " << client.decryptor_->invariant_noise_budget(tempctxt_rotatedshifted) << ", ";

            // Enc(2^i x^j) if j = 0 (mod 2**i).
            evaluator_->add(tempctxt_shifted, tempctxt_rotatedshifted, newtemp[a + temp.size()]);
        }
        temp = newtemp;
        /*
        cout << "end: "; 
        for (int h = 0; h < temp.size();h++){
            cout << client.decryptor_->invariant_noise_budget(temp[h]) << ", "; 
        }
        cout << endl; 
        */
    }
    // Last step of the loop
    vector<Ciphertext> newtemp(temp.size() << 1);
    int index_raw = (n << 1) - (1 << (logm - 1));
    int index = (index_raw * galois_elts[logm - 1]) % (n << 1);
    for (uint32_t a = 0; a < temp.size(); a++)
    {
        if (a >= (m - (1 << (logm - 1))))
        {                                                         // corner case.
            evaluator_->multiply_plain(temp[a], two, newtemp[a]); // plain multiplication by 2.
            // cout << client.decryptor_->invariant_noise_budget(newtemp[a]) << ", ";
        }
        else
        {
            evaluator_->apply_galois(temp[a], galois_elts[logm - 1], galkey, tempctxt_rotated);
            evaluator_->add(temp[a], tempctxt_rotated, newtemp[a]);
            multiply_power_of_X(temp[a], tempctxt_shifted, index_raw);
            multiply_power_of_X(tempctxt_rotated, tempctxt_rotatedshifted, index);
            evaluator_->add(tempctxt_shifted, tempctxt_rotatedshifted, newtemp[a + temp.size()]);
        }
    }

    vector<Ciphertext>::const_iterator first = newtemp.begin();
    vector<Ciphertext>::const_iterator last = newtemp.begin() + m;
    vector<Ciphertext> newVec(first, last);
    return newVec;
}

inline void PIRServer::multiply_power_of_X(const Ciphertext &encrypted, Ciphertext &destination,
                                           uint32_t index)
{

    auto coeff_mod_count = params_.coeff_modulus().size();
    auto coeff_count = params_.poly_modulus_degree();
    auto encrypted_count = encrypted.size();

    //cout << "coeff mod count for power of X = " << coeff_mod_count << endl;
    //cout << "coeff count for power of X = " << coeff_count << endl;

    // First copy over.
    destination = encrypted;

    // Prepare for destination
    // Multiply X^index for each ciphertext polynomial
    for (int i = 0; i < encrypted_count; i++)
    {
        for (int j = 0; j < coeff_mod_count; j++)
        {
            negacyclic_shift_poly_coeffmod(encrypted.data(i) + (j * coeff_count),
                                           coeff_count, index,
                                           params_.coeff_modulus()[j],
                                           destination.data(i) + (j * coeff_count));
        }
    }
}

inline void PIRServer::decompose_to_plaintexts_ptr(const Ciphertext &encrypted, Plaintext *plain_ptr, int logt)
{

    vector<Plaintext> result;
    auto coeff_count = params_.poly_modulus_degree();
    auto coeff_mod_count = params_.coeff_modulus().size();
    auto encrypted_count = encrypted.size();

    uint64_t t1 = 1 << logt; //  t1 <= t.

    uint64_t t1minusone = t1 - 1;
    // A triple for loop. Going over polys, moduli, and decomposed index.

    for (int i = 0; i < encrypted_count; i++)
    {
        const uint64_t *encrypted_pointer = encrypted.data(i);
        for (int j = 0; j < coeff_mod_count; j++)
        {
            // populate one poly at a time.
            // create a polynomial to store the current decomposition value
            // which will be copied into the array to populate it at the current
            // index.
            double logqj = log2(params_.coeff_modulus()[j].value());
            //int expansion_ratio = ceil(logqj + exponent - 1) / exponent;
            int expansion_ratio = ceil(logqj / logt);
            // cout << "local expansion ratio = " << expansion_ratio << endl;
            uint64_t curexp = 0;
            for (int k = 0; k < expansion_ratio; k++)
            {
                // Decompose here
                for (int m = 0; m < coeff_count; m++)
                {
                    plain_ptr[i * coeff_mod_count * expansion_ratio + j * expansion_ratio + k][m] =
                        (*(encrypted_pointer + m + (j * coeff_count)) >> curexp) & t1minusone;
                }
                curexp += logt;
            }
        }
    }
}

vector<Plaintext> PIRServer::decompose_to_plaintexts(const Ciphertext &encrypted)
{
    vector<Plaintext> result;
    auto coeff_count = params_.poly_modulus_degree();
    auto coeff_mod_count = params_.coeff_modulus().size();
    auto plain_bit_count = params_.plain_modulus().bit_count();
    auto encrypted_count = encrypted.size();

    // Generate powers of t.
    uint64_t plainMod = params_.plain_modulus().value();

    // A triple for loop. Going over polys, moduli, and decomposed index.
    for (int i = 0; i < encrypted_count; i++)
    {
        const uint64_t *encrypted_pointer = encrypted.data(i);
        for (int j = 0; j < coeff_mod_count; j++)
        {
            // populate one poly at a time.
            // create a polynomial to store the current decomposition value
            // which will be copied into the array to populate it at the current
            // index.
            int logqj = log2(params_.coeff_modulus()[j].value());
            int expansion_ratio = ceil(logqj / log2(plainMod));

            // cout << "expansion ratio = " << expansion_ratio << endl;
            uint64_t cur = 1;
            for (int k = 0; k < expansion_ratio; k++)
            {
                // Decompose here
                Plaintext temp(coeff_count);
                transform(encrypted_pointer + (j * coeff_count),
                          encrypted_pointer + ((j + 1) * coeff_count),
                          temp.data(),
                          [cur, &plainMod](auto &in) { return (in / cur) % plainMod; });

                result.emplace_back(move(temp));
                cur *= plainMod;
            }
        }
    }

    return result;
}
