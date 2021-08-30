#include<globals.h>
#include<sha256.h>
#include<cuckoo.hpp>

using namespace std::chrono;
using namespace std;
using namespace seal;

int NUM_TOTAL_WORKER = 0;

string CLIENT_IP = "";
string MASTER_IP = "";

uint64_t number_of_items = 0;
int ITEM_SIZE = 0;
int k = 0; // k in top k

int warmup_count = 0;
int response_received = 0;

uint64_t query_gen_time, latency, decode_time;

uint64_t query_send_timestamp;
vector<uint64_t> response_rcv_timestamp;

clock_t total_cpu_start_time, total_cpu_stop_time;


pthread_mutex_t warmup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t warmup_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t response_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t response_cond = PTHREAD_COND_INITIALIZER;

int EXPANSION_FACTOR = 0;

//PirReply *all_replies;
PIRClient *client;

void sendResponse(int id,string response);
void printReport();
oraclePair get_oracle(CuckooCode *code, uint64_t number_of_items, uint64_t size_per_item, uint32_t d);
vector<u_int32_t> getIndices(oraclePair &op, unordered_map<size_t, vector<size_t>> &schedules);
vector<size_t> setRandomKeys();



int main(int argc, char *argv[]) {

    //uint64_t ITEM_SIZE = 1024; // in bytes
    //uint32_t N = 2048;

    // Recommended values: (LOGT, d) = (12, 2) or (8, 1). 
    //uint32_t LOGT = 12; 
    //uint32_t d = 2;

    int option;
    const char *optstring = "n:p:s:c:k:";
    while ((option = getopt(argc, argv, optstring)) != -1) {
	switch (option) {

        case 'n':
            number_of_items = stoi(optarg);
            break;
       case 'p':
            MASTER_IP = string(optarg);
            break;
	    case 's':
		    ITEM_SIZE = stoi(optarg);
	    	break;
        case 'c':
            CLIENT_IP = string(optarg);
            break;
        case 'k':
		    k = stoi(optarg);
		    break;
  
        case '?':
            cout<<"error optopt: "<<optopt<<endl;
            cout<<"error opterr: "<<opterr<<endl;
            return 1;
	    }
    }
    if(!number_of_items) {cout<<"Missing -n\n";return 0;}
    if (MASTER_IP.size() < 7){cout << "Missing -p\n"; return 0;}
    if (!ITEM_SIZE){cout << "Missing -w\n"; return 0;}
    if (!k){cout << "Missing -k\n"; return 0;}
    if(CLIENT_IP.size() < 7) {cout<<"Missing -c\n";return 0;}


    rpc::server response_server(string(CLIENT_IP), CLIENT_PORT);
    response_server.bind("sendResponse", sendResponse);
    response_server.async_run(1);

    rpc::client query_client(MASTER_IP, MASTER_PORT);

    CuckooCode *code = new CuckooCode(k,REPLICA,FACTOR);
    oraclePair op = get_oracle(code, number_of_items, ITEM_SIZE, DIM);

    NUM_TOTAL_WORKER = op.sizes.size();

    int max_elems_per_bucket = op.sizes[0].s1;
    for(int  i = 1; i < NUM_TOTAL_WORKER;i++) {
        if(op.sizes[i].s1 > max_elems_per_bucket) {
            max_elems_per_bucket = op.sizes[i].s1;
        }
    }
    chrono::high_resolution_clock::time_point time_start, time_end, total_start, total_end;


    EncryptionParameters params(scheme_type::BFV);
    PirParams pir_params;

    // Generates all parameters
    //cout << "Main: Generating all parameters" << endl;
    gen_params(max_elems_per_bucket, ITEM_SIZE, N, LOGT, DIM, params, pir_params);

    //cout << "Main: Initializing the database (this may take some time) ..." << endl;

 
    // Initialize PIR client....
    client = new PIRClient(params, pir_params);
    GaloisKeys galois_keys = client->generate_galois_keys();
    string serialized_gal_key = serialize_galoiskeys(galois_keys);

    //all_replies = new PirReply[NUM_TOTAL_WORKER];

    EXPANSION_FACTOR = pir_params.expansion_ratio;
    //cout<<"Gal key size: "<<galois_keys.size()<<endl;

    // Set galois key for client with id 0
    //cout << "Main: Setting Galois keys..."<<endl;

    pthread_mutex_lock(&warmup_lock);
    while (warmup_count < NUM_TOTAL_WORKER)
    {
        pthread_cond_wait(&warmup_cond, &warmup_lock);
    }
    pthread_mutex_unlock(&warmup_lock);

    warmup_count = 0;

    {
        vector<string> dummy;
        for(int i = 0;i<NUM_TOTAL_WORKER;i++) {
            string str = "1234567890";
            for(int j = 0;j<12;j++) {
                str = str+str;
            }
            dummy.push_back(str);
        }

        query_client.call("sendQuery", serialized_gal_key, dummy);
    }
 
    vector<string> serialized_query;
    vector<uint64_t> keys;


    pthread_mutex_lock(&warmup_lock);
    while (warmup_count < NUM_TOTAL_WORKER)
    {
        pthread_cond_wait(&warmup_cond, &warmup_lock);
    }
    pthread_mutex_unlock(&warmup_lock);
    
    total_cpu_start_time = clock();

    time_start = chrono::high_resolution_clock::now();

    for(int i = 0; i < k;i++) {
        keys.push_back(rand()%number_of_items);
    }
    unordered_map<size_t, vector<size_t>> schedules = code->get_schedule(keys);
    if(schedules.size()==0)
    {
        cout<<"null ; can't be scheduled...."<<endl;
        return -1;
    }
   // for getting the index in each bucket
    vector<u_int32_t> ind_vec= getIndices(op,schedules);


    for(int i = 0;i<ind_vec.size();i++) {
        uint64_t ele_index = ind_vec[i];
        uint64_t index = client->get_fv_index(ele_index, ITEM_SIZE);   
        uint64_t offset = client->get_fv_offset(ele_index, ITEM_SIZE); 
        PirQuery query = client->generate_query(index);
        serialized_query.push_back(serialize_query(query));    
    }

 
    time_end = chrono::high_resolution_clock::now();
    query_gen_time = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
    
    query_send_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    time_start = chrono::high_resolution_clock::now();
    query_client.async_call("sendQuery", serialized_gal_key, serialized_query);

    // Measure query processing (including expansion)
    pthread_mutex_lock(&response_lock);
    pthread_cond_wait(&response_cond, &response_lock);
    pthread_mutex_unlock(&response_lock);
    time_end = chrono::high_resolution_clock::now();
    total_cpu_stop_time = clock();

    latency = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

    // time_start = chrono::high_resolution_clock::now();
    // for(int i = 0; i < NUM_TOTAL_WORKER;i++) {
    //     Plaintext result = client->decode_reply(all_replies[i]);
    // }
    // time_end = chrono::high_resolution_clock::now();
    // decode_time= chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
 

    printReport();
    return 0;
}


