/*
Proyecto MPI - Patrón Maestro/Esclavo
Grupo 4

Compilar: mpicxx master_slave_mpi.cpp -lcrypto -o master_slave_mpi.o
Ejecutar: mpirun -np <num_procesos> ./master_slave_mpi.o <archivo>
*/

#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>
#include <mpi.h>
#include <vector>

using namespace std;

void encryptText(uint64_t key_num, const string& plain_text, string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    // Convertir el uint64_t key_num en DES_cblock con el orden de bytes correcto
    for (int i = 0; i < 8; i++) {
        key_block[i] = (key_num >> (56 - 8 * i)) & 0xFF;
    }

    // Ajustar la paridad de la clave
    DES_set_odd_parity(&key_block);

    // Verificar si la clave es débil
    if (DES_is_weak_key(&key_block)) {
        cerr << "La clave ingresada es débil. Por favor, ingrese otra clave." << endl;
        exit(1);
    }

    // Establecer la clave
    if (DES_set_key_checked(&key_block, &schedule) != 0) {
        cerr << "Error al establecer la clave DES." << endl;
        exit(1);
    }

    // Ajustar el tamaño del texto cifrado
    size_t input_length = plain_text.size();
    size_t padded_length = ((input_length + 7) / 8) * 8;
    string padded_plain_text = plain_text;
    padded_plain_text.resize(padded_length, '\0');

    cipher_text.resize(padded_length, '\0');

    // Cifrar el texto en bloques de 8 bytes
    for (size_t i = 0; i < padded_length; i += 8) {
        DES_ecb_encrypt(
            (const_DES_cblock*)(padded_plain_text.data() + i),
            (DES_cblock*)(cipher_text.data() + i),
            &schedule, DES_ENCRYPT);
    }
}

