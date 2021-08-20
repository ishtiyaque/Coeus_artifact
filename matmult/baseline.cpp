#include<unistd.h>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream> 
#include<cmath>
#include<ctime>
#include<stack>

#include <iostream>
#include "seal/seal.h"

using namespace std;
using namespace seal;
using namespace chrono;

#define N 8192
#define PLAIN_BIT 46

int QUERY_CT = 0;
int RESPONSE_CT = 0;

#define DB_SIZE ((N * QUERY_CT * RESPONSE_CT))

uint32_t get_previous_power_of_two(uint32_t number);
uint32_t get_number_of_bits(uint64_t number);

int main(int argc, char **argv)
{
    int option;
    const char *optstring = "r:c:";
    while ((option = getopt(argc, argv, optstring)) != -1)
    {
        switch (option)
        {
        case 'r':
            RESPONSE_CT = stoi(optarg);
            break;
        case 'c':
            QUERY_CT = stoi(optarg);
            break;

        case '?':
            cout << "error optopt: " << optopt << endl;
            cout << "error opterr: " << opterr << endl;
            return 1;
        }
    }
    if (!RESPONSE_CT || !QUERY_CT)
    {
        printf("Incorrect parameter format!\n");
        exit(0);
    }
    chrono::high_resolution_clock::time_point time_start, time_end, total_start, total_end;
    std::ostringstream ss;
    srand(time(NULL));

    EncryptionParameters parms(scheme_type::BFV);
    parms.set_poly_modulus_degree(N);
    parms.set_coeff_modulus(CoeffModulus::Create(N, {60, 60, 60}));

    parms.set_plain_modulus(PlainModulus::Batching(N, PLAIN_BIT));
    auto context = SEALContext::Create(parms);
    uint64_t plain_modulus = parms.plain_modulus().value();
    auto qualifiers = context->first_context_data()->qualifiers();

    KeyGenerator keygen(context);

    auto pid = context->first_parms_id();

    PublicKey public_key = keygen.public_key();
    SecretKey secret_key = keygen.secret_key();

    Encryptor encryptor(context, secret_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context, secret_key);

    BatchEncoder batch_encoder(context);
    size_t slot_count = batch_encoder.slot_count();
    size_t row_size = slot_count / 2;

    vector<int> steps;
    steps.push_back(0);
    for (int i = 1; i < (N / 2); i *= 2)
    {
        steps.push_back(i);
    }

    GaloisKeys gal_keys = keygen.galois_keys_local(steps);

    uint64_t aa = 1 << 20;
    aa = aa * aa;
    vector<Plaintext> encoded_db(DB_SIZE);

    for (int i = 0; i < DB_SIZE; i++)
    {
        vector<uint64_t> temp;
        for (int j = 0; j < N; j++)
        {
            temp.push_back(aa + i * 7 + j * 5 + (i >> 5) + (j & 15));
        }
        batch_encoder.encode(temp, encoded_db[i]);
        evaluator.transform_to_ntt_inplace(encoded_db[i], pid);
    }

    Ciphertext query[QUERY_CT];
    Ciphertext response[RESPONSE_CT];

    vector<uint64_t> pod_matrix(N, 0ULL);
    pod_matrix[5] = pod_matrix[123] = 1;
    Plaintext query_pt;
    batch_encoder.encode(pod_matrix, query_pt);
    for (int i = 0; i < QUERY_CT; i++)
    {
        encryptor.encrypt_symmetric(query_pt, query[i]);
    }

    uint64_t add_time = 0, mult_time = 0, ntt_time = 0, row_rot_time = 0, col_rot_time = 0, inv_ntt_time = 0;
    Ciphertext temp;

    total_start = chrono::high_resolution_clock::now();

    for (int res_idx = 0; res_idx < RESPONSE_CT; res_idx++)
    {
        for (int query_idx = 0; query_idx < QUERY_CT; query_idx++)
        {
            Ciphertext query_ct = query[query_idx];
            Ciphertext local_result;

            for (int iter = 0; iter < 2; iter++)
            {
                int start_idx = (res_idx * QUERY_CT + query_idx) * N + iter * N / 2;

                temp = query_ct;

                time_start = chrono::high_resolution_clock::now();
                evaluator.transform_to_ntt_inplace(temp);
                time_end = chrono::high_resolution_clock::now();
                ntt_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();


                time_start = chrono::high_resolution_clock::now();
                evaluator.multiply_plain_inplace(temp, encoded_db[start_idx]);
                time_end = chrono::high_resolution_clock::now();
                mult_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();

                if(iter == 0) {
                    local_result = temp;
                }else {
                    time_start = chrono::high_resolution_clock::now();
                    evaluator.add_inplace(local_result, temp);
                    time_end = chrono::high_resolution_clock::now();
                    add_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();

                }
            

                for (int i = 1; i < N / 2; i++)
                {
                    time_start = chrono::high_resolution_clock::now();
                    {
                        int tot_rot = 0;
                        temp = query_ct;
                        while(tot_rot != i) {
                            int next_rot = get_previous_power_of_two(i-tot_rot);
                            //printf("next rot %d\n", next_rot);
                            evaluator.rotate_rows_inplace(temp, next_rot, gal_keys);
                            tot_rot += next_rot;
                        }
                    }
                    time_end = chrono::high_resolution_clock::now();
                    row_rot_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();

                    time_start = chrono::high_resolution_clock::now();
                    evaluator.transform_to_ntt_inplace(temp);
                    time_end = chrono::high_resolution_clock::now();
                    ntt_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();


                    time_start = chrono::high_resolution_clock::now();
                    evaluator.multiply_plain_inplace(temp, encoded_db[start_idx + i]);
                    time_end = chrono::high_resolution_clock::now();
                    mult_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();


                    time_start = chrono::high_resolution_clock::now();
                    evaluator.add_inplace(local_result, temp);
                    time_end = chrono::high_resolution_clock::now();
                    add_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();
                }

                if(iter == 0) {
                    time_start = chrono::high_resolution_clock::now();
                    evaluator.rotate_columns_inplace(query_ct, gal_keys);
                    time_end = chrono::high_resolution_clock::now();
                    col_rot_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();
                }
            }

            if(query_idx == 0) {
                response[res_idx] = local_result;
            }else {
                time_start = chrono::high_resolution_clock::now();
                evaluator.add_inplace(response[res_idx],local_result);
                time_end = chrono::high_resolution_clock::now();
                add_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();

            }
        }
    }

    for(int i = 0; i < RESPONSE_CT;i++) {
        time_start = chrono::high_resolution_clock::now();
        evaluator.transform_from_ntt_inplace(response[i]);
        time_end = chrono::high_resolution_clock::now();
        inv_ntt_time += (chrono::duration_cast<chrono::microseconds>(time_end - time_start)).count();
    }


    total_end = chrono::high_resolution_clock::now();

    cout << "Add time: " <<endl<< add_time << endl;
    cout << "Mult time: " <<endl<< mult_time << endl;
    cout << "Row rotation time: " <<endl<< row_rot_time << endl;
    cout << "Col rotation time: " <<endl<< col_rot_time << endl;
    cout << "NTT time: " <<endl<< ntt_time << endl;
    cout << "Inv NTT time: " <<endl<< inv_ntt_time << endl;

    cout << "Total time: " <<endl<< (chrono::duration_cast<chrono::microseconds>(total_end - total_start)).count() << endl;


    return 0;
}



uint32_t get_previous_power_of_two(uint32_t number)
{
    if (!(number & (number - 1)))
    {
        return number;
    }

    uint32_t number_of_bits = get_number_of_bits(number);
    return (1 << (number_of_bits-1));
}


uint32_t get_number_of_bits(uint64_t number)
{
    uint32_t count = 0;
    while (number)
    {
        count++;
        number /= 2;
    }
    return count;
}