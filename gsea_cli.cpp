#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include "fileEncrypting.h"
#include "filePartitioner.h"
#include "huffman.h"

using namespace std;

static void print_usage(const char *progname) {
    fprintf(stderr,
        "GSEA - Gestión Segura y Eficiente de Archivos\n"
        "Usage: %s [OPERACIONES] [OPCIONES] -i INPUT -o OUTPUT\n\n"
        "OPERACIONES (pueden combinarse):\n"
        "  -c, --compress      Comprimir\n"
        "  -d, --decompress    Descomprimir\n"
        "  -e, --encrypt        Encriptar\n"
        "  -u, --decrypt        Desencriptar\n\n"
        "OPCIONES:\n"
        "  -i, --input PATH     Archivo o directorio de entrada (requerido)\n"
        "  -o, --output PATH    Archivo o directorio de salida (requerido)\n"
        "  -k, --key KEY        Clave para encriptación/desencriptación\n"
        "                       Puede ser texto o ruta a archivo con la clave\n"
        "  --comp-alg ALG       Algoritmo de compresión: lzw o huffman (default: lzw)\n"
        "  --enc-alg ALG        Algoritmo de encriptación: vigenere (default: vigenere)\n\n"
        "EJEMPLOS:\n"
        "  %s -c -i archivo.txt -o archivo.lzw --comp-alg lzw\n"
        "  %s -e -i archivo.txt -o archivo.enc -k \"mi_clave_secreta\"\n"
        "  %s -e -i archivo.txt -o archivo.enc -k keyfile.txt\n"
        "  %s -ce -i archivo.txt -o archivo.enc.lzw -k \"mi_clave\" --comp-alg huffman\n"
        "  %s -d -i archivo.lzw -o archivo.txt --comp-alg lzw\n",
        progname, progname, progname, progname, progname);
}

// Función para leer la clave desde archivo o usar como texto
static string getKey(const char* keyopt) {
    if (!keyopt) return "";
    
    // Intentar leer como archivo primero
    int fd = open(keyopt, O_RDONLY);
    if (fd >= 0) {
        struct stat st;
        if (fstat(fd, &st) == 0 && st.st_size > 0) {
            char* buffer = new char[st.st_size + 1];
            ssize_t r = read(fd, buffer, st.st_size);
            close(fd);
            if (r > 0) {
                buffer[r] = '\0';
                string key(buffer, r);
                delete[] buffer;
                return key;
            }
            delete[] buffer;
        } else {
            close(fd);
        }
    }
    
    // Si no es archivo, usar como texto directamente
    return string(keyopt);
}

// Función para verificar si una ruta es un directorio
static bool isDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

// Función para obtener todos los archivos de un directorio (no recursivo por ahora)
static vector<string> getFilesInDirectory(const char* dirPath) {
    vector<string> files;
    DIR* dir = opendir(dirPath);
    if (dir == NULL) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar . y ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Construir ruta completa
        string fullPath = string(dirPath);
        if (fullPath.back() != '/' && fullPath.back() != '\\') {
            fullPath += "/";
        }
        fullPath += entry->d_name;
        
        // Verificar que sea un archivo regular (no directorio)
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            files.push_back(fullPath);
        }
    }
    
    closedir(dir);
    return files;
}

// Función para generar nombre de archivo de salida basado en el de entrada
static string generateOutputFilename(const string& inputFile, const string& outputDir,
                                    bool compress, bool decompress, bool encrypt, bool decrypt,
                                    const char* compAlg) {
    // Extraer nombre base del archivo
    size_t lastSlash = inputFile.find_last_of("/\\");
    string baseName = (lastSlash == string::npos) ? inputFile : inputFile.substr(lastSlash + 1);
    
    // Determinar extensión de salida
    string outputName = baseName;
    
    if (decompress && decrypt) {
        // Quitar extensiones de compresión y encriptación
        if (outputName.size() > 4 && outputName.substr(outputName.size() - 4) == ".enc") {
            outputName = outputName.substr(0, outputName.size() - 4);
        }
        if (outputName.size() > 4 && outputName.substr(outputName.size() - 4) == ".lzw") {
            outputName = outputName.substr(0, outputName.size() - 4);
        } else if (outputName.size() > 5 && outputName.substr(outputName.size() - 5) == ".huff") {
            outputName = outputName.substr(0, outputName.size() - 5);
        }
    } else if (decompress) {
        // Quitar extensión de compresión
        if (outputName.size() > 4 && outputName.substr(outputName.size() - 4) == ".lzw") {
            outputName = outputName.substr(0, outputName.size() - 4);
        } else if (outputName.size() > 5 && outputName.substr(outputName.size() - 5) == ".huff") {
            outputName = outputName.substr(0, outputName.size() - 5);
        }
    } else if (decrypt) {
        // Quitar extensión de encriptación
        if (outputName.size() > 4 && outputName.substr(outputName.size() - 4) == ".enc") {
            outputName = outputName.substr(0, outputName.size() - 4);
        }
    } else if (compress && encrypt) {
        // Agregar extensiones
        if (strcmp(compAlg, "lzw") == 0) {
            outputName += ".lzw.enc";
        } else if (strcmp(compAlg, "huffman") == 0) {
            outputName += ".huff.enc";
        }
    } else if (compress) {
        // Agregar extensión de compresión
        if (strcmp(compAlg, "lzw") == 0) {
            outputName += ".lzw";
        } else if (strcmp(compAlg, "huffman") == 0) {
            outputName += ".huff";
        }
    } else if (encrypt) {
        outputName += ".enc";
    }
    
    // Construir ruta completa de salida
    string outputPath = outputDir;
    if (outputPath.back() != '/' && outputPath.back() != '\\') {
        outputPath += "/";
    }
    outputPath += outputName;
    
    return outputPath;
}

// Función para procesar un archivo individual
static int processFile(const char* inputPath, const char* outputPath,
                      bool compress, bool decompress, bool encrypt, bool decrypt,
                      const char* compAlg, const char* encAlg,
                      const string& key) {
    
    string tempFile1, tempFile2;
    
    // Determinar orden de operaciones según el estándar:
    // Comprimir y encriptar: primero comprimir, luego encriptar
    // Desencriptar y descomprimir: primero desencriptar, luego descomprimir
    
    if (compress && encrypt) {
        // Paso 1: Comprimir
        tempFile1 = string(inputPath);
        if (strcmp(compAlg, "lzw") == 0) {
            tempFile1 += ".lzw";
        } else if (strcmp(compAlg, "huffman") == 0) {
            tempFile1 += ".huff";
        }
        
        if (strcmp(compAlg, "lzw") == 0) {
            filePartitioner partitioner;
            partitioner.compressLZW(inputPath);
        } else if (strcmp(compAlg, "huffman") == 0) {
            Huffman huffman;
            huffman.compress(inputPath);
        } else {
            fprintf(stderr, "Error: Algoritmo de compresión desconocido: %s\n", compAlg);
            return 1;
        }
        
        // Paso 2: Encriptar el archivo comprimido
        if (encryptFile(tempFile1.c_str(), outputPath, key) != 0) {
            fprintf(stderr, "Error al encriptar archivo comprimido\n");
            unlink(tempFile1.c_str());
            return 1;
        }
        
        // Limpiar archivo temporal comprimido
        unlink(tempFile1.c_str());
        return 0;
    }
    
    if (decompress && decrypt) {
        // Paso 1: Desencriptar
        // Crear nombre de archivo temporal único
        tempFile1 = string(inputPath) + ".tmp_decrypted";
        // Si el archivo ya existe, agregar un número
        int counter = 0;
        string tempTest = tempFile1;
        struct stat st;
        while (stat(tempTest.c_str(), &st) == 0) {
            tempTest = tempFile1 + "_" + to_string(counter++);
        }
        tempFile1 = tempTest;
        
        if (decryptFile(inputPath, tempFile1.c_str(), key) != 0) {
            fprintf(stderr, "Error al desencriptar archivo\n");
            unlink(tempFile1.c_str());
            return 1;
        }
        
        // Paso 2: Descomprimir
        if (strcmp(compAlg, "lzw") == 0) {
            filePartitioner partitioner;
            partitioner.decompressLZW(tempFile1.c_str());
            // El archivo descomprimido será tempFile1 sin .lzw
            string decompressed = tempFile1;
            if (decompressed.size() > 4 && decompressed.substr(decompressed.size() - 4) == ".lzw") {
                decompressed = decompressed.substr(0, decompressed.size() - 4);
            } else {
                decompressed += ".out";
            }
            if (rename(decompressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo descomprimido\n");
                unlink(tempFile1.c_str());
                unlink(decompressed.c_str());
                return 1;
            }
        } else if (strcmp(compAlg, "huffman") == 0) {
            Huffman huffman;
            huffman.decompress(tempFile1.c_str());
            string decompressed = tempFile1;
            if (decompressed.size() > 5 && decompressed.substr(decompressed.size() - 5) == ".huff") {
                decompressed = decompressed.substr(0, decompressed.size() - 5);
            } else {
                decompressed += ".out";
            }
            if (rename(decompressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo descomprimido\n");
                unlink(tempFile1.c_str());
                unlink(decompressed.c_str());
                return 1;
            }
        }
        
        // Limpiar archivo temporal
        unlink(tempFile1.c_str());
        return 0;
    }
    
    // Operaciones individuales
    if (compress) {
        if (strcmp(compAlg, "lzw") == 0) {
            filePartitioner partitioner;
            partitioner.compressLZW(inputPath);
            string compressed = string(inputPath) + ".lzw";
            if (compressed != outputPath && rename(compressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo comprimido\n");
                return 1;
            }
        } else if (strcmp(compAlg, "huffman") == 0) {
            Huffman huffman;
            huffman.compress(inputPath);
            string compressed = string(inputPath) + ".huff";
            if (compressed != outputPath && rename(compressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo comprimido\n");
                return 1;
            }
        }
        return 0;
    }
    
    if (decompress) {
        if (strcmp(compAlg, "lzw") == 0) {
            filePartitioner partitioner;
            partitioner.decompressLZW(inputPath);
            string decompressed = inputPath;
            if (decompressed.size() > 4 && decompressed.substr(decompressed.size() - 4) == ".lzw") {
                decompressed = decompressed.substr(0, decompressed.size() - 4);
            } else {
                decompressed += ".out";
            }
            if (decompressed != outputPath && rename(decompressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo descomprimido\n");
                return 1;
            }
        } else if (strcmp(compAlg, "huffman") == 0) {
            Huffman huffman;
            huffman.decompress(inputPath);
            string decompressed = inputPath;
            if (decompressed.size() > 5 && decompressed.substr(decompressed.size() - 5) == ".huff") {
                decompressed = decompressed.substr(0, decompressed.size() - 5);
            } else {
                decompressed += ".out";
            }
            if (decompressed != outputPath && rename(decompressed.c_str(), outputPath) != 0) {
                fprintf(stderr, "Error al renombrar archivo descomprimido\n");
                return 1;
            }
        }
        return 0;
    }
    
    if (encrypt) {
        return encryptFile(inputPath, outputPath, key);
    }
    
    if (decrypt) {
        return decryptFile(inputPath, outputPath, key);
    }
    
    return 1;
}

int main(int argc, char **argv) {
    bool compress = false;
    bool decompress = false;
    bool encrypt = false;
    bool decrypt = false;
    char *input = NULL;
    char *output = NULL;
    char *keyopt = NULL;
    const char *compAlg = "lzw";  // default
    const char *encAlg = "vigenere";   // default
    
    // Opciones largas
    static struct option long_options[] = {
        {"compress", no_argument, 0, 'c'},
        {"decompress", no_argument, 0, 'd'},
        {"encrypt", no_argument, 0, 'e'},
        {"decrypt", no_argument, 0, 'u'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"key", required_argument, 0, 'k'},
        {"comp-alg", required_argument, 0, 0},
        {"enc-alg", required_argument, 0, 0},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "cdeui:o:k:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                compress = true;
                break;
            case 'd':
                decompress = true;
                break;
            case 'e':
                encrypt = true;
                break;
            case 'u':
                decrypt = true;
                break;
            case 'i':
                input = optarg;
                break;
            case 'o':
                output = optarg;
                break;
            case 'k':
                keyopt = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 0:
                // Opciones largas sin short option
                if (strcmp(long_options[option_index].name, "comp-alg") == 0) {
                    compAlg = optarg;
                    if (strcmp(compAlg, "lzw") != 0 && strcmp(compAlg, "huffman") != 0) {
                        fprintf(stderr, "Error: Algoritmo de compresión debe ser 'lzw' o 'huffman'\n");
                        return 1;
                    }
                } else if (strcmp(long_options[option_index].name, "enc-alg") == 0) {
                    encAlg = optarg;
                    if (strcmp(encAlg, "vigenere") != 0) {
                        fprintf(stderr, "Error: Algoritmo de encriptación debe ser 'vigenere'\n");
                        return 1;
                    }
                }
                break;
            case '?':
                print_usage(argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    // Validar que haya al menos una operación
    if (!compress && !decompress && !encrypt && !decrypt) {
        fprintf(stderr, "Error: Debe especificar al menos una operación (-c, -d, -e, -u)\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Validar que no se combinen operaciones incompatibles
    if (compress && decompress) {
        fprintf(stderr, "Error: No se puede comprimir y descomprimir al mismo tiempo\n");
        return 1;
    }
    if (encrypt && decrypt) {
        fprintf(stderr, "Error: No se puede encriptar y desencriptar al mismo tiempo\n");
        return 1;
    }
    
    // Validar entrada y salida
    if (!input || !output) {
        fprintf(stderr, "Error: Debe especificar entrada (-i) y salida (-o)\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Validar clave si es necesario
    string key;
    bool keyNeeded = encrypt || decrypt;
    
    if (keyNeeded) {
        if (!keyopt) {
            fprintf(stderr, "Error: Se requiere clave (-k) para encriptación/desencriptación\n");
            return 1;
        }
        
        key = getKey(keyopt);
        if (key.empty()) {
            fprintf(stderr, "Error: No se pudo leer la clave. Debe ser texto o ruta a archivo válido\n");
            return 1;
        }
    }
    
    // Verificar si la entrada es un directorio
    if (isDirectory(input)) {
        // Procesar directorio
        vector<string> files = getFilesInDirectory(input);
        
        if (files.empty()) {
            fprintf(stderr, "Error: El directorio está vacío o no contiene archivos regulares\n");
            return 1;
        }
        
        // Verificar que la salida también sea un directorio
        struct stat st;
        if (stat(output, &st) != 0) {
            // El directorio no existe, intentar crearlo
            #ifdef _WIN32
            if (mkdir(output) != 0) {
            #else
            if (mkdir(output, 0755) != 0) {
            #endif
                fprintf(stderr, "Error: No se pudo crear el directorio de salida: %s\n", output);
                return 1;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: La salida debe ser un directorio cuando la entrada es un directorio\n");
            return 1;
        }
        
        printf("Procesando %zu archivo(s) en el directorio...\n", files.size());
        
        // Contadores thread-safe
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex; // Para sincronizar prints
        
        // Determinar número de threads
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4; // Fallback
        numThreads = min(numThreads, files.size()); // No más threads que archivos
        
        printf("Usando %zu thread(s) para procesamiento paralelo\n", numThreads);
        
        // Dividir archivos entre threads
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    const string& file = files[i];
                    string outputFile = generateOutputFilename(file, output, compress, decompress, 
                                                              encrypt, decrypt, compAlg);
                    
                    // Print sincronizado
                    {
                        lock_guard<mutex> lock(printMutex);
                        printf("Thread %zu: Procesando %s -> %s\n", 
                               t, file.c_str(), outputFile.c_str());
                    }
                    
                    int result = processFile(file.c_str(), outputFile.c_str(), compress, decompress,
                                            encrypt, decrypt, compAlg, encAlg, 
                                            keyNeeded ? key : string(""));
                    
                    if (result == 0) {
                        successCount++;
                        lock_guard<mutex> lock(printMutex);
                        printf("Thread %zu: ✓ Completado: %s\n", t, file.c_str());
                    } else {
                        failCount++;
                        lock_guard<mutex> lock(printMutex);
                        fprintf(stderr, "Thread %zu: ✗ Error al procesar: %s\n", t, file.c_str());
                    }
                }
            });
        }
        
        // Esperar a que todos los threads terminen
        for (auto& th : threads) {
            th.join();
        }
        
        printf("\nProcesamiento completado: %d exitoso(s), %d fallido(s)\n", 
               successCount.load(), failCount.load());
        
        return (failCount.load() > 0) ? 1 : 0;
    } else {
        // Procesar archivo individual
        int result = processFile(input, output, compress, decompress, encrypt, decrypt,
                                compAlg, encAlg, keyNeeded ? key : string(""));
        
        if (result == 0) {
            printf("Operación completada exitosamente\n");
        }
        
        return result;
    }
}

