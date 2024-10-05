# CP-Proyecto-2
 Computación Paralela: Proyecto 2

``` bash
mpicc -o bruteforce.o bruteforce.c -lcrypto
mpirun -np 4 ./bruteforce.o 
```


``` bash
mpicc -o bruteforce.o bruteforce.c -I/opt/homebrew/opt/openssl/include -L/opt/homebrew/opt/openssl/lib -lcrypto
```

