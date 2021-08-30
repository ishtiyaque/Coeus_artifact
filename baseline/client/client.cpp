#include <globals.h>

int RESPONSE_CT = 0;
int QUERY_CT = 0;
string MASTER_IP;
int SCALE_FACTOR = 0;
int response_received = 0;
string CLIENT_IP;
int NUM_GROUP = 0;

int NUM_WORKER;

pthread_mutex_t response_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t response_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t warmup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t warmup_cond = PTHREAD_COND_INITIALIZER;

uint64_t query_gen_time = 0;

clock_t total_cpu_start_time, total_cpu_stop_time;

int warmup_count = 0;

uint64_t query_send_timestamp;
vector<uint64_t> response_rcv_timestamp;

Ciphertext *final_result;

void sendResponse(int start, int end, vector<vector<uint64_t>> ct);
void printReport();
int main(int argc, char **argv)
{

    int option;
    const char *optstring = "r:p:q:c:s:g:";
    while ((option = getopt(argc, argv, optstring)) != -1)
    {
        switch (option)
        {

        case 'r':
            RESPONSE_CT = stoi(optarg);
            break;
        case 'q':
            QUERY_CT = stoi(optarg);
            break;
        case 'c':
            CLIENT_IP = string(optarg);
            break;
        case 'p':
            MASTER_IP = string(optarg);
            break;
        case 's':
            SCALE_FACTOR = stoi(optarg);
            break;
        case 'g':
            NUM_GROUP = stoi(optarg);
            break;

        case '?':
            cout << "error optopt: " << optopt << endl;
            cout << "error opterr: " << opterr << endl;
            return 1;
        }
    }

    if (!RESPONSE_CT)
    {
        cout << "Missing -r\n";
        return 0;
    }
    if (!QUERY_CT)
    {
        cout << "Missing -q\n";
        return 0;
    }
    if (CLIENT_IP.size() < 7)
    {
        cout << "Missing -c\n";
        return 0;
    }
    if (MASTER_IP.size() < 7)
    {
        cout << "Missing -p\n";
        return 0;
    }
    if(!SCALE_FACTOR) {cout<<"Missing -s\n";return 0;}
    if(!NUM_GROUP) {cout<<"Missing -g\n";return 0;}


    NUM_WORKER = (QUERY_CT / SCALE_FACTOR) * NUM_GROUP;

    rpc::server response_server(string(CLIENT_IP), CLIENT_PORT);
    response_server.bind("sendResponse", sendResponse);
    response_server.async_run(1);

    rpc::client query_client(MASTER_IP, MASTER_PORT);

    chrono::high_resolution_clock::time_point time_start, time_end, total_start, total_end;

    EncryptionParameters parms(scheme_type::BFV);
    parms.set_poly_modulus_degree(N);
    //parms.set_coeff_modulus(CoeffModulus::BFVDefault(N));
    parms.set_coeff_modulus(CoeffModulus::Create(N, COEFF_MODULUS_SIZE));

    parms.set_plain_modulus(PlainModulus::Batching(N, PLAIN_BIT));
    //parms.set_plain_modulus(PLAIN_MODULUS);
    auto context = SEALContext::Create(parms);
    uint64_t plain_modulus = parms.plain_modulus().value();
    // cout << parms.plain_modulus().value() << endl;
    // cout << parms.coeff_modulus().at(0).value() << endl;
    // cout << parms.coeff_modulus().at(1).value() << endl;
    //print_parameters(context);
    //cout << endl;
    //cout << "Maximum size of co-efficient modulus: " << CoeffModulus::MaxBitCount(N) << endl;
    auto qualifiers = context->first_context_data()->qualifiers();
    //cout << "Batching enabled: " << boolalpha << qualifiers.using_batching << endl;
    //cout << "Fast plain lift enabled: " << qualifiers.using_fast_plain_lift << endl;

    KeyGenerator keygen(context);

    auto pid = context->first_parms_id();

    //PublicKey public_key = keygen.public_key();
    SecretKey secret_key = keygen.secret_key();

    Encryptor encryptor(context, secret_key);
    //evaluator = new Evaluator(context);
    Decryptor decryptor(context, secret_key);

    BatchEncoder batch_encoder(context);
    size_t slot_count = batch_encoder.slot_count();
    size_t row_size = slot_count / 2;

    vector<int> steps;
    steps.push_back(0);

    for (int i = 1; i < N / 2; i *= 2)
    {
        steps.push_back(i);
    }

    Serializable<GaloisKeys> gal_keys = keygen.galois_keys(steps);

    std::stringstream gkss;

    time_start = chrono::high_resolution_clock::now();

    int sz = gal_keys.save(gkss, compr_mode_type::deflate);
    time_end = chrono::high_resolution_clock::now();
    auto ser_time = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
    cout << "serialize time" << endl
         << ser_time << endl;
    string serialized_gal_key = gkss.str();

    final_result = new Ciphertext[RESPONSE_CT];
    Ciphertext query_ct[QUERY_CT];

    pthread_mutex_lock(&warmup_lock);
    while (warmup_count < NUM_WORKER)
    {
        pthread_cond_wait(&warmup_cond, &warmup_lock);
    }
    pthread_mutex_unlock(&warmup_lock);
    warmup_count = 0;

    {
        vector<uint64_t> zero_matrix(N, 0ULL);
        Plaintext zero_pt;
        batch_encoder.encode(zero_matrix, zero_pt);
        Serializable<Ciphertext> zero_ct = encryptor.encrypt_symmetric(zero_pt);
        for (int i = 0; i < RESPONSE_CT; i++)
        {
            encryptor.encrypt_symmetric(zero_pt, final_result[i]); // Allocating memory
        }

        stringstream dummy;
        zero_ct.save(dummy);
        string s = dummy.str();
        vector<string> v;
        v.push_back(s);
        for(int i = 0; i < 3;i++) {
            s = s+s;
        }
        query_client.call("sendQuery", s, v);
    }

    pthread_mutex_lock(&warmup_lock);
    while (warmup_count < NUM_WORKER)
    {
        pthread_cond_wait(&warmup_cond, &warmup_lock);
    }
    pthread_mutex_unlock(&warmup_lock);

    vector<string> serialized_query;
    vector<uint64_t> pod_matrix(N, 0ULL);
    
    total_cpu_start_time = clock();  
    time_start = chrono::high_resolution_clock::now();
    pod_matrix[5] = 1;
    pod_matrix[123] = 1;
    pod_matrix[6543] = 1;
    Plaintext query_pt;
    batch_encoder.encode(pod_matrix, query_pt);


    for (int i = 0; i < QUERY_CT; i++)
    {
        Serializable<Ciphertext> sct = encryptor.encrypt_symmetric(query_pt);
        stringstream querystream;
        sct.save(querystream);
        serialized_query.push_back(querystream.str());
    }
    time_end = chrono::high_resolution_clock::now();
    query_gen_time = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

    query_send_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    total_start = chrono::high_resolution_clock::now();
    query_client.async_call("sendQuery", serialized_gal_key, serialized_query);
    //cout<<"send time "<<send_time<<endl;

    pthread_mutex_lock(&response_lock);
    pthread_cond_wait(&response_cond, &response_lock);
    pthread_mutex_unlock(&response_lock);
    total_end = chrono::high_resolution_clock::now();
    total_cpu_stop_time = clock();
    auto latency = chrono::duration_cast<chrono::microseconds>(total_end - total_start).count();

    //cout << "All response received" << endl;
    cout << "latency (us) "<<endl << latency << endl;

    printReport();

    return 0;
}

