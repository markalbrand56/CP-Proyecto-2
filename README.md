# Proyecto MPI - Algoritmos de Cifrado DES
## Computación Paralela: Proyecto 2

### Integrantes
- Mark Albrand (21004)
- Jimena Hernández (21199)
- Javier Heredia (21600)

### Descripción
Para este proyecto se diseño un programa que encuentra la llave privada con la que fue cifrado un texto plano. La búsqueda se hará probando todas las posibles combinaciones de llaves, hasta encontrar una que descifra el texto (fuerza bruta). También se presentan 4 enfoques diferentes utilizando DES y MPI.

1. **Versión Naive (dynamic range)**: Se divide el rango de llaves a probar en partes iguales y se asigna a cada proceso una parte del rango. Cada proceso prueba todas las llaves en su rango y se detiene cuando encuentra la llave correcta.

2. **Versión Naive (búsqueda distribuida por fuerza bruta)**: Utiliza la misma idea que la versión anterior, pero en lugar de dividir el rango de llaves, todos los procesos inician en el mismo índice que su identificador, y por cada iteración se mueven la misma cantidad de procesos. 

3. **Versión Master Slave**: El proceso maestro administra los rangos de llaves y asigna trabajos a los esclavos según lo soliciten, balanceando la carga de manera eficiente. Los esclavos ejecutan la búsqueda en los rangos asignados y devuelven los resultados, solicitando nuevos rangos si aún no se encuentra la llave.

4. **Versión Depth First Search (DFS)**: Se implementa un algoritmo de búsqueda en profundidad para encontrar la llave. Cada proceso se encarga de probar una rama del árbol de búsqueda, y se detiene cuando encuentra la llave correcta.

### Compilación y Ejecución
Para compilar el programa se debe ejecutar el siguiente comando:

``` bash
mpic++ <programa>.cpp -lcrypto -o build/<programa>.o

mpirun -np <n> ./build/<programa>.o <archivo>.txt
```

Donde:
``` bash
- <programa> es el nombre del programa a compilar.
- <n> es el número de procesos a utilizar.
- <archivo> es el archivo de texto a descifrar.
```

### Funciones principales
- **`encryptText`**: Cifra un texto plano utilizando DES.
- **`tryKey`**:     Intenta descifrar el texto cifrado usando la clave dada y verifica si contiene la frase clave. Si la encuentra, imprime el texto descifrado y retorna verdadero.
- **`decryptText`**: Descifra un texto cifrado utilizando DES.

### Resultados
Los resultados de este proyecto se encuentran en el archivo pdf adjunto.

### Requerimientos
- OpenMPI
- OpenSSL