void sendResponse(int id,string response)
{
    if (id == -1)
    {
        pthread_mutex_lock(&warmup_lock);
        warmup_count++;
        if (warmup_count == NUM_TOTAL_WORKER)
        {
            pthread_cond_signal(&warmup_cond);
        }
        pthread_mutex_unlock(&warmup_lock);
        return;
    }

    response_rcv_timestamp.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count());
    //cout << "received response size " << response.size() << endl;

    PirReply reply = deserialize_ciphertexts(EXPANSION_FACTOR,response,CIPHER_SIZE);
    //cout << "deserialized reply " << reply.size() << endl;
    Plaintext result = client->decode_reply(reply);

    //all_replies[id] = reply;
    //all_replies.push_back(reply);

    pthread_mutex_lock(&response_lock);
    response_received ++;
    if (response_received == NUM_TOTAL_WORKER)
    {
        pthread_cond_broadcast(&response_cond);
    }
    pthread_mutex_unlock(&response_lock);

    return;
}

void printReport()
{
    cout<<"latency "<<endl<<latency<<endl;
    cout << "query send:" << endl;
    cout << query_send_timestamp << endl;
    cout << "response_received" << endl;
    for (int i = 0; i < response_rcv_timestamp.size(); i++)
    {
        cout << response_rcv_timestamp[i] <<"\t";
    }
    cout<<"\nTotal CPU time (sec): "<<endl<<(((float)total_cpu_stop_time-total_cpu_start_time)/CLOCKS_PER_SEC)<<endl;
    cout<<"query gen time "<<endl<<query_gen_time<<endl;
    //cout<<"decode time "<<endl<<decode_time<<endl;
    return;
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

vector<size_t> setRandomKeys(){
    //random key key set are made
    unordered_set<size_t> key_set;
    size_t val=number_of_items;
    while(key_set.size()<k){
        size_t value = rand()%val;
        key_set.insert(value);
    } 
    vector<size_t> keys;
    keys.insert(keys.end(), key_set.begin(), key_set.end());
    return keys;
}