/*
Proyecto MPI
Grupo 4

Parte B inciso 1: DES

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Función para cifrar y descifrar con XOR
string xorCipher(string text, string key) {
    string output = text;
    for (size_t i = 0; i < text.size(); ++i) {
        output[i] = text[i] ^ key[i % key.size()];
    }
    return output;
}

string loadText(string filename){
    ifstream file(filename);
    if (!file.is_open()){
        cerr << "No se pudo abrir el archivo " << filename << endl;
        return "";
    }

    string text((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    return text;
}

int main(int argc, char *argv[]){

    if (argc != 3){
        cerr << "Uso: " << argv[0] << " <archivo> <clave>" << endl;
    }

    string filename = argv[1];
    string key = argv[2];

    string plaintext = loadText(filename);
    string ciphertext = xorCipher(plaintext, key);

    cout << "Texto cifrado: " << ciphertext << endl;

    string decryptedText = xorCipher(ciphertext, key);

    cout << "Texto descifrado: " << decryptedText << endl;

    return 0;
}