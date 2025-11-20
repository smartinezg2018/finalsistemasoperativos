# GSEA - Gestión Segura y Eficiente de Archivos

**Proyecto Final - Sistemas Operativos**  
**Universidad EAFIT**

## Equipo

- **Samuel Aristizabal**
- **Agustín Figueroa**
- **Simón Martinez**

---

## Descripción

GSEA es una herramienta de línea de comandos desarrollada en C++ que permite realizar operaciones de compresión, descompresión, encriptación y desencriptación de archivos de manera eficiente. El proyecto implementa algoritmos avanzados con soporte para procesamiento paralelo y manejo de archivos grandes mediante técnicas de chunking.

## Características Principales

### Algoritmos de Compresión
- **LZW (Lempel-Ziv-Welch)**: Compresión sin pérdidas con procesamiento por chunks y paralelismo
- **Huffman**: Codificación adaptativa con árboles de Huffman, soporte para chunks y procesamiento paralelo

### Algoritmos de Encriptación
- **Vigenère**: Cifrado de sustitución polialfabética con clave repetida

### Funcionalidades Avanzadas
- ✅ Procesamiento de archivos individuales y directorios completos
- ✅ Procesamiento paralelo con múltiples threads para mejor rendimiento
- ✅ Manejo eficiente de archivos grandes mediante chunking
- ✅ Operaciones combinadas (comprimir+encriptar, desencriptar+descomprimir)
- ✅ Compatibilidad multiplataforma (Windows y Linux)
- ✅ Interfaz de línea de comandos completa y menú interactivo

## Compilación

### Requisitos
- Compilador C++ compatible con C++17 (g++ o clang++)
- Make
- Sistema operativo: Windows (MinGW) o Linux

### Compilar el proyecto

```bash
make all
```

Esto generará dos ejecutables:
- `main` / `main.exe`: Programa con menú interactivo
- `gsea` / `gsea.exe`: Herramienta CLI completa

### Limpiar archivos compilados

```bash
make clean
```

Esto eliminará todos los archivos objeto y ejecutables generados.

## Uso

### Interfaz de Menú Interactivo

Ejecutar el programa principal:

```bash
./main
# o en Windows
./main.exe
```

El menú ofrece las siguientes opciones:
1. Encriptar
2. Desencriptar
3. Comprimir
4. Descomprimir
5. Encriptar y comprimir
6. Desencriptar y descomprimir

### Interfaz de Línea de Comandos (CLI)

#### Sintaxis General

```bash
./gsea [OPERACIONES] [OPCIONES] -i INPUT -o OUTPUT
```

#### Operaciones

- `-c, --compress`: Comprimir archivo(s)
- `-d, --decompress`: Descomprimir archivo(s)
- `-e, --encrypt`: Encriptar archivo(s)
- `-u, --decrypt`: Desencriptar archivo(s)

Las operaciones pueden combinarse (ej: `-ce` para comprimir y encriptar).

#### Opciones

- `-i, --input PATH`: Archivo o directorio de entrada (requerido)
- `-o, --output PATH`: Archivo o directorio de salida (requerido)
- `-k, --key KEY`: Clave para encriptación/desencriptación
  - Puede ser texto directo o ruta a un archivo con la clave
- `--comp-alg ALG`: Algoritmo de compresión (`lzw` o `huffman`, default: `lzw`)
- `--enc-alg ALG`: Algoritmo de encriptación (`vigenere`, default: `vigenere`)
- `-h, --help`: Mostrar ayuda

## Ejemplos de Uso

### Compresión

```bash
# Comprimir un archivo con LZW
./gsea -c -i documento.txt -o documento.lzw --comp-alg lzw

# Comprimir un archivo con Huffman
./gsea -c -i documento.txt -o documento.huff --comp-alg huffman

# Comprimir todos los archivos de un directorio
./gsea -c -i ./archivos/ -o ./comprimidos/ --comp-alg lzw
```

### Descompresión

```bash
# Descomprimir archivo LZW
./gsea -d -i documento.lzw -o documento.txt --comp-alg lzw

# Descomprimir archivo Huffman
./gsea -d -i documento.huff -o documento.txt --comp-alg huffman
```

### Encriptación

```bash
# Encriptar con clave como texto
./gsea -e -i documento.txt -o documento.enc -k "mi_clave_secreta"

# Encriptar con clave desde archivo
./gsea -e -i documento.txt -o documento.enc -k keyfile.txt
```

### Desencriptación

```bash
# Desencriptar archivo
./gsea -u -i documento.enc -o documento.txt -k "mi_clave_secreta"
```

### Operaciones Combinadas

```bash
# Comprimir y encriptar (primero comprime, luego encripta)
./gsea -ce -i documento.txt -o documento.enc.lzw -k "mi_clave" --comp-alg lzw

# Desencriptar y descomprimir (primero desencripta, luego descomprime)
./gsea -ud -i documento.enc.lzw -o documento.txt -k "mi_clave" --comp-alg lzw
```

### Procesamiento de Directorios

```bash
# Procesar todos los archivos de un directorio
./gsea -c -i ./mis_archivos/ -o ./archivos_comprimidos/ --comp-alg huffman
```

El programa detecta automáticamente si la entrada es un directorio y procesa todos los archivos regulares en paralelo.

## Estructura del Proyecto

```
finalSistemasOperativos/
├── archivosPrueba/      # Archivos de prueba
├── main.cpp             # Programa principal con menú
├── menu.cpp             # Implementación del menú interactivo
├── gsea_cli.cpp         # CLI completa
├── filePartitioner.cpp  # Manejo de chunks y paralelismo para LZW
├── lzw.cpp              # Implementación de LZW
├── huffman.cpp          # Implementación de Huffman con chunks
├── vigenere.cpp         # Implementación de Vigenère
├── vigenere.h           # Encabezado de Vigenère
├── fileEncrypting.cpp   # Funciones de alto nivel para encriptación
├── fileEncrypting.h     # Encabezado de fileEncrypting
├── Makefile             # Archivo de compilación
└── README.md            # Este archivo
```

## Detalles Técnicos

### Procesamiento Paralelo

- **LZW**: Los archivos grandes se dividen en chunks de 1MB que se procesan en paralelo usando `std::thread`
- **Huffman**: Similar a LZW, cada chunk tiene su propio árbol de frecuencias y se procesa en paralelo
- **Directorio**: Los archivos de un directorio se procesan concurrentemente

### Manejo de Archivos Grandes

- División automática en chunks para evitar problemas de memoria
- Formato de archivo con magic numbers para identificación
- Metadatos incluidos para reconstrucción correcta

### Compatibilidad

- **Windows**: Compilado y probado con MinGW
- **Linux**: Compatible con compiladores estándar
- Usa llamadas al sistema POSIX (`open`, `read`, `write`, `close`) para máxima portabilidad

## Notas Importantes

1. **Claves de Encriptación**: La clave puede ser cualquier texto. Puede proporcionarse como:
   - Texto directo (ej: `"mi_clave_secreta"`)
   - Archivo de texto con la clave (se lee el contenido completo del archivo)

2. **Extensiones de Archivos**:
   - LZW: `.lzw`
   - Huffman: `.huff`
   - Encriptado: `.enc`
   - Desencriptado: `.dec`

3. **Orden de Operaciones Combinadas**:
   - `-ce` (comprimir y encriptar): Primero comprime, luego encripta
   - `-ud` (desencriptar y descomprimir): Primero desencripta, luego descomprime

## Licencia

Este proyecto fue desarrollado como trabajo académico para la clase de Sistemas Operativos de la Universidad EAFIT.

