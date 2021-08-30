#include "cuckoo.hpp"
//size_t CuckooCode::hash_and_mod(size_t id, size_t nonce, u_int8_t data[], size_t modulus){
size_t CuckooCode::hash_and_mod(size_t id, size_t nonce, string data, size_t modulus){
   // size_t int_value;
    //string mytext(reinterpret_cast<char*>(data));
    string make = to_string(id) + to_string(nonce) + data;
    string output1 = sha256(make);
    string newOutput = output1.substr(48,16);
    //char* endptr = NULL;
    unsigned long value;
    std::istringstream iss(newOutput);
    iss >> std::hex >> value;
    size_t int_value = value%modulus;
   // long long int int_value = strtoll(output1.c_str(), &endptr, 10)%modulus;
    return int_value;
}
bool CuckooCode::insert(size_t attempt, unordered_map<size_t, vector<size_t>> &buckets, size_t key, unordered_map<size_t, size_t> &elements) { 
    if (attempt >= MAX_ATTEMSTS){
        return false;
    }
    //cout<<"in insert "<<key<<endl;
     // Case 1: check to see if any of the d buckets is empty. If so, insert there.
    for(auto hash_id: buckets[key])
    {
      //  cout<<" hash_id "<<hash_id<<" "<<buckets[key][0]<<endl;
        if(elements.count(hash_id)==0)
        {
            //cout<<"at position "<<hash_id<< " element is "<<key<<endl;
            elements[hash_id]=key;
            return true;
        }
    }
    // Case 2: all possible buckets are filled. Relocate an existing entry
    vector<size_t> possible_buckets = buckets[key];
    size_t index =  rand() % possible_buckets.size();
    size_t chosen_bucket = possible_buckets[index];
    size_t old_key = elements.at(chosen_bucket);
    elements[chosen_bucket] = key;

    return insert(attempt+1, buckets, old_key, elements);
}
unordered_map<size_t, vector<size_t>> CuckooCode::get_schedule(vector<size_t> keys){
    long total_buckets = ceil(this->k * this->r);
    //cout<<"total number of buckets "<<total_buckets<<endl;
    unordered_map<size_t, vector<size_t>> buckets;
    for(auto key:keys)
    {
        //cout<<"key "<<key<<endl;
        vector <size_t> bucket_choices;
        bucket_choices.reserve(this->d);
        // Map entry's key to d buckets (no repeats)
        string data =to_string(key);
        for(size_t i=0;i<this->d;i++)
        {
            size_t nonce =0;
            size_t bucket =hash_and_mod(i, nonce, data ,total_buckets); 
            while(std::count(bucket_choices.begin(), bucket_choices.end(), bucket)>0)
            {
                nonce= nonce+1;
                bucket =hash_and_mod(i, nonce, data ,total_buckets);
            }
            bucket_choices.push_back(bucket);
           // cout<<key<<" :bucket number "<<bucket<<" id: "<<i<<" nonce "<<nonce<<endl;
            

        }
        buckets.insert({key,bucket_choices});
     //   cout<<"In bucket "<<key<<" buckets "<<buckets[key][0]<<" "<<buckets[key][1]<<endl;
    }
    unordered_map<size_t, size_t> elements;
    for(auto key: keys)
    {
        size_t attempt = 0;
        unordered_map<size_t, vector<size_t>> schedule;
       //printf("%s\n",typeof(attempt));
        if(!insert(attempt,buckets,key,elements)){
            return schedule; //null can't possible to insert
        }
    }
    unordered_map<size_t, vector<size_t>> schedule;
    for (auto element: elements){ //elements will be empty or with element??
        vector<size_t> k;
        k.push_back(element.first);
       // schedule[elements.at(element.first)] = k;
        schedule.insert({elements.at(element.first),k});
    }
    return schedule;
}
//vector<compactPair> CuckooCode::encode(vector<KVpair> collection)
vector<vector<KVpair>> CuckooCode::encode(vector<KVpair> collection)
{
    long total_buckets = ceil(this->k * this->r);
    vector <vector<KVpair>> collections(total_buckets); 
    //vector<compactPair> totalList(total_buckets);
  //  compactPair cp;    
    for(auto entry: collection){
         vector <size_t> bucket_choices;
         bucket_choices.reserve(this->d);
        // Map entry's key to d buckets (no repeats)
        string data =to_string(entry.i);
        for(size_t i=0;i<this->d;i++)
        {
            size_t nonce =0;
            size_t bucket =hash_and_mod(i, nonce, data ,total_buckets); 
            while(std::count(bucket_choices.begin(), bucket_choices.end(), bucket)>0)
            {
                nonce= nonce+1;
                bucket =hash_and_mod(i, nonce, data ,total_buckets);
            }
            bucket_choices.push_back(bucket);
           // cout<<"b "<<bucket_choices[0]<<" "<<bucket<<endl;
        //    cout<<"-------------------"<<endl;
        //    cout<<" bucket "<<bucket<<" bucket size "<<totalList[bucket].bucketVal.size()<<endl;
             KVpair temp=entry;
        //     totalList[bucket].bucketVal.push_back(entry);
        //     totalList[bucket].bucketIndexKeyPair.insert({entry.i,totalList[bucket].bucketVal.size()-1});
        //    // totalList[bucket]=cp;
        //     cout<<" bucket "<<bucket<<" bucket size "<<totalList[bucket].bucketVal.size()<<endl;
        //     cout<<" key  "<<entry.i<<" in bucket index  "<<totalList[bucket].bucketIndexKeyPair[entry.i]<<endl;
            collections[bucket].push_back(temp);
          //  cout<<"-------------------"<<endl;
        }        
    }
    //return totalList;
    return collections;
}
KVpair CuckooCode::decode(KVpair sets[]){
    KVpair temp= sets[0];
    return temp;
}
