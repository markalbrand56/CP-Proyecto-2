#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>
#include <mpi.h>
#include <vector>

using namespace std;

/*
Función encyptText
Parámetros:
    key: clave de cifrado
    plain_text: texto a cifrar
    cipher_text: texto cifrado
Descripción:
    Cifra el texto plano usando la clave de cifrado dada.
    El texto cifrado se almacena en la variable cipher_text.
*/
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

/*
Función tryKey
Parámetros:
    key: clave a probar
    cipher_text: texto cifrado
    key_phrase: frase clave a buscar
Retorno:
    true si la clave descifra el texto cifrado y contiene la frase clave, false en caso contrario
Descripción:
    Descifra el texto cifrado usando la clave dada y verifica si contiene la frase clave.
*/
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

/*
Función loadText
Parámetros:
    filename: nombre del archivo a cargar
Retorno:
    contenido del archivo
Descripción:
    Carga el contenido de un archivo en una cadena de texto.
*/
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

        cout << "Llave ingresada " << key << endl;

        if (key == 0) {
            cerr << "La llave no puede ser 0\n";
            MPI_Finalize();
            return 1;
        }

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

    // Enfoque paralelo sin maestro-esclavo: los procesos solicitan un rango dinámico
    uint64_t found_key = 0;
    bool found = false;
    bool stop = false;

    MPI_Request request;
    MPI_Status status;

    // Cada proceso trabaja en un rango de claves
    uint64_t range_size = 50000000;  // Ajustar el tamaño del rango dinámico
    uint64_t start = rank * range_size;
    uint64_t end = start + range_size;

    cout << "Proceso " << rank << " busca en el rango: [" << start << ", " << end << ")\n";

    // Búsqueda en el rango asignado
    for (uint64_t i = start; i < end && !found; i++) {
        if (tryKey(i, cipher_text, key_phrase)) {
            found = true;
            found_key = i;

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
    }

    if (found) {
        // Si el proceso encuentra la clave, lo comunica a los demás
        for (int proc = 0; proc < size; proc++) {
            if (proc != rank) {
                MPI_Send(&found_key, 1, MPI_UINT64_T, proc, 0, MPI_COMM_WORLD);
            }
        }

        cout << "Proceso " << rank << " encontró la llave: " << found_key << "\n";

        stop = true;
        MPI_Bcast(&stop, 1, MPI_C_BOOL, rank, MPI_COMM_WORLD);
    } else {
        // Si no encuentra la clave, consulta con otros procesos
        while (!found) {
            int flag;
            MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

            if (flag) {
                // Recibe la clave encontrada por otro proceso
                MPI_Recv(&found_key, 1, MPI_UINT64_T, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
                found = true;
            } else {
                // Si no hay mensaje, continúa con la búsqueda en otro rango
                start += range_size * size;
                end = start + range_size;

                cout << "Proceso " << rank << " busca en el rango: [" << start << ", " << end << ")\n";

                for (uint64_t i = start; i < end && !found; i++) {
                    if (tryKey(i, cipher_text, key_phrase)) {
                        found = true;
                        found_key = i;
                        break;
                    }

                    // Verificar si hay algún mensaje de otro proceso indicando que la clave fue encontrada
                    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

                    if (flag) {
                        // Recibir el mensaje con la clave encontrada
                        MPI_Recv(&found_key, 1, MPI_UINT64_T, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
                        found = true;  // Detener la búsqueda
                        break;
                    }
                }
            }
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
