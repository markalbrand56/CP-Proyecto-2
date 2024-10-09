#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>
#include <mpi.h>
#include <vector>

using namespace std;

void encryptText(uint64_t key, const string& plain_text, string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    cipher_text.resize(((plain_text.size() + 7) / 8) * 8);

    for (size_t i = 0; i < plain_text.size(); i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(plain_text.c_str() + i), (DES_cblock*)(cipher_text.data() + i), &schedule, DES_ENCRYPT);
    }
}

bool tryKey(uint64_t key, const string& cipher_text, const string& key_phrase) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    vector<char> decrypted_text(cipher_text.size(), 0);
    size_t cipher_text_length = cipher_text.size();

    for (size_t i = 0; i < cipher_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(cipher_text.c_str() + i), (DES_cblock*)(decrypted_text.data() + i), &schedule, DES_DECRYPT);
    }

    string decrypted_str(decrypted_text.data(), cipher_text_length);
    if (decrypted_str.find(key_phrase) != string::npos) {
        cout << "Texto descifrado con la llave: " << key << " -> " << decrypted_str << "\n";
        return true;
    }

    return false;
}

string loadText(const string& filename) {
    ifstream file(filename);

    if (!file.is_open()) {
        cerr << "No se pudo abrir el archivo " << filename << endl;
        return "";
    }

    string text((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    return text;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    string key_phrase;
    uint64_t key;
    string cipher_text;
    string plain_text;

    if (argc != 2) {
        if (rank == 0) {
            cerr << "Uso: " << argv[0] << " <archivo>" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        string filename = argv[1];
        plain_text = loadText(filename);

        cout << "Ingrese la frase clave a buscar: ";
        getline(cin, key_phrase);

        cout << "Ingrese una clave numérica para cifrar (0 - 2^64 - 1): ";
        cin >> key;

        encryptText(key, plain_text, cipher_text);
        cout << "Texto cifrado: " << cipher_text << endl;
    }

    int phrase_length = key_phrase.size();
    MPI_Bcast(&phrase_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    key_phrase.resize(phrase_length);
    MPI_Bcast(&key_phrase[0], phrase_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    int cipher_length = cipher_text.size();
    MPI_Bcast(&cipher_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cipher_text.resize(cipher_length);
    MPI_Bcast(&cipher_text[0], cipher_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    uint64_t found_key = 0;
    bool found = false;
    bool found_global = false;

    uint64_t range_size = 50000000;
    uint64_t start = rank * range_size;
    uint64_t end = start + range_size;

    cout << "Proceso " << rank << " busca en el rango [" << start << ", " << end << ")\n";

    double start_time = MPI_Wtime();

    for (uint64_t i = start; i < end && !found; i++) {
        MPI_Bcast(&found_global, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (found_global) break;

        if (tryKey(i, cipher_text, key_phrase)) {
            found = true;
            found_key = i;
            found_global = true;

            MPI_Bcast(&found_global, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
            for (int proc = 0; proc < size; proc++) {
                if (proc != rank) {
                    MPI_Send(&found_key, 1, MPI_UINT64_T, proc, 0, MPI_COMM_WORLD);
                }
            }
            cout << "Proceso " << rank << " encontró la llave: " << found_key << "\n";
        }
    }

    double end_time = MPI_Wtime();
    double elapsed_time = end_time - start_time;

    if (rank == 0) {
        cout << "Clave encontrada. Tiempo total de ejecución: " << fixed << setprecision(4) << elapsed_time << " segundos\n";
    }

    MPI_Finalize();
    return 0;
}
