/*
Proyecto MPI
Grupo 4

Parte A inciso 3: DES fuerza bruta

g++ -o build/naive-sequential.o naive-sequential.cpp -lcrypto
./build/naive-sequential.o input.txt

*/

#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>

using namespace std;

/*
Función encryptText
Parámetros:
    key: Llave de cifrado
    plain_text: Texto a cifrar
    cipher_text: Texto cifrado
Descripción:
    Cifra el texto plano usando la llave proporcionada
Retorno:
    void
*/
void encryptText(uint64_t key, const string& plain_text, string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    // Convertir la llave de uint64_t a DES_cblock
    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    char encrypted_text[256] = {0};  // Inicializar a cero
    size_t plain_text_length = plain_text.size();
    cipher_text.resize(((plain_text_length + 7) / 8) * 8);  // Asegurarse de que el tamaño es múltiplo de 8

    for (size_t i = 0; i < plain_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(plain_text.c_str() + i), (DES_cblock*)(encrypted_text + i), &schedule, DES_ENCRYPT);
    }

    cipher_text = string(encrypted_text, plain_text_length); // Ajustar tamaño
}

/*
Función tryKey
Parámetros:
    key: Llave a probar
    cipher_text: Texto cifrado
    key_phrase: Frase clave a buscar
Descripción:
    Descifra el texto cifrado usando la llave proporcionada y verifica si contiene la frase clave
Retorno:
    bool: Verdadero si la frase clave fue encontrada, falso en caso contrario
*/
bool tryKey(uint64_t key, const string& cipher_text, const string& key_phrase) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    // Convertir la llave de uint64_t a DES_cblock
    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    char decrypted_text[2048] = {0};  // Inicializar a cero
    size_t cipher_text_length = cipher_text.size();

    for (size_t i = 0; i < cipher_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(cipher_text.c_str() + i), (DES_cblock*)(decrypted_text + i), &schedule, DES_DECRYPT);
    }

    // Verificar si contiene la frase clave
    if (strstr(decrypted_text, key_phrase.c_str()) != nullptr) {
        cout << "Texto descifrado con la llave: " << key << " -> " << decrypted_text << "\n";
        return true;
    }

    return false;
}

/*
Función loadText
Parámetros:
    filename: Nombre del archivo a cargar
Descripción:
    Carga el contenido de un archivo de texto en un string
Retorno:
    string: Contenido del archivo
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

/*
Función generateKey
Parámetros:
    index: Índice para generar la llave
Descripción:
    Genera una llave a partir de un índice
Retorno:
    uint64_t: Llave generada
*/
uint64_t generateKey(unsigned long long index) {
    // Generar una llave a partir del índice
    return static_cast<uint64_t>(index);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Uso: " << argv[0] << " <archivo>" << endl;
        return 1;
    }

    string filename = argv[1];
    string plain_text = loadText(filename);
    string key_phrase;
    cout << "Ingrese la frase clave a buscar: ";
    getline(cin, key_phrase);

    string cipher_text;
    uint64_t key; // No se usará al inicio, solo para cifrado

    cout << "Ingrese una clave numérica para cifrar (0 - 2^64 - 1): ";
    cin >> key;

    cout << "Llave ingresada " << key << endl;

    if (key == 0) {
        cerr << "La llave no puede ser 0\n";
        return 1;
    }

    cout << "Usando la llave: " << key << endl;

    encryptText(key, plain_text, cipher_text);

    cout << "Texto cifrado: " << cipher_text << endl;

    // Empezar a medir el tiempo
    clock_t start_time = clock();

    // Iterar sobre todas las posibles combinaciones de llaves (0 a 2^64-1)
    for (uint64_t i = 0; i <= UINT64_MAX; i++) {
        // cout << "Probando llave: " << i << "\n";

        if (tryKey(i, cipher_text, key_phrase)) {
            cout << "Llave encontrada: " << i << "\n";
            break;
        }
    }

    // Fin de la medición del tiempo
    clock_t end_time = clock();
    double elapsed_time = static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC;
    cout << "Tiempo total de ejecución: " << fixed << setprecision(4) << elapsed_time << " segundos\n";

    return 0;
}
