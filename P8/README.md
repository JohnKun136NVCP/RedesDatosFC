# Cómo usar este programa

## Primero es compilar el programa

```bash
g++ -std=c++11 -o cisco7decrypt ciscodecript7.cpp
```

## ¿Cómo usar?

```bash
# Contraseña por terminal

./cisco7decrypt -d -p XXXXXX

# Encriptar

./cisco7decrypt -e -p XXXXXX

# Desencriptar archivo de configuración
./cisco7decrypt -f  XXXXXX

```