void sendResponse(int start, int end, vector<vector<uint64_t>> ct)
{
    if (start == -1)
    {
        pthread_mutex_lock(&warmup_lock);
        warmup_count++;
        if (warmup_count == NUM_WORKER)
        {
            pthread_cond_signal(&warmup_cond);
        }
        pthread_mutex_unlock(&warmup_lock);
        return;
    }

    response_rcv_timestamp.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count());
    //cout << "received response from " << start << " to " << end << endl;
    for (int i = start; i < end; i++)
    {
        assert(ct[i - start].size() == (N * 2 * 2));
        std::copy(ct[i - start].begin(), ct[i - start].end(), final_result[i].data());
    }
    pthread_mutex_lock(&response_lock);
    response_received += (end - start);
    if (response_received == RESPONSE_CT)
    {
        pthread_cond_broadcast(&response_cond);
    }
    pthread_mutex_unlock(&response_lock);

    return;
}

void printReport()
{
    cout << "query send:" << endl;
    cout << query_send_timestamp << endl;
    cout << "response_received" << endl;
    for (int i = 0; i < response_rcv_timestamp.size(); i++)
    {
        cout << response_rcv_timestamp[i] <<"\t";
    }
    cout<<"\nTotal CPU time (sec) "<<endl<<((float)total_cpu_stop_time-total_cpu_start_time)/CLOCKS_PER_SEC;

    return;
}