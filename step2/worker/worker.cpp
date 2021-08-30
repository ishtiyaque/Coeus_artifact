#include<globals.h>
#include<sha256.h>
#include<cuckoo.hpp>

using namespace std::chrono;
using namespace std;
using namespace seal;

int NUM_THREAD = 0;
int WORKER_ID = -1;
int NUM_TOTAL_WORKER = 0;
int ITEM_SIZE = 0;
int k = 0;

string *worker_ip;
string serialized_gal_key;
string serialized_query;

clock_t total_cpu_start_time, total_cpu_stop_time;

pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t request_received_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t warmup_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t warmup_received_lock = PTHREAD_MUTEX_INITIALIZER;

int warmup_count = 0;
bool warmup_received = false;

uint64_t query_rcv_timestamp, response_send_timestamp;
uint64_t processing_start_timestamp, processing_end_timestamp;
uint64_t response_send_start_timestamp,response_send_end_timestamp;



string CLIENT_IP = "";

void readWorkerIp();
void sendClientWarmUp(rpc::client &response_client);
void sendQuery( string _serialized_gal_key, string _serialized_query);
oraclePair get_oracle(CuckooCode *code, uint64_t number_of_items, uint64_t size_per_item, uint32_t d);
vector<u_int32_t> getIndices(oraclePair &op, unordered_map<size_t, vector<size_t>> &schedules);
void printReport();

int main(int argc, char *argv[]) {

    uint64_t number_of_items = 0;

    int option;
    const char *optstring = "n:t:s:i:k:c:";
    while ((option = getopt(argc, argv, optstring)) != -1) {
	switch (option) {

        case 'n':
            number_of_items = stoi(optarg);
            break;
        case 't':
            NUM_THREAD = stoi(optarg);
            break;
        case 'i':
            WORKER_ID = stoi(optarg);
            break;
        case 'k':
            k = stoi(optarg);
            break;
 	    case 's':
		    ITEM_SIZE = stoi(optarg);
	    	break;
        case 'c':
            CLIENT_IP = string(optarg);
            break;  
        case '?':
            cout<<"error optopt: "<<optopt<<endl;
            cout<<"error opterr: "<<opterr<<endl;
            return 1;
	    }
    }
    if(!number_of_items) {cout<<"Missing -n\n";return 0;}
    if(!NUM_THREAD) {cout<<"Missing -t\n";return 0;}
    if(!ITEM_SIZE) {cout<<"Missing -s\n";return 0;}
    if(!k) {cout<<"Missing -k\n";return 0;}
    if(WORKER_ID < 0) {cout<<"Missing -i\n";return 0;}
    if(CLIENT_IP.size() < 7) {cout<<"Missing -c\n";return 0;}

    NUM_TOTAL_WORKER = (int)ceil(k * FACTOR);

    readWorkerIp();

    rpc::server query_server(string(worker_ip[WORKER_ID]), WORKER_PORT + WORKER_ID);
    query_server.bind("sendQuery", sendQuery);
    query_server.async_run(1);
    rpc::client response_client(CLIENT_IP, CLIENT_PORT);

    CuckooCode *code = new CuckooCode(k,REPLICA,FACTOR);
    oraclePair op = get_oracle(code, number_of_items, ITEM_SIZE, DIM);

    int max_elems_per_bucket = op.sizes[0].s1;
    for(int  i = 1; i < NUM_TOTAL_WORKER;i++) {
        if(op.sizes[i].s1 > max_elems_per_bucket) {
            max_elems_per_bucket = op.sizes[i].s1;
        }
    }

    EncryptionParameters params(scheme_type::BFV);
    PirParams pir_params;

    // Generates all parameters
    //cout << "Main: Generating all parameters" << endl;
    gen_params(max_elems_per_bucket, ITEM_SIZE, N, LOGT, DIM, params, pir_params);

    //cout << "Main: Initializing the database (this may take some time) ..." << endl;

    // Create test database
    auto db(make_unique<uint8_t[]>(max_elems_per_bucket * ITEM_SIZE));

    // Copy of the database. We use this at the end to make sure we retrieved
    // the correct element.

    for (uint64_t i = 0; i < max_elems_per_bucket; i++) {
        for (uint64_t j = 0; j < ITEM_SIZE; j++) {
            auto val = rand() % 256;
            db.get()[(i * ITEM_SIZE) + j] = val;
        }
    }


    // Initialize PIR Server
    //cout << "Main: Initializing server" << endl;
    PIRServer server(params, pir_params,NUM_THREAD);
    server.set_database(move(db), max_elems_per_bucket, ITEM_SIZE);
    server.preprocess_database();
    sendClientWarmUp(response_client);

    pthread_mutex_lock(&warmup_received_lock);
    while(!warmup_received){
        pthread_cond_wait(&warmup_received_cond, &warmup_received_lock);
    }
    pthread_mutex_unlock(&warmup_received_lock);


    sendClientWarmUp(response_client);
    total_cpu_start_time = clock();

    pthread_mutex_lock(&request_received_lock);
    pthread_cond_wait(&request_received_cond, &request_received_lock);
    pthread_mutex_unlock(&request_received_lock);

    GaloisKeys galois_keys = *deserialize_galoiskeys(serialized_gal_key);
    server.set_galois_key(0, galois_keys);

    PirQuery query = deserialize_query(DIM, 1, serialized_query, CIPHER_SIZE); 
 
    processing_start_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    PirReply reply = server.generate_reply(query, 0);
    processing_end_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    string serialized_response =  serialize_ciphertexts(reply);

    response_send_start_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    response_client.call("sendResponse",WORKER_ID, serialized_response);
    response_send_end_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

    total_cpu_stop_time = clock();

   printReport();

    return 0;
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
	fs.open("../common/worker_ip.txt", ios::in);
	if (!fs) {
		cout << "Missing worker_ip.txt\n";
        exit(1);
	}

    for(int i = 0; i< NUM_TOTAL_WORKER;  i++) {
        fs>>worker_ip[i];
    }

    return;

}

void sendClientWarmUp(rpc::client &response_client) {
    string str = "123456789";
    for(int i = 0; i < 12;i++) {
        str = str + str;
    }
    // vector<uint64_t> v;
    // for(int i = 0; i < N; i++) {
    //     v.push_back(i);
    // }
    // dummy.push_back(v);
    // dummy.push_back(v);

    response_client.call("sendResponse", -1, str);
}

oraclePair get_oracle(CuckooCode *code, uint64_t number_of_items, uint64_t size_per_item, uint32_t d){
    //cout<<"in get oracle "<<endl;
    oraclePair op;
    vector<KVpair> collection;
    //cout<<"number of elements "<<number_of_items<<endl;
    size_t number_of_ele=number_of_items;
    size_t size_per_elem = size_per_item;
    for(size_t i=0;i<number_of_ele;i++){
        KVpair kv;
        kv.i=i; 
        size_t randVal = rand();
        char x[size_per_elem];
        for(size_t random=0;random<size_per_elem;random++){
            x[random] = 'a';
        }
        kv.value = x;
        collection.push_back(kv);
    }
        vector<vector<KVpair>> oracle = code->encode(collection);
        int bucket =0;
        for(auto o: oracle)
        {
            sizePair sp;
            sp.s1=o.size();
            sp.s2 = size_per_elem;
            op.sizes.push_back(sp);
            bucket++;
        }
        op.oracle = oracle;
    return op;
}

vector<u_int32_t> getIndices(oraclePair &op, unordered_map<size_t, vector<size_t>> &schedules){
    // for getting the index in each bucket
    unordered_map<size_t, size_t> indexes;
    indexes.reserve(op.oracle.size());
    for(auto schedule: schedules){
       indexes[schedule.first] = schedule.second[0];
        op.oracle[schedule.second[0]];
        size_t index_in_bucket = 0;
        for(auto el_in_bucket: op.oracle[schedule.second[0]]){
                if(el_in_bucket.i==schedule.first)
                {
                    break;
                }
                index_in_bucket++;
        }
        indexes.insert({schedule.second[0],index_in_bucket});
    }
    vector<u_int32_t> ind_vec;
    ind_vec.reserve(op.oracle.size());
    for(size_t i=0;i<op.oracle.size();i++){
        if(indexes.find(i) == indexes.end()){
            ind_vec.push_back((rand()) % (op.sizes[i].s1));
        }
        else{
            ind_vec.push_back(indexes[i]);
        }
    }
    return ind_vec;
}


void printReport() {
    cout<<"query and key receive timestamp"<<endl;
    cout<<query_rcv_timestamp<<endl;
    cout<<"processing start and end timestamp"<<endl;
    cout<<processing_start_timestamp<<"\t"<<processing_end_timestamp<<endl;
    cout<<"computation time"<<endl;
    cout<<processing_end_timestamp - processing_start_timestamp<<endl;
    cout<<"response sending finished"<<endl;
    cout<<response_send_end_timestamp<<endl;
    cout<<"time to send response"<<endl;
    cout<<(response_send_end_timestamp - processing_end_timestamp)<<endl;
    cout<<"Total CPU time "<<endl<<((float)total_cpu_stop_time-total_cpu_start_time)/CLOCKS_PER_SEC;

    return;
}