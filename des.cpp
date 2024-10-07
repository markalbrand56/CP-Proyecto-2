/*
Proyecto MPI
Grupo 4

Parte B inciso 1: DES

g++ -o build/des.o des.cpp -lssl -lcrypto
./build/des.o

*/

/*
Proyecto MPI
Grupo 4

Parte A inciso 3: DES cifrado simple

g++ -o build/encrypt-text.o encrypt-text.cpp
./build/encrypt-text.o

*/

#include <iostream>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <openssl/des.h>

using namespace std;

void encryptText(uint64_t key, const string& plain_text, string& cipher_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key(&key_block, &schedule);

    size_t padded_length = ((plain_text.size() + 7) / 8) * 8;  // Longitud múltiplo de 8
    char* encrypted_text = new char[padded_length]();

    for (size_t i = 0; i < plain_text.size(); i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(plain_text.c_str() + i), (DES_cblock*)(encrypted_text + i), &schedule, DES_ENCRYPT);
    }

    cipher_text = string(encrypted_text, padded_length);
    delete[] encrypted_text;
}

void decrypt(uint64_t key, const string& cipher_text, string& plain_text) {
    DES_cblock key_block;
    DES_key_schedule schedule;

    memcpy(key_block, &key, sizeof(key_block));
    DES_set_key(&key_block, &schedule);

    size_t cipher_text_length = cipher_text.size();
    char* decrypted_text = new char[cipher_text_length]();

    for (size_t i = 0; i < cipher_text_length; i += 8) {
        DES_ecb_encrypt((const_DES_cblock*)(cipher_text.c_str() + i), (DES_cblock*)(decrypted_text + i), &schedule, DES_DECRYPT);
    }

    plain_text = string(decrypted_text, cipher_text_length);
    delete[] decrypted_text;
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
    if (argc != 2) {
        cerr << "Uso: " << argv[0] << " <archivo>" << endl;
        return 1;
    }

    string filename = argv[1];
    string plain_text = loadText(filename);
    if (plain_text.empty()) {
        return 1;
    }

    cout << "Texto original: " << plain_text << endl;

    uint64_t key;
    cout << "Ingrese una llave numérica para cifrar (0 - 2^64 - 1): ";
    cin >> key;

    cout << "Llave utilizada para cifrar: " << key << endl;

    string cipher_text;
    encryptText(key, plain_text, cipher_text);
    cout << "Texto cifrado: " << cipher_text << endl;

    cout << "Llave utilizada para descifrar: " << key << endl;

    string decrypted_text;
    decrypt(key, cipher_text, decrypted_text);
    cout << "Texto descifrado: " << decrypted_text << endl;

    return 0;
}