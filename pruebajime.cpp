#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>
#include <mpi.h>  // Incluir la librería de MPI
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <cmath>
#include <thread>
#include <future>
#include <functional>
#include <atomic>

using namespace std;

void encryptText(uint64_t key, const string& plain_text, string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_checked(&key_block, &schedule);

    cipher_text.resize(((plain_text.size() + 7) / 8) * 8);

    for (size_t i = 0; i < plain_text.size(); i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(plain_text.c_str() + i), (DES_cblock*)(cipher_text.data() + i), &schedule, DES_ENCRYPT);
    }
}

bool tryKey(uint64_t key, const string& cipher_text, const string& key_phrase, string& decrypted_str) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_checked(&key_block, &schedule);

    vector<char> decrypted_text(cipher_text.size(), 0);
    size_t cipher_text_length = cipher_text.size();

    for (size_t i = 0; i < cipher_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(cipher_text.c_str() + i), (DES_cblock*)(decrypted_text.data() + i), &schedule, DES_DECRYPT);
    }

    decrypted_str.assign(decrypted_text.data(), cipher_text_length);
    if (decrypted_str.find(key_phrase) != string::npos) {
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

// Función de búsqueda usando un pool de hilos
bool forkJoinSearch(uint64_t start_key, uint64_t end_key, const string& cipher_text, const string& key_phrase, uint64_t& found_key, string& found_decrypted_text) {
    const int max_threads = thread::hardware_concurrency();
    vector<thread> thread_pool;
    atomic<bool> found(false);
    mutex found_mutex;

    // Lambda para intentar una clave y actualizar el estado si se encuentra la correcta
    auto tryKeys = [&](uint64_t start, uint64_t step) {
        string decrypted_str;
        for (uint64_t key_candidate = start; key_candidate < end_key && !found.load(); key_candidate += step) {
            if (tryKey(key_candidate, cipher_text, key_phrase, decrypted_str)) {
                lock_guard<mutex> lock(found_mutex);
                if (!found.load()) {
                    found = true;
                    found_key = key_candidate;
                    found_decrypted_text = decrypted_str;
                }
                break;
            }
        }
    };

    // Crear un pool de hilos y distribuir el trabajo
    for (int i = 0; i < max_threads; ++i) {
        thread_pool.emplace_back(tryKeys, start_key + i, max_threads);
    }

    // Esperar a que todos los hilos terminen
    for (auto& t : thread_pool) {
        if (t.joinable()) {
            t.join();
        }
    }

    return found.load();
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
    uint64_t total_keys = pow(2, 20);  // Reducir el espacio de búsqueda para la demostración

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

    // Calcular el rango de claves para cada proceso
    uint64_t range_size = total_keys / size;
    uint64_t start_key = rank * range_size;
    uint64_t end_key = (rank == size - 1) ? total_keys : start_key + range_size;

    // Cada proceso intenta un subconjunto de las claves usando Fork-Join
    bool found = false;
    uint64_t found_key = 0;
    string found_decrypted_text;
    found = forkJoinSearch(start_key, end_key, cipher_text, key_phrase, found_key, found_decrypted_text);

    // Si se encuentra la clave, notificar a los demás procesos
    if (found) {
        for (int proc = 0; proc < size; proc++) {
            if (proc != rank) {
                MPI_Send(&found_key, 1, MPI_UINT64_T, proc, 0, MPI_COMM_WORLD);
                // Enviar también el tamaño y el texto descifrado
                int decrypted_length = found_decrypted_text.size();
                MPI_Send(&decrypted_length, 1, MPI_INT, proc, 0, MPI_COMM_WORLD);
                MPI_Send(found_decrypted_text.c_str(), decrypted_length, MPI_CHAR, proc, 0, MPI_COMM_WORLD);
            }
        }
        cout << "Proceso " << rank << " encontró la llave: " << found_key << "\n";
        cout << "Texto descifrado: " << found_decrypted_text << "\n";
    }

    // Verificar si hay algún mensaje de otro proceso indicando que la clave fue encontrada
    int flag;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
    if (flag) {
        // Recibir el mensaje con la clave encontrada
        MPI_Recv(&found_key, 1, MPI_UINT64_T, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        found = true;

        // Recibir el tamaño y el texto descifrado
        int decrypted_length;
        MPI_Recv(&decrypted_length, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        vector<char> decrypted_buffer(decrypted_length);
        MPI_Recv(decrypted_buffer.data(), decrypted_length, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        string received_decrypted_text(decrypted_buffer.data(), decrypted_length);

        if (rank != 0) {
            cout << "Proceso " << rank << " recibió notificación de clave encontrada: " << found_key << "\n";
            cout << "Texto descifrado: " << received_decrypted_text << "\n";
        }
    }

    // Fin de la medición del tiempo
    double end_time = MPI_Wtime();
    double elapsed_time = end_time - start_time;

    if (rank == 0) {
        cout << "Proceso " << rank << " - Clave encontrada!. Tiempo total de ejecución: " << fixed << setprecision(4) << elapsed_time << " segundos\n";
    }

    MPI_Finalize();  // Finalizar MPI
    return 0;
}