bool tryKey(uint64_t key_num, const string& cipher_text, const string& key_phrase) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    // Convertir el uint64_t key_num en DES_cblock con el orden de bytes correcto
    for (int i = 0; i < 8; i++) {
        key_block[i] = (key_num >> (56 - 8 * i)) & 0xFF;
    }

    // Ajustar la paridad de la clave
    DES_set_odd_parity(&key_block);

    // Verificar si la clave es débil
    if (DES_is_weak_key(&key_block)) {
        // La clave es débil, ignorarla
        return false;
    }

    // Establecer la clave
    if (DES_set_key_checked(&key_block, &schedule) != 0) {
        // Error al establecer la clave
        return false;
    }

    size_t input_length = cipher_text.size();
    vector<char> decrypted_text(input_length, '\0');

    // Descifrar el texto en bloques de 8 bytes
    for (size_t i = 0; i < input_length; i += 8) {
        DES_ecb_encrypt(
            (const_DES_cblock*)(cipher_text.data() + i),
            (DES_cblock*)(decrypted_text.data() + i),
            &schedule, DES_DECRYPT);
    }

    // Convertir el texto descifrado a string
    string decrypted_str(decrypted_text.begin(), decrypted_text.end());

    // Verificar si contiene exactamente la frase clave
    if (decrypted_str.find(key_phrase) != string::npos) {
        cout << "Texto descifrado con la llave: " << key_num << " -> " << decrypted_str << "\n";
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
        // Proceso Maestro carga el texto y obtiene la frase clave y la clave de cifrado
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

    // Difundir la clave numérica y la frase clave a todos los procesos
    MPI_Bcast(&key, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    int phrase_length;
    if (rank == 0) {
        phrase_length = key_phrase.size();
    }
    MPI_Bcast(&phrase_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    key_phrase.resize(phrase_length);
    MPI_Bcast(&key_phrase[0], phrase_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    int cipher_length;
    if (rank == 0) {
        cipher_length = cipher_text.size();
    }
    MPI_Bcast(&cipher_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cipher_text.resize(cipher_length);
    MPI_Bcast(&cipher_text[0], cipher_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Empezar a medir el tiempo
    double start_time = MPI_Wtime();

    const uint64_t total_keys = UINT64_MAX;
    const uint64_t work_unit_size = 1000000; // Tamaño de cada unidad de trabajo

    bool key_found = false;
    uint64_t found_key = 0;

    if (rank == 0) {
        // Proceso Maestro
        uint64_t next_key = 0;
        int num_workers = size - 1;
        int active_workers = num_workers;
        MPI_Status status;

        while (!key_found && active_workers > 0) {
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int worker_rank = status.MPI_SOURCE;

            if (status.MPI_TAG == 0) {
                // Recibir solicitud de trabajo
                uint64_t worker_request;
                MPI_Recv(&worker_request, 1, MPI_UINT64_T, worker_rank, 0, MPI_COMM_WORLD, &status);

                if (next_key >= total_keys) {
                    // No hay más trabajo
                    uint64_t no_more_work = 0;
                    MPI_Send(&no_more_work, 1, MPI_UINT64_T, worker_rank, 0, MPI_COMM_WORLD);
                    active_workers--;
                } else {
                    // Enviar siguiente unidad de trabajo
                    uint64_t work_unit[2];
                    work_unit[0] = next_key;
                    if (next_key + work_unit_size > total_keys) {
                        work_unit[1] = total_keys;
                    } else {
                        work_unit[1] = next_key + work_unit_size - 1;
                    }
                    next_key = work_unit[1] + 1;
                    MPI_Send(work_unit, 2, MPI_UINT64_T, worker_rank, 1, MPI_COMM_WORLD);
                }
            } else if (status.MPI_TAG == 2) {
                // Recibir resultado de un esclavo que encontró la clave
                uint64_t result;
                MPI_Recv(&result, 1, MPI_UINT64_T, worker_rank, 2, MPI_COMM_WORLD, &status);
                key_found = true;
                found_key = result;

                // Notificar a todos los esclavos que detengan la búsqueda
                for (int i = 1; i < size; i++) {
                    if (i != worker_rank) {
                        uint64_t stop_signal = 0;
                        MPI_Send(&stop_signal, 1, MPI_UINT64_T, i, 3, MPI_COMM_WORLD);
                    }
                }
                active_workers--;
            }
        }

        // Recibir confirmación de los esclavos restantes
        while (active_workers > 0) {
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int worker_rank = status.MPI_SOURCE;

            if (status.MPI_TAG == 0) {
                // Recibir solicitud de trabajo
                uint64_t worker_request;
                MPI_Recv(&worker_request, 1, MPI_UINT64_T, worker_rank, 0, MPI_COMM_WORLD, &status);
                // Enviar señal de detener
                uint64_t stop_signal = 0;
                MPI_Send(&stop_signal, 1, MPI_UINT64_T, worker_rank, 3, MPI_COMM_WORLD);
                active_workers--;
            } else {
                // Recibir cualquier mensaje restante
                uint64_t dummy;
                MPI_Recv(&dummy, 1, MPI_UINT64_T, worker_rank, status.MPI_TAG, MPI_COMM_WORLD, &status);
            }
        }

    } else {
        // Procesos Esclavos
        bool found = false;
        uint64_t work_unit[2];
        MPI_Status status;

        while (!found) {
            // Solicitar trabajo al maestro
            uint64_t request = 1;
            MPI_Send(&request, 1, MPI_UINT64_T, 0, 0, MPI_COMM_WORLD);

            // Esperar respuesta del maestro
            MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == 0) {
                // No hay más trabajo
                uint64_t dummy;
                MPI_Recv(&dummy, 1, MPI_UINT64_T, 0, 0, MPI_COMM_WORLD, &status);
                break;
            } else if (status.MPI_TAG == 1) {
                // Recibir unidad de trabajo del maestro
                MPI_Recv(work_unit, 2, MPI_UINT64_T, 0, 1, MPI_COMM_WORLD, &status);

                uint64_t start = work_unit[0];
                uint64_t end = work_unit[1];

                // Búsqueda en el rango asignado
                for (uint64_t i = start; i <= end; i++) {
                    if (tryKey(i, cipher_text, key_phrase)) {
                        // Encontró la llave
                        cout << "Proceso " << rank << " encontró la llave: " << i << endl;
                        found = true;
                        // Notificar al maestro
                        uint64_t result = i;
                        MPI_Send(&result, 1, MPI_UINT64_T, 0, 2, MPI_COMM_WORLD);
                        break;
                    }
                }

                // Verificar si otro proceso encontró la clave
                int flag;
                MPI_Iprobe(0, 3, MPI_COMM_WORLD, &flag, &status);
                if (flag) {
                    // Recibir señal de detener
                    uint64_t dummy;
                    MPI_Recv(&dummy, 1, MPI_UINT64_T, 0, 3, MPI_COMM_WORLD, &status);
                    break;
                }

            } else if (status.MPI_TAG == 3) {
                // Recibir señal de detener
                uint64_t dummy;
                MPI_Recv(&dummy, 1, MPI_UINT64_T, 0, 3, MPI_COMM_WORLD, &status);
                break;
            }
        }
    }

    // Fin de la medición del tiempo
    double end_time = MPI_Wtime();
    double elapsed_time = end_time - start_time;

    if (rank == 0) {
        if (key_found) {
            // Imprimir la frase clave en lugar de la clave numérica
            cout << "La clave encontrada es: " << key_phrase << endl;
        } else {
            cout << "No se encontró la clave." << endl;
        }
        cout << "Tiempo total de ejecución: " << fixed << setprecision(2) << elapsed_time << " segundos" << endl;
    }

    MPI_Finalize();
    return 0;
}