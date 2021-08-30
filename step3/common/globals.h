#include "pir.hpp"
#include "pir_client.hpp"
#include "pir_server.hpp"
#include <seal/seal.h>
#include <chrono>
#include <memory>
#include <random>
#include <cstdint>
#include <cstddef>

#include <netinet/in.h>
#include <sys/socket.h>
#include<unistd.h>
#include<strings.h>
#include<string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include<vector>
#include <pthread.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <memory>
#include <limits>
#include <sstream> 
#include<cmath>
#include<ctime>
#include<stack>
#include<fstream>

#include "rpc/client.h"
#include<rpc/server.h>

using namespace std::chrono;
using namespace std;
using namespace seal;
    
    
#define ITEM_SIZE (15*256)   // in bytes
#define N  2048

// Recommended values: (logt, d) = (12, 2) or (8, 1). 
#define LOGT  15 
#define DIM 2

#define MASTER_PORT 4000
#define CLIENT_PORT 2000
#define WORKER_PORT 3000

#define CIPHER_SIZE 32841
