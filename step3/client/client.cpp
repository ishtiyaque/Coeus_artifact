#include<globals.h>

using namespace std::chrono;
using namespace std;
using namespace seal;

int NUM_TOTAL_WORKER = 0;

string CLIENT_IP = "";
string MASTER_IP = "";

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

PirReply *all_replies;

void sendResponse(int id,string response);
void printReport();


int main(int argc, char *argv[]) {

    uint64_t number_of_items = 0;
    //uint64_t ITEM_SIZE = 1024; // in bytes
    //uint32_t N = 2048;

    // Recommended values: (LOGT, d) = (12, 2) or (8, 1). 
    //uint32_t LOGT = 12; 
    //uint32_t d = 2;

    int option;
    const char *optstring = "n:w:p:c:";
    while ((option = getopt(argc, argv, optstring)) != -1) {
	switch (option) {

        case 'n':
            number_of_items = stoi(optarg);
            break;
        case 'w':
            NUM_TOTAL_WORKER = stoi(optarg);
            break;
        case 'p':
            MASTER_IP = string(optarg);
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
    if (MASTER_IP.size() < 7){cout << "Missing -p\n"; return 0;}
    if (!NUM_TOTAL_WORKER){cout << "Missing -w\n"; return 0;}
    if(CLIENT_IP.size() < 7) {cout<<"Missing -c\n";return 0;}


    rpc::server response_server(string(CLIENT_IP), CLIENT_PORT);
    response_server.bind("sendResponse", sendResponse);
    response_server.async_run(1);

    rpc::client query_client(MASTER_IP, MASTER_PORT);

    chrono::high_resolution_clock::time_point time_start, time_end, total_start, total_end;

    EncryptionParameters params(scheme_type::BFV);
    PirParams pir_params;

    // Generates all parameters
    //cout << "Main: Generating all parameters" << endl;
    gen_params(number_of_items, ITEM_SIZE, N, LOGT, DIM, params, pir_params);

    //cout << "Main: Initializing the database (this may take some time) ..." << endl;

 
    // Initialize PIR client....
    PIRClient client(params, pir_params);
    GaloisKeys galois_keys = client.generate_galois_keys();
    string serialized_gal_key = serialize_galoiskeys(galois_keys);

    all_replies = new PirReply[NUM_TOTAL_WORKER];

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

    query_client.call("sendQuery", serialized_gal_key, serialized_gal_key);


    pthread_mutex_lock(&warmup_lock);
    while (warmup_count < NUM_TOTAL_WORKER)
    {
        pthread_cond_wait(&warmup_cond, &warmup_lock);
    }
    pthread_mutex_unlock(&warmup_lock);
    
    total_cpu_start_time = clock();

    time_start = chrono::high_resolution_clock::now();

    uint64_t ele_index = rand() % number_of_items; // element in DB at random position
    uint64_t index = client.get_fv_index(ele_index, ITEM_SIZE);   // index of FV plaintext
    uint64_t offset = client.get_fv_offset(ele_index, ITEM_SIZE); // offset in FV plaintext
    PirQuery query = client.generate_query(index);
    std::string serialized_query = serialize_query(query);
  
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
    latency = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

    time_start = chrono::high_resolution_clock::now();
    for(int i = 0; i < NUM_TOTAL_WORKER;i++) {
        Plaintext result = client.decode_reply(all_replies[i]);
    }
    time_end = chrono::high_resolution_clock::now();
    decode_time= chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
 
    total_cpu_stop_time = clock();

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

    all_replies[id] = reply;
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
    cout<<"decode time "<<endl<<decode_time<<endl;
    return;
}