#include <cstddef>
#include <math.h> 
#include <vector>
#include "sha256.h"
#include <algorithm>
#include <unordered_map>
#include <time.h>
#include "globals.h"
#include <iostream>
#include<sstream>
#define MAX_ATTEMSTS 1000
using namespace std;


class CuckooCode {
    public:
    size_t k;
    size_t d;
    double r;
    CuckooCode(size_t k, size_t d, double r){
        this->k = k; //batch size
        this->d = d; //d choices
        this->r = r;  //total buckets = ceil(k*r)
    }
    bool insert(size_t, unordered_map<size_t, vector<size_t>>&, size_t, unordered_map<size_t, size_t>&);
   // vector<compactPair> encode(vector<KVpair>);
   vector<vector<KVpair>> encode(vector<KVpair>);
    unordered_map<size_t, vector<size_t>> get_schedule(vector<size_t>);
    KVpair decode(KVpair[]);
    size_t hash_and_mod(size_t, size_t, string, size_t);
};