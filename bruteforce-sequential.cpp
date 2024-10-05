/*
Proyecto MPI
Grupo 4

Parte A: DES fuerza bruta

*/

#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cmath>

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

// Función para probar todas las posibles llaves (fuerza bruta)
string bruteForce(string ciphertext, string keyword, size_t keyLength) {
    size_t totalKeys = pow(256, keyLength);  // Total de combinaciones de llaves
    string keyCandidate(keyLength, 0);

    // Probar cada combinación posible de la llave
    for (size_t i = 0; i < totalKeys; ++i) {
        // Generar la llave candidata
        for (size_t j = 0; j < keyLength; ++j) {
            keyCandidate[j] = (i >> (j * 8)) & 0xFF;
        }

        // Descifrar con la llave candidata
        string decryptedText = xorCipher(ciphertext, keyCandidate);

        // Comprobar si contiene la palabra clave
        if (decryptedText.find(keyword) != string::npos) {
            return keyCandidate;  // Se encontró la llave correcta
        }
    }

    return "";  // No se encontró la llave
}

int main() {
    // Texto plano original y la palabra clave a buscar
    string plaintext = "Este es el texto original con el que vamos a trabajar.";
    string keyword = "original";  // Palabra clave en el texto descifrado
    string key = "clave";  // Llave con la que se cifró el texto

    // Cifrar el texto original
    string ciphertext = xorCipher(plaintext, key);

    // Probar diferentes longitudes de llaves
    vector<size_t> keyLengths = {3, 4, 5};  // Longitudes de las llaves a probar

    for (size_t keyLength : keyLengths) {
        cout << "Probando con longitud de llave: " << keyLength << endl;

        // Medir el tiempo de ejecución
        auto start = high_resolution_clock::now();

        string foundKey = bruteForce(ciphertext, keyword, keyLength);

        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);

        if (!foundKey.empty()) {
            cout << "Llave encontrada: " << foundKey << endl;
        } else {
            cout << "Llave no encontrada" << endl;
        }

        cout << "Tiempo de ejecución: " << duration.count() << " ms" << endl;
        cout << "--------------------------------------------" << endl;
    }

    return 0;
}
