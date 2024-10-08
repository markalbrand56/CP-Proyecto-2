/*
Proyecto MPI
Grupo 4

Parte B: DES Naive

mpicc naive-mpi.cpp -lcrypto -o /build/naive-mpi.o
mpirun -np 4 ./build/naive-mpi.o <archivo>

*/

#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>
#include <mpi.h>  // Incluir la librería de MPI
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

    // Verificar si contiene exactamente la frase clave
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
    MPI_Init(&argc, &argv);  // Inicializar MPI

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // Obtener el ID del proceso
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // Obtener el número de procesos

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
        // Solo el proceso maestro carga el texto y pide la frase clave y la clave de cifrado
        string filename = argv[1];
        plain_text = loadText(filename);

        cout << "Ingrese la frase clave a buscar: ";
        getline(cin, key_phrase);

        cout << "Ingrese una clave numérica para cifrar (0 - 2^64 - 1): ";
        cin >> key;

        // Cifrar el texto usando la clave dada
        encryptText(key, plain_text, cipher_text);

        cout << "Texto cifrado: " << cipher_text << endl;
    }

    // Enviar la clave numérica y la frase clave a todos los procesos
    MPI_Bcast(&key, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    // Enviar la frase clave a todos los procesos
    int phrase_length = key_phrase.size();
    MPI_Bcast(&phrase_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    key_phrase.resize(phrase_length);
    MPI_Bcast(&key_phrase[0], phrase_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Enviar el texto cifrado a todos los procesos
    int cipher_length = cipher_text.size();
    MPI_Bcast(&cipher_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cipher_text.resize(cipher_length);
    MPI_Bcast(&cipher_text[0], cipher_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Empezar a medir el tiempo
    clock_t start_time = clock();

    // Cada proceso trabaja en un rango de llaves
    uint64_t range = UINT64_MAX / size;
    uint64_t start = rank * range;
    uint64_t end = (rank == size - 1) ? UINT64_MAX : (start + range - 1);

    bool found = false;
    uint64_t found_key = 0;

    // Búsqueda por fuerza bruta en el rango asignado
    for (uint64_t i = start; i <= end && !found; i++) {
        if (tryKey(i, cipher_text, key_phrase)) {
            found_key = i;
            found = true;
            cout << "Proceso " << rank << " encontró la llave: " << i << "\n";
        }
        // Verificar si algún proceso ha encontrado la llave
        MPI_Allreduce(MPI_IN_PLACE, &found, 1, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD);
        if (found) {
            break;
        }
    }

    // Fin de la medición del tiempo
    clock_t end_time = clock();
    double elapsed_time = static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC;

    if (rank == 0) {
        cout << "Tiempo total de ejecución: " << fixed << setprecision(2) << elapsed_time << " segundos\n";
    }

    MPI_Finalize();  // Finalizar MPI
    return 0;
}