/*
Proyecto MPI
Grupo 4

Parte A inciso 3: DES fuerza bruta

g++ -o build/bruteforce-sequential.o bruteforce-sequential.cpp
./build/bruteforce-sequential.o

*/
#include <iostream>
#include <cstring>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <openssl/des.h>

void encryptText(const std::string& key, const std::string& plain_text, std::string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;
    memcpy(key_block, key.c_str(), sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    char encrypted_text[256] = {0};  // Inicializar a cero
    size_t plain_text_length = plain_text.size();
    cipher_text.resize(((plain_text_length + 7) / 8) * 8);  // Asegurarse de que el tamaño es múltiplo de 8

    for (size_t i = 0; i < plain_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(plain_text.c_str() + i), (DES_cblock*)(encrypted_text + i), &schedule, DES_ENCRYPT);
    }
    
    cipher_text = std::string(encrypted_text, plain_text_length); // Ajustar tamaño
}

bool tryKey(const std::string& key, const std::string& cipher_text, const std::string& key_phrase) {
    DES_cblock key_block;
    DES_key_schedule schedule;
    memcpy(key_block, key.c_str(), sizeof(key_block));
    DES_set_key_unchecked(&key_block, &schedule);

    char decrypted_text[256] = {0};  // Inicializar a cero
    size_t cipher_text_length = cipher_text.size();

    for (size_t i = 0; i < cipher_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(cipher_text.c_str() + i), (DES_cblock*)(decrypted_text + i), &schedule, DES_DECRYPT);
    }

    // Verificar si contiene la frase clave
    if (strstr(decrypted_text, key_phrase.c_str()) != nullptr) {
        std::cout << "Texto descifrado con la llave: " << key << " -> " << decrypted_text << "\n";
        return true;
    }

    // if (key == "AAAAEMJC") {
    //     std::cout << "Texto descifrado con la llave: " << key << " -> " << decrypted_text << "\n";
    //     return true;
    // }

    return false;
}

std::string loadText(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "No se pudo abrir el archivo " << filename << std::endl;
        return "";
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return text;
}

const std::string valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

std::string generateKey(unsigned long long index) {
    std::string key(8, ' '); // Crear una cadena de 8 espacios

    for (int i = 0; i < 8; ++i) {
        key[7 - i] = valid_chars[index % valid_chars.size()]; // Obtener el carácter correspondiente
        index /= valid_chars.size(); // Dividir el índice para la siguiente posición
    }

    return key;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <archivo> <clave para cifrar>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];

    std::string plain_text = loadText(filename);
    std::string key_phrase;
    std::cout << "Ingrese la frase clave a buscar: ";
    std::getline(std::cin, key_phrase);

    std::string cipher_text;

    std::string key = argv[2];

    encryptText(key, plain_text, cipher_text);

    std::cout << "Texto cifrado: " << cipher_text << std::endl;

    // Empezar a medir el tiempo
    clock_t start_time = clock();

    // Iterar sobre todas las posibles combinaciones de llaves (2^56 para DES)
    for (unsigned long long i = 0; i < (1ULL << 56); i++) {
        std::string key = generateKey(i);

        std::cout << "Probando llave: " << key << "\n";

        if (tryKey(key, cipher_text, key_phrase)) {
            std::cout << "Llave encontrada: " << key << "\n";
            break;
        }
    }

    // Fin de la medición del tiempo
    clock_t end_time = clock();
    double elapsed_time = static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC;
    std::cout << "Tiempo total de ejecución: " << std::fixed << std::setprecision(2) << elapsed_time << " segundos\n";

    return 0;
}
