#include<globals.h>

pthread_mutex_t request_received_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t warmup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t warmup_cond = PTHREAD_COND_INITIALIZER;



bool warmup_received = false;

void sendQuery( string _serialized_gal_key, vector<string> _serialized_query);
void *sendToWorker(void *arg);
void prepareSendThread();
void printReport();

string serialized_gal_key;
vector<string> serialized_query;
string MASTER_IP;
int NUM_TOTAL_WORKER = 0;
int k = 0;

uint64_t query_rcv_timestamp;
uint64_t *query_send_start_timestamp;
uint64_t *query_send_end_timestamp;

clock_t total_cpu_start_time, total_cpu_stop_time;


pthread_t *send_threads;
int *thread_id;
string *worker_ip;
int main(int argc, char **argv) {

    int option;
    const char *optstring = "p:k:";
    while ((option = getopt(argc, argv, optstring)) != -1) {
	switch (option) {

       case 'p':
            MASTER_IP = string(optarg);
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
    if(MASTER_IP.size() < 7) {cout<<"Missing -p\n";return 0;}
    if(!k) {cout<<"Missing -k\n";return 0;}

    NUM_TOTAL_WORKER = (int)ceil(k * FACTOR);

    query_send_start_timestamp = new uint64_t[NUM_TOTAL_WORKER];
    query_send_end_timestamp = new uint64_t[NUM_TOTAL_WORKER];


    rpc::server query_server(string(MASTER_IP), MASTER_PORT);
    query_server.bind("sendQuery", sendQuery);
    query_server.async_run(1);
    
    pthread_mutex_lock(&warmup_lock);
    pthread_cond_wait(&warmup_cond, &warmup_lock);
    pthread_mutex_unlock(&warmup_lock);
   
    total_cpu_start_time = clock();

    prepareSendThread();
    sleep(3);
    total_cpu_stop_time = clock();

    printReport();
    return 0;
}


void sendQuery( string _serialized_gal_key, vector<string> _serialized_query) {

    if(!warmup_received) {
        pthread_mutex_lock(&warmup_lock);
        warmup_received = true;
        serialized_gal_key = _serialized_gal_key;
        serialized_query = _serialized_query;
        pthread_cond_signal(&warmup_cond);
        pthread_mutex_unlock(&warmup_lock);
        return;
    }
    query_rcv_timestamp = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    serialized_gal_key = _serialized_gal_key;
    serialized_query = _serialized_query;
    //cout<<"actual query size "<<serialized_query.size()<<endl;
    pthread_mutex_lock(&request_received_lock);
    pthread_cond_broadcast(&request_received_cond);
    pthread_mutex_unlock(&request_received_lock);
    return;
}

void prepareSendThread() {
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


    thread_id = new int[NUM_TOTAL_WORKER];
    send_threads = new pthread_t[NUM_TOTAL_WORKER];

    for (int i = 0; i < NUM_TOTAL_WORKER; i++)
    {
        thread_id[i] = i;
        if (pthread_create(&send_threads[i], NULL, sendToWorker, (void *)(&thread_id[i])))
        {            
            printf("Error creating thread\n");
            exit(1);
        }
    }
    for (int i = 0; i < NUM_TOTAL_WORKER; i++)
    {
        pthread_join(send_threads[i], NULL);
    }
    return;

}

void *sendToWorker(void *arg) {
    int my_id = * ((int *)arg);
    rpc::client worker_client(worker_ip[my_id], WORKER_PORT + my_id);
    //cout<<"connected to worker at "<<worker_ip[my_id]<<":"<<MASTER_WORKER_PORT + my_id<<endl;
    worker_client.call("sendQuery", serialized_gal_key, serialized_query[my_id]);

    pthread_mutex_lock(&request_received_lock);
    pthread_cond_wait(&request_received_cond, &request_received_lock);
    pthread_mutex_unlock(&request_received_lock);
    query_send_start_timestamp[my_id] = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    worker_client.call("sendQuery", serialized_gal_key, serialized_query[my_id]);
    query_send_end_timestamp[my_id] = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    return NULL;
}

void printReport() {
    cout<<"query receive timestamp:"<<endl;
    cout<<query_rcv_timestamp<<endl;
    cout<<"send to worker start timestamp:"<<endl;
    for(int i = 0; i < NUM_TOTAL_WORKER;i++) {
         cout<<query_send_start_timestamp[i]<<"\t";
    }
    cout<<"\nsend to worker end timestamp:"<<endl;
    for(int i = 0; i < NUM_TOTAL_WORKER;i++) {
        cout<<query_send_end_timestamp[i]<<"\t";
    }
    cout<<"\nTotal CPU time including overheads:(sec)\n"<<((float)total_cpu_stop_time-total_cpu_start_time)/CLOCKS_PER_SEC;


    return;
}
