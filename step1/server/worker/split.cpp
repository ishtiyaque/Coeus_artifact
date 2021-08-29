#include<globals.h>

//#define DB_ELEMENTS ((N/SPLIT_FACTOR)*(RESPONSE_CT))

int NUM_THREAD = 0;
int NUM_TOTAL_WORKER = 0;
int WORKER_ID = -1;
int RESPONSE_CT = 0;
int SPLIT_FACTOR = 0;
int NUM_WORKER_PER_GROUP =0;
int NUM_GROUP= 0;
int GROUP_ID;

int DB_ELEMENTS=0;

int group_start, group_end;

string CLIENT_IP;

string *worker_ip;
string serialized_gal_key;
string serialized_query;

clock_t total_cpu_start_time, total_cpu_stop_time;

int agg_start;
int agg_end;

pthread_t *threads;
pthread_barrier_t mult_barrier;
pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t request_received_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t warmup_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t warmup_received_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t agg_done_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t aggregation_lock = PTHREAD_MUTEX_INITIALIZER;

int aggregation_count = 0;
bool warmup_received = false;

rpc::client **worker_clients;

Ciphertext *aggregation_result;
Ciphertext **partial_results;
Ciphertext *final_result;
Ciphertext *temp_ct;

Evaluator *evaluator;
BatchEncoder *batch_encoder;
vector<Plaintext> encoded_db;
parms_id_type pid;

vector<uint64_t> partial_result_recv_timestamp;
vector<uint64_t> partial_result_send_timestamp;

uint64_t query_rcv_timestamp, response_send_timestamp;
uint64_t processing_start_timestamp, processing_end_timestamp;
uint64_t aggregation_end_timestamp, response_send_end_timestamp;

uint64_t worker_tot_rot, worker_rot_start, worker_rot_end;


class thread_argument
{
public:
    int thread_id;
    Ciphertext query;
    Ciphertext *thread_result;
    GaloisKeys *gal_keys;
};

void *populate_db(void *arg);
vector<vector<uint64_t>> get_encoded_block(uint64_t **matrix, uint rstart, uint cstart);
void *thread_multiply(void *arg);
void sendPartialResult(vector< vector<uint64_t> >);
void sendQuery( string _serialized_gal_key, string _serialized_query);
void readWorkerIp();
void printReport();
void sendClientWarmUp(rpc::client &response_client);
//void sendResultToOthers();
//void sendToClient(rpc::client &response_client);

#define SEND_RESULT_TO_OTHERS()  \
{                                                       \
    int ct_per_worker = RESPONSE_CT / NUM_WORKER_PER_GROUP;   \
    int remaining = RESPONSE_CT % NUM_WORKER_PER_GROUP;       \
    int itr = 0;                                        \
    vector<uint64_t> ct(2*2*N);                         \
    for(int i = group_start; i < group_end;i++) {                \
        int start = itr;                                \
        int end = start + ct_per_worker;                \
        if (remaining > (i%NUM_WORKER_PER_GROUP) )                             \
        {                                               \
            end++;                                      \
        }                                               \
        itr = end;                                      \
        if(i != WORKER_ID) {                            \
            vector< vector<uint64_t> > result;          \
            for(int j = start; j < end;j++) {           \
                std::copy(final_result[j].data(), final_result[j].data() + 2*2*N, ct.begin());  \
                result.push_back(ct);                   \
            }                                           \
            worker_clients[i - group_start]->async_call("sendPartialResult", result);                         \
            partial_result_send_timestamp.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count()); \
        }                                               \
    }                                                   \
}                                                       \


#define SEND_TO_CLIENT()   \
{                                                       \
    vector<uint64_t> ct(N*2*2);                         \
    vector<vector<uint64_t> > result;                   \
    for(int i = agg_start; i < agg_end;i++) {           \
        std::copy(aggregation_result[i-agg_start].data(), aggregation_result[i-agg_start].data()+2*2*N, ct.begin()); \
        result.push_back(ct);                           \
    }                                                   \
    response_send_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count(); \
    response_client.call("sendResponse", agg_start, agg_end, result);   \
}                                                       \


