#include <netinet/in.h>
#include <sys/socket.h>
#include<unistd.h>
#include<strings.h>
#include<string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include<vector>
#include <pthread.h>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#include <memory>
#include <limits>
#include <sstream> 
#include<cmath>
#include<ctime>
#include<stack>

#include "rpc/client.h"
#include<rpc/server.h>

#include <iostream>
#include<fstream>
#include "seal/seal.h"

using namespace std;
using namespace seal;
using namespace chrono;

#define COEFF_MODULUS_SIZE {60, 60, 60}

#define N 8192
#define PLAIN_BIT 46


#define MASTER_PORT 4000
#define CLIENT_PORT 2000
#define MASTER_WORKER_PORT 3000
#define WORKER_WORKER_PORT 3500

