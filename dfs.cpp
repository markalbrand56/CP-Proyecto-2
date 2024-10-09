/*
Proyecto MPI
Grupo 4

Parte B: DES Naive

mpic++ naive-mpi.cpp -lcrypto -o build/naive-mpi.o
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

    // Enviar la frase clave y el texto cifrado a todos los procesos
    int phrase_length = key_phrase.size();
    MPI_Bcast(&phrase_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    key_phrase.resize(phrase_length);
    MPI_Bcast(&key_phrase[0], phrase_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    int cipher_length = cipher_text.size();
    MPI_Bcast(&cipher_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cipher_text.resize(cipher_length);
    MPI_Bcast(&cipher_text[0], cipher_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Empezar a medir el tiempo
    double start_time = MPI_Wtime();

    // Cada proceso trabaja en un rango de llaves
    uint64_t start = rank;
    bool found = false;
    uint64_t found_key = 0;

    // Canal de mensajes (para comunicar clave encontrada)
    MPI_Request request;
    MPI_Status status;
    bool message_received = false;

    // Búsqueda utilizando un algoritmo más eficiente: Búsqueda en profundidad primero (DFS)
    vector<uint64_t> stack;
    stack.push_back(start);

    while (!stack.empty() && !found) {
        uint64_t current_key = stack.back();
        stack.pop_back();

        if (tryKey(current_key, cipher_text, key_phrase)) {
            found_key = current_key;
            found = true;

            // Enviar mensaje a los demás procesos para indicar que la clave fue encontrada
            for (int proc = 0; proc < size; proc++) {
                if (proc != rank) {
                    MPI_Send(&found_key, 1, MPI_UINT64_T, proc, 0, MPI_COMM_WORLD);
                }
            }
            break;
        }

        // Verificar si hay algún mensaje de otro proceso indicando que la clave fue encontrada
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            // Recibir el mensaje con la clave encontrada
            MPI_Recv(&found_key, 1, MPI_UINT64_T, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            found = true;  // Detener la búsqueda
            break;
        }

        // Agregar próximas posibles llaves a la pila
        if (current_key + size <= UINT64_MAX && !found) {
            stack.push_back(current_key + size);
        }
    }

    // Fin de la medición del tiempo
    double end_time = MPI_Wtime();
    double elapsed_time = end_time - start_time;

    if (rank == 0) {
        cout << "Clave encontrada. Tiempo total de ejecución: " << fixed << setprecision(4) << elapsed_time << " segundos\n";
    }

    MPI_Finalize();  // Finalizar MPI
    return 0;
}