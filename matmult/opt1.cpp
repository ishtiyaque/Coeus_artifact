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
int QUERY_CT;
int RESPONSE_CT;

#define DB_SIZE (N * QUERY_CT * RESPONSE_CT)

vector<vector<uint64_t>> get_encoded_block(uint64_t **matrix, uint rstart, uint cstart);

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

    Encryptor encryptor(context, public_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context, secret_key);

    BatchEncoder batch_encoder(context);
    size_t slot_count = batch_encoder.slot_count();
    size_t row_size = slot_count / 2;

    vector<int> steps;
    steps.push_back(0);
    for (int i = 1; i < N / 2; i *= 2)
    {
        steps.push_back(i);
    }

    GaloisKeys gal_keys = keygen.galois_keys_local(steps);

    vector<Plaintext> encoded_db;

    Plaintext pt;

    int aa = 1 << 20;
    aa = aa * aa;
    for (int i = 0; i < DB_SIZE; i++)
    {
        vector<uint64_t> pod_matrix;
        for (int j = 0; j < N; j++)
        {
            pod_matrix.push_back(aa + i * 7 + j * 5 + (i >> 5) + (j & 15));
        }
        batch_encoder.encode(pod_matrix, pt);
        encoded_db.push_back(pt);
        evaluator.transform_to_ntt_inplace(encoded_db[i], pid);
    }
    vector<uint64_t> pod_matrix(N, 0ULL);
    pod_matrix[5] = 1;
    pod_matrix[123] = 1;
    pod_matrix[6543] = 1;
    Plaintext query_pt;
    batch_encoder.encode(pod_matrix, query_pt);
    Ciphertext query_ct[QUERY_CT];
    for (int i = 0; i < QUERY_CT; i++)
    {
        encryptor.encrypt(query_pt, query_ct[i]);
    }
    Ciphertext final_result[RESPONSE_CT];

    uint64_t add_time = 0, mult_time = 0, ntt_time = 0, row_rot_time = 0, col_rot_time = 0, inv_ntt_time = 0;
    Ciphertext temp_ct;
    Ciphertext column_sum;

    stack<Ciphertext> st;

    total_start = chrono::high_resolution_clock::now();

    for (int res_idx = 0; res_idx < RESPONSE_CT; res_idx++)
    {

        for (int q_idx = 0; q_idx < QUERY_CT; q_idx++)
        {
            Ciphertext q_ct = query_ct[q_idx];

            for (int iter = 0; iter < 2; iter++)
            {
                for (int i = 0; i < N / 2; i++)
                {
                    Ciphertext temp_q_ct;

                    time_start = chrono::high_resolution_clock::now();
                    if (st.empty())
                    {
                        evaluator.rotate_rows(q_ct, i, gal_keys, temp_q_ct);
                    }
                    else
                    {
                        int k = 1;
                        while (!(i & k))
                        {
                            k = k << 1;
                        }
                        Ciphertext cached_ct = st.top();

                        evaluator.rotate_rows(cached_ct, k, gal_keys, temp_q_ct);
                        if (i & (k << 1))
                        {
                            st.pop();
                        }
                    }

                    if (i && !(i % 2))
                    {
                        st.push(temp_q_ct);
                    }
                    time_end = chrono::high_resolution_clock::now();

                    row_rot_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

                    time_start = chrono::high_resolution_clock::now();
                    evaluator.transform_to_ntt_inplace(temp_q_ct);
                    time_end = chrono::high_resolution_clock::now();
                    ntt_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

                    if (q_idx == 0 && iter == 0 && i == 0)
                    {
                        time_start = chrono::high_resolution_clock::now();
                        evaluator.multiply_plain(temp_q_ct, encoded_db[(q_idx * RESPONSE_CT * N) + res_idx * N + (iter * N / 2) + i], final_result[res_idx]); // Need to update db index value
                        time_end = chrono::high_resolution_clock::now();
                        mult_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
                    }
                    else
                    {
                        time_start = chrono::high_resolution_clock::now();
                        evaluator.multiply_plain(temp_q_ct, encoded_db[(q_idx * RESPONSE_CT * N) + res_idx * N + (iter * N / 2) + i], temp_ct); // Need to update db index value
                        time_end = chrono::high_resolution_clock::now();
                        mult_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

                        time_start = chrono::high_resolution_clock::now();
                        evaluator.add_inplace(final_result[res_idx], temp_ct);
                        time_end = chrono::high_resolution_clock::now();
                        add_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
                    }
                }
                if (iter == 0)
                {
                    time_start = chrono::high_resolution_clock::now();
                    evaluator.rotate_columns_inplace(q_ct, gal_keys);
                    time_end = chrono::high_resolution_clock::now();
                    col_rot_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
                }
            }
        }
    }

    for (int i = 0; i < RESPONSE_CT; i++)
    {
        time_start = chrono::high_resolution_clock::now();
        evaluator.transform_from_ntt_inplace(final_result[i]);
        time_end = chrono::high_resolution_clock::now();
        inv_ntt_time += chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();
    }

    total_end = chrono::high_resolution_clock::now();

    cout << "Add time: " << endl
         << add_time << endl;
    cout << "Mult time: " << endl
         << mult_time << endl;
    cout << "Row rotation time: " << endl
         << row_rot_time << endl;
    cout << "Col rotation time: " << endl
         << col_rot_time << endl;
    cout << "NTT time: " << endl
         << ntt_time << endl;
    cout << "Inv NTT time: " << endl
         << inv_ntt_time << endl;

    cout << "Total time: " << endl
         << (chrono::duration_cast<chrono::microseconds>(total_end - total_start)).count() << endl;

    return 0;
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