int main(int argc, char **argv) {

    int option;
    const char *optstring = "r:w:t:i:c:s:g:";
    while ((option = getopt(argc, argv, optstring)) != -1) {
	switch (option) {

        case 'r':
            RESPONSE_CT = stoi(optarg);
            break;

        case 'w':
            NUM_WORKER_PER_GROUP = stoi(optarg);
            break;
        case 't':
            NUM_THREAD = stoi(optarg);
            break;
        case 'i':
            WORKER_ID = stoi(optarg);
            break;

        case 'c':
            CLIENT_IP = string(optarg);
            break;  
        case 's':
            SPLIT_FACTOR = stoi(optarg);
            break;  
        case 'g':
            NUM_GROUP = stoi(optarg);
            break;

        case '?':
            cout<<"error optopt: "<<optopt<<endl;
            cout<<"error opterr: "<<opterr<<endl;
            return 1;
	    }
    }
    if(!RESPONSE_CT) {cout<<"Missing -r\n";return 0;}
    if(!NUM_WORKER_PER_GROUP) {cout<<"Missing -w\n";return 0;}
    if(!NUM_THREAD) {cout<<"Missing -t\n";return 0;}
    if(CLIENT_IP.size() < 7) {cout<<"Missing -c\n";return 0;}
    if(WORKER_ID < 0) {cout<<"Missing -i\n";return 0;}
    if(!SPLIT_FACTOR) {cout<<"Missing -s\n";return 0;}
    if(!NUM_GROUP) {cout<<"Missing -g\n";return 0;}

    NUM_TOTAL_WORKER = NUM_WORKER_PER_GROUP * NUM_GROUP;
    GROUP_ID = WORKER_ID/NUM_WORKER_PER_GROUP;
    group_start = GROUP_ID * NUM_WORKER_PER_GROUP;
    group_end = group_start + NUM_WORKER_PER_GROUP;


    worker_tot_rot = floor((double)((N/2)/(SPLIT_FACTOR/2)));
    worker_rot_start = ((WORKER_ID%NUM_WORKER_PER_GROUP)%(SPLIT_FACTOR/2)) * worker_tot_rot;
    int remaining = (N/2 ) % (SPLIT_FACTOR/2 );
    if (remaining > ((WORKER_ID%NUM_WORKER_PER_GROUP)%(SPLIT_FACTOR/2) ))
    {
        worker_rot_start += ((WORKER_ID%NUM_WORKER_PER_GROUP)%(SPLIT_FACTOR/2) );
        worker_tot_rot++;
    }
    else
    {
        worker_rot_start += remaining;
    }
    worker_rot_end = worker_rot_start + worker_tot_rot;

    DB_ELEMENTS = worker_tot_rot * RESPONSE_CT;

    readWorkerIp();
    rpc::server worker_server(string(worker_ip[WORKER_ID]), WORKER_WORKER_PORT + WORKER_ID);
    rpc::server query_server(string(worker_ip[WORKER_ID]), MASTER_WORKER_PORT + WORKER_ID);
    //cout<<"waiting for query at "<<worker_ip[WORKER_ID]<<":"<<MASTER_WORKER_PORT + WORKER_ID<<endl;
    worker_server.bind("sendPartialResult", sendPartialResult);
    worker_server.async_run(1);
    query_server.bind("sendQuery", sendQuery);
    query_server.async_run(1);
    rpc::client response_client(CLIENT_IP, CLIENT_PORT);

    int num_agg_ct = floor((double)(RESPONSE_CT) / NUM_WORKER_PER_GROUP);
    agg_start = (WORKER_ID % NUM_WORKER_PER_GROUP) * num_agg_ct;
    remaining = (RESPONSE_CT) % NUM_WORKER_PER_GROUP ;
    if (remaining > (WORKER_ID % NUM_WORKER_PER_GROUP))
    {
        agg_start += (WORKER_ID % NUM_WORKER_PER_GROUP) ;
        num_agg_ct++;
    }
    else
    {
        agg_start += remaining;
    }
    agg_end = agg_start + num_agg_ct;
    aggregation_result = new Ciphertext[agg_end - agg_start];
    chrono::high_resolution_clock::time_point time_start, time_end, total_start, total_end;
    srand(time(NULL));

    threads = new pthread_t[NUM_THREAD];
    encoded_db = vector<Plaintext>(DB_ELEMENTS);

    EncryptionParameters parms(scheme_type::BFV);
    parms.set_poly_modulus_degree(N);
    parms.set_coeff_modulus(CoeffModulus::Create(N, COEFF_MODULUS_SIZE));

    //parms.set_coeff_modulus({COEFF_MODULUS_54, COEFF_MODULUS_55});
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
    pid = context->first_parms_id();

    //PublicKey public_key = keygen.public_key();
    //SecretKey secret_key = keygen.secret_key();

    //Encryptor encryptor(context, secret_key);
    evaluator = new Evaluator(context);
    //Decryptor decryptor(context, secret_key);

    batch_encoder = new BatchEncoder(context);
    size_t slot_count = batch_encoder->slot_count();
    size_t row_size = slot_count / 2;

    final_result = new Ciphertext[RESPONSE_CT];
    partial_results = new Ciphertext*[NUM_THREAD];
    for(int i = 0; i < NUM_THREAD; i++) {
        partial_results[i] = new Ciphertext[RESPONSE_CT];
    }

    pthread_barrier_init(&mult_barrier,NULL, NUM_THREAD);
    thread_argument args[NUM_THREAD];
    Ciphertext query;
    //Ciphertext col_rotated_query;
    GaloisKeys gal_keys;

    for (int i = 0; i < NUM_THREAD; i++)
    {
        args[i].thread_id = i;
        args[i].thread_result = partial_results[i];
    }

    threads = new pthread_t[NUM_THREAD];
    for (int i = 0; i < NUM_THREAD; i++)
    {
        if (pthread_create(&threads[i], NULL, populate_db, (void *)(&args[i].thread_id)))
        {            
            printf("Error creating thread\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREAD; i++)
    {
        pthread_join(threads[i], NULL);
    }

    //printf("Completed DB population\n");

    sleep(15);

    worker_clients = new rpc::client*[NUM_WORKER_PER_GROUP];
    for(int i = group_start; i < group_end;i++) {
        if(i != WORKER_ID) {
            worker_clients[i - group_start] = new rpc::client(worker_ip[i], WORKER_WORKER_PORT+i);
        }
    }
    sendClientWarmUp(response_client);

    pthread_mutex_lock(&warmup_received_lock);
    while(!warmup_received){
        pthread_cond_wait(&warmup_received_cond, &warmup_received_lock);
    }
    pthread_mutex_unlock(&warmup_received_lock);

    temp_ct = new Ciphertext();
    stringstream dummy_stream(serialized_query);
    temp_ct->load(context, dummy_stream); // Alllocate memory
    temp_ct->is_ntt_form() = true;

    for(int i = 0; i < (agg_end - agg_start);i++) {
        aggregation_result[i] = *temp_ct; // Allocate memory
        aggregation_result[i].is_ntt_form() = true;
    }
    
    sendClientWarmUp(response_client);
    total_cpu_start_time = clock();

    pthread_mutex_lock(&request_received_lock);
    pthread_cond_wait(&request_received_cond, &request_received_lock);
    pthread_mutex_unlock(&request_received_lock);

    stringstream gkss(serialized_gal_key);
    stringstream qss(serialized_query);
    gal_keys.load(context, gkss);
    query.load(context, qss);
    if(((WORKER_ID%NUM_WORKER_PER_GROUP)%SPLIT_FACTOR) >= (SPLIT_FACTOR/2)) {
        evaluator->rotate_columns_inplace(query, gal_keys);
        //cout<<"rotated column"<<endl;
    }
    processing_start_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

    for (int i = 0; i < NUM_THREAD ; i++)
    {
        args[i].gal_keys = &gal_keys;
        args[i].query = query;
    }


    for (int i = 0; i < NUM_THREAD; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_multiply, (void *)(&args[i])))
        {
            printf("Error creating thread\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREAD; i++)
    {
        pthread_join(threads[i], NULL);
    }
    //cout<<"done computation\n";
    processing_end_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    SEND_RESULT_TO_OTHERS()
    //cout<<"sent result to other threads"<<endl;

    pthread_mutex_lock(&aggregation_lock);
    if(aggregation_count == 0) {
        for(int i = agg_start; i < agg_end;i++) {
            aggregation_result[i - agg_start] = final_result[i];
        }

    } else {
        for(int i = agg_start; i < agg_end;i++) {
            evaluator->add_inplace(aggregation_result[i - agg_start], final_result[i]);
        }
    }
    aggregation_count++;
    while(aggregation_count != NUM_WORKER_PER_GROUP) {
        pthread_cond_wait(&agg_done_cond, &aggregation_lock);
    }
    pthread_mutex_unlock(&aggregation_lock);
    for(int i = 0; i < num_agg_ct;i++) {
        evaluator->transform_from_ntt_inplace(aggregation_result[i]);
    }
    aggregation_end_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    SEND_TO_CLIENT()
    response_send_end_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    total_cpu_stop_time = clock();

    sleep(4);
    printReport();

    return 0;
}


void sendPartialResult(vector< vector<uint64_t> > result) {
    assert(result.size() == (agg_end - agg_start));
    //cout<<"received result from worker\n";
    partial_result_recv_timestamp.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count());
    pthread_mutex_lock(&aggregation_lock);
    if(aggregation_count == 0) {
        for(int i = 0; i < result.size();i++) {
            std::copy(result[i].begin(), result[i].end(), aggregation_result[i].data());
        }

    } else {
        for(int i = 0; i < result.size();i++) {
            std::copy(result[i].begin(), result[i].end(), temp_ct->data());
            evaluator->add_inplace(aggregation_result[i], *temp_ct);
        }
    }
    aggregation_count++;
    if(aggregation_count == NUM_WORKER_PER_GROUP) {
        pthread_cond_signal(&agg_done_cond);
    }
    pthread_mutex_unlock(&aggregation_lock);

}


void sendQuery( string _serialized_gal_key, string _serialized_query) {
    if(!warmup_received) {
        pthread_mutex_lock(&warmup_received_lock);
        serialized_query = _serialized_query;
        warmup_received = true;
        pthread_cond_signal(&warmup_received_cond);
        pthread_mutex_unlock(&warmup_received_lock);
        return;
    }
    //cout<<"received query"<<endl;
    query_rcv_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    serialized_gal_key = _serialized_gal_key;
    serialized_query = _serialized_query;
    pthread_mutex_lock(&request_received_lock);
    pthread_cond_broadcast(&request_received_cond);
    pthread_mutex_unlock(&request_received_lock);

    return;
}


void readWorkerIp() {
    worker_ip = new string[NUM_TOTAL_WORKER];

    fstream fs;
	fs.open("../../common/worker_ip.txt", ios::in);
	if (!fs) {
		cout << "Missing worker_ip.txt\n";
        exit(1);
	}

    for(int i = 0; i< NUM_TOTAL_WORKER;  i++) {
        fs>>worker_ip[i];
    }

    return;

}

void *populate_db(void *arg)
{
    int my_id = *((int*) arg);
    int elem_per_thread = floor((double)(DB_ELEMENTS) / NUM_THREAD);
    int start = my_id * elem_per_thread;
    int remaining = (DB_ELEMENTS) % NUM_THREAD ;
    if (remaining > my_id )
    {
        start += my_id ;
        elem_per_thread++;
    }
    else
    {
        start += remaining;
    }
    uint64_t aa =  1<< 20;
    aa = aa*aa;
    int end = start + elem_per_thread;
    vector<uint64_t> pod_matrix(N, 0ULL);
    Plaintext pt;
    for(int i = start; i < end;i++) {
        for(int j = 0; j < N;j++) {
            //pod_matrix[j] = rand()%100;
            pod_matrix[j] = aa + i*7 + j*5 + (i>>5) + (j & 15);
        }
        batch_encoder->encode(pod_matrix, pt);
        encoded_db[i] = pt;
        evaluator->transform_to_ntt_inplace(encoded_db[i], pid);
    }

    return NULL;
}

vector<vector<uint64_t>> get_encoded_block(uint64_t **matrix, uint rstart, uint cstart)
{ // Assume the submatrix is padded by caller if required
    vector<vector<uint64_t>> encoded_block(N);
    vector<uint64_t> pod_matrix1(N, 0Ull), pod_matrix2(N, 0ULL);

    for (int i = 0; i < N / 2; i++)
    {
        for (int j = 0; j < N / 2; j++)
        {
            pod_matrix1[j] = matrix[rstart + j][cstart + (i + j) % (N / 2)];
            pod_matrix1[j + N / 2] = matrix[rstart + N / 2 + j][cstart + N / 2 + (i + j) % (N / 2)];

            pod_matrix2[j] = matrix[rstart + j][cstart + N / 2 + (i + j) % (N / 2)];
            pod_matrix2[j + N / 2] = matrix[rstart + N / 2 + j][cstart + (i + j) % (N / 2)];
        }
        encoded_block[i] = pod_matrix1;
        encoded_block[i + N / 2] = pod_matrix2;
    }

    return encoded_block;
}


void *thread_multiply(void *arg)
{
    thread_argument *t_arg = (thread_argument *)arg;
    int my_id = t_arg->thread_id;
    //int iter = my_id & 1; // Odd for column rotated queries
    int rot_per_thread = floor((double)(worker_tot_rot) / NUM_THREAD);
    int start_rot = worker_rot_start +  my_id  * rot_per_thread;
    int remaining = (worker_tot_rot) % (NUM_THREAD );
    if (remaining > my_id )
    {
        start_rot += (my_id );
        rot_per_thread++;
    }
    else
    {
        start_rot += remaining;
    }
    int end_rot = start_rot + rot_per_thread;
    GaloisKeys gal_keys = *(t_arg->gal_keys);
    //chrono::high_resolution_clock::time_point time_start, time_end;
    //uint64_t add_time = 0, mult_time = 0, ntt_time = 0, row_rot_time = 0, col_rot_time = 0, inv_ntt_time = 0;
    Ciphertext temp_ct;
    //printf("id %d start %d end %d\n", my_id, start_rot, end_rot);
    stack<Ciphertext> st;

    Ciphertext q_ct = t_arg->query;

    for (int i = start_rot; i < end_rot; i++)
    {
        Ciphertext temp_q_ct = q_ct;;

        if(i != 0)
        {
            //time_start = chrono::high_resolution_clock::now();
            if (st.empty())
            {
                int k = 1;
                int j = i/2;

                // Setting k as the largest power of 2 that s smaller than i
                while(j) {
                    j >>= 1;
                    k <<= 1;
                }
                int rot_sum = k;
                while(rot_sum != i) {
                    evaluator->rotate_rows_inplace(temp_q_ct, k, gal_keys);

                    k = k >> 1;
                    if(!(i & k)) {
                        st.push(temp_q_ct);
                    }
                    while(k && !(i & k)) {
                        k = k >> 1;
                    }
                    rot_sum += k;
                }
                evaluator->rotate_rows_inplace(temp_q_ct, k, gal_keys);
            }
            else
            {
                int k = 1;
                while (!(i & k))
                {
                    k = k << 1;
                }
                Ciphertext cached_ct = st.top();

                evaluator->rotate_rows(cached_ct, k, gal_keys, temp_q_ct);

                if (i & (k << 1))
                {
                    st.pop();
                }
            }

            if (!(i % 2))
            {
                st.push(temp_q_ct);
            }
            //time_end = chrono::high_resolution_clock::now();

            //row_rot_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
        }

        //time_start = chrono::high_resolution_clock::now();
        evaluator->transform_to_ntt_inplace(temp_q_ct);
        //time_end = chrono::high_resolution_clock::now();
        //ntt_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();


        for (int res_idx = 0; res_idx < RESPONSE_CT; res_idx++)
        {
            if (i == start_rot)
            {
                //time_start = chrono::high_resolution_clock::now();
                evaluator->multiply_plain(temp_q_ct, encoded_db[res_idx * (N/SPLIT_FACTOR) + (i-start_rot)], t_arg->thread_result[res_idx]);
                //time_end = chrono::high_resolution_clock::now();
                //mult_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
            }
            else
            {
                //time_start = chrono::high_resolution_clock::now();
                evaluator->multiply_plain(temp_q_ct, encoded_db[res_idx * (N/SPLIT_FACTOR) + (i-start_rot)], temp_ct);
                //time_end = chrono::high_resolution_clock::now();
                //mult_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

                //time_start = chrono::high_resolution_clock::now();
                evaluator->add_inplace(t_arg->thread_result[res_idx], temp_ct);
                //time_end = chrono::high_resolution_clock::now();
                //add_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
            }
        }
    }
    pthread_barrier_wait(&mult_barrier);

    if(my_id >= RESPONSE_CT) {
        return NULL;
    }

    int resp_per_thread = floor((double)RESPONSE_CT / NUM_THREAD);
    int start_resp = my_id  * resp_per_thread;
    remaining = RESPONSE_CT % NUM_THREAD ;
    if (remaining > my_id )
    {
        start_resp += my_id;
        resp_per_thread++;
    }
    else
    {
        start_resp += remaining;
    }
    int end_resp = start_resp + resp_per_thread;

    Ciphertext sum;

    for(int i = start_resp; i < end_resp;i++) {
        sum = partial_results[0][i];
        for(int j = 1; j < NUM_THREAD;j++) {
            evaluator->add_inplace(sum, partial_results[j][i]);
        }
        final_result[i] = sum;
    }

    return NULL;
}

void sendClientWarmUp(rpc::client &response_client) {
    vector< vector < uint64_t> > dummy;
    vector<uint64_t> v;
    for(int i = 0; i < N; i++) {
        v.push_back(i);
    }
    dummy.push_back(v);
    dummy.push_back(v);
    response_client.call("sendResponse", -1, -1, dummy);
}

void printReport() {
    cout<<"query and key receive timestamp"<<endl;
    cout<<query_rcv_timestamp<<endl;
    cout<<"processing start and end timestamp"<<endl;
    cout<<processing_start_timestamp<<"\t"<<processing_end_timestamp<<endl;
    cout<<"computation time"<<endl;
    cout<<processing_end_timestamp - processing_start_timestamp<<endl;
    cout<<"partial result receive:"<<endl;
    for(int i = 0; i < partial_result_recv_timestamp.size();i++) {
        cout<<partial_result_recv_timestamp[i]<<"\t";
    }
    cout<<"\npartial result send:"<<endl;
    for(int i = 0; i < partial_result_send_timestamp.size();i++) {
        cout<<partial_result_send_timestamp[i]<<"\t";
    }
    cout<<"\naggregation end:"<<endl;
    cout<<aggregation_end_timestamp<<endl;
    cout<<"response sending finished"<<endl;
    cout<<response_send_end_timestamp<<endl;
    cout<<"time to send response"<<endl;
    cout<<(response_send_end_timestamp - aggregation_end_timestamp)<<endl;
    cout<<"Total CPU time "<<endl<<((float)total_cpu_stop_time-total_cpu_start_time)/CLOCKS_PER_SEC;

    return;
}