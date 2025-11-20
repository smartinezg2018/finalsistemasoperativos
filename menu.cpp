#include "menu.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/types.h>
#endif
#include "filePartitioner.h"
#include "huffman.h"
#include "fileEncrypting.h"

using namespace std;

// Función auxiliar para leer la clave desde archivo
static string read_key_from_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return "";
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return "";
    }
    
    size_t fileSize = st.st_size;
    if (fileSize == 0) {
        close(fd);
        return "";
    }
    
    char* buffer = new char[fileSize + 1];
    ssize_t r = read(fd, buffer, fileSize);
    close(fd);
    
    if (r <= 0) {
        delete[] buffer;
        return "";
    }
    
    buffer[r] = '\0';
    string key(buffer, r);
    delete[] buffer;
    return key;
}

// Función auxiliar para verificar si una ruta es un directorio
static bool isDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

// Función auxiliar para obtener todos los archivos de un directorio
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

// Función auxiliar para generar nombre de archivo de salida
static string generateOutputFilename(const string& inputFile, const string& outputDir,
                                    const string& operation, const string& compAlg) {
    // Extraer nombre base del archivo
    size_t lastSlash = inputFile.find_last_of("/\\");
    string baseName = (lastSlash == string::npos) ? inputFile : inputFile.substr(lastSlash + 1);
    
    string outputName = baseName;
    
    // Determinar extensión según operación
    if (operation == "compress") {
        if (compAlg == "lzw") {
            outputName += ".lzw";
        } else if (compAlg == "huffman") {
            outputName += ".huff";
        }
    } else if (operation == "encrypt") {
        outputName += ".enc";
    } else if (operation == "encrypt_compress") {
        if (compAlg == "lzw") {
            outputName += ".enc.lzw";
        } else if (compAlg == "huffman") {
            outputName += ".enc.huff";
        }
    } else if (operation == "decompress") {
        if (baseName.size() > 4 && baseName.substr(baseName.size() - 4) == ".lzw") {
            outputName = baseName.substr(0, baseName.size() - 4);
        } else if (baseName.size() > 5 && baseName.substr(baseName.size() - 5) == ".huff") {
            outputName = baseName.substr(0, baseName.size() - 5);
        }
    } else if (operation == "decrypt") {
        if (baseName.size() > 4 && baseName.substr(baseName.size() - 4) == ".enc") {
            outputName = baseName.substr(0, baseName.size() - 4);
        }
    } else if (operation == "decrypt_decompress") {
        // Quitar extensiones
        if (baseName.size() > 4 && baseName.substr(baseName.size() - 4) == ".enc") {
            outputName = baseName.substr(0, baseName.size() - 4);
        }
        if (outputName.size() > 4 && outputName.substr(outputName.size() - 4) == ".lzw") {
            outputName = outputName.substr(0, outputName.size() - 4);
        } else if (outputName.size() > 5 && outputName.substr(outputName.size() - 5) == ".huff") {
            outputName = outputName.substr(0, outputName.size() - 5);
        }
    }
    
    // Construir ruta completa de salida
    string outputPath = outputDir;
    if (outputPath.back() != '/' && outputPath.back() != '\\') {
        outputPath += "/";
    }
    outputPath += outputName;
    
    return outputPath;
}

// Función para obtener la clave Vigenère
string getVigenereKey() {
    string keyInput;
    cout << "\nIngrese la clave (texto o ruta a archivo con la clave): ";
    cin.ignore();
    getline(cin, keyInput);
    
    if (keyInput.empty()) {
        cout << "Error: La clave no puede estar vacia." << endl;
        return "";
    }
    
    // Intentar leer como archivo primero
    // Verificar si el archivo existe antes de intentar leerlo
    struct stat st;
    if (stat(keyInput.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        // Es un archivo, intentar leerlo
        string key = read_key_from_file(keyInput.c_str());
        if (!key.empty()) {
            return key;
        }
        // Si no se pudo leer el archivo, usar el nombre del archivo como clave
        return keyInput;
    }
    
    // Si no es archivo, usar el texto directamente como clave
    return keyInput;
}

// Función para procesar encriptación (archivo o directorio)
void processEncrypt(const string& inputPath, const string& outputPath, const string& key) {
    if (isDirectory(inputPath.c_str())) {
        // Procesar directorio con concurrencia
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        // Crear directorio de salida si no existe
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "encrypt", "");
                    int result = encryptFile(files[i].c_str(), outputFile.c_str(), key);
                    
                    lock_guard<mutex> lock(printMutex);
                    if (result == 0) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al encriptar: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        // Procesar archivo individual
        if (encryptFile(inputPath.c_str(), outputPath.c_str(), key) == 0) {
            cout << "\n[OK] Archivo encriptado exitosamente: " << outputPath << endl;
        } else {
            cout << "\n[ERROR] Error al encriptar el archivo." << endl;
        }
    }
}

// Función para encriptar un archivo
void encryptFileMenu() {
    string inputPath, outputPath;
    cout << "\n=== ENCRIPTAR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    string key = getVigenereKey();
    if (key.empty()) {
        return;
    }
    
    processEncrypt(inputPath, outputPath, key);
}

// Función para procesar desencriptación (archivo o directorio)
void processDecrypt(const string& inputPath, const string& outputPath, const string& key) {
    if (isDirectory(inputPath.c_str())) {
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "decrypt", "");
                    int result = decryptFile(files[i].c_str(), outputFile.c_str(), key);
                    
                    lock_guard<mutex> lock(printMutex);
                    if (result == 0) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al desencriptar: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        if (decryptFile(inputPath.c_str(), outputPath.c_str(), key) == 0) {
            cout << "\n[OK] Archivo desencriptado exitosamente: " << outputPath << endl;
        } else {
            cout << "\n[ERROR] Error al desencriptar el archivo." << endl;
        }
    }
}

// Función para desencriptar un archivo
void decryptFileMenu() {
    string inputPath, outputPath;
    cout << "\n=== DESENCRIPTAR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    string key = getVigenereKey();
    if (key.empty()) {
        return;
    }
    
    processDecrypt(inputPath, outputPath, key);
}

// Función para procesar compresión (archivo o directorio)
void processCompress(const string& inputPath, const string& outputPath, const string& compAlg) {
    if (isDirectory(inputPath.c_str())) {
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t, compAlg]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "compress", compAlg);
                    bool success = false;
                    
                    if (compAlg == "lzw") {
                        filePartitioner partitioner;
                        partitioner.compressLZW(files[i]);
                        string compressed = files[i] + ".lzw";
                        if (compressed != outputFile) {
                            if (rename(compressed.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                        } else {
                            success = true;
                        }
                    } else if (compAlg == "huffman") {
                        Huffman huffman;
                        huffman.compress(files[i]);
                        string compressed = files[i] + ".huff";
                        if (compressed != outputFile) {
                            if (rename(compressed.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                        } else {
                            success = true;
                        }
                    }
                    
                    lock_guard<mutex> lock(printMutex);
                    if (success) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al comprimir: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        // Procesar archivo individual
        string outputFile = outputPath;
        if (compAlg == "lzw") {
            filePartitioner partitioner;
            partitioner.compressLZW(inputPath);
            string compressed = inputPath + ".lzw";
            if (compressed != outputFile) {
                if (rename(compressed.c_str(), outputFile.c_str()) != 0) {
                    cout << "\n[ERROR] Error al renombrar archivo comprimido." << endl;
                    return;
                }
            }
            cout << "\n[OK] Archivo comprimido con LZW: " << outputFile << endl;
        } else if (compAlg == "huffman") {
            Huffman huffman;
            huffman.compress(inputPath);
            string compressed = inputPath + ".huff";
            if (compressed != outputFile) {
                if (rename(compressed.c_str(), outputFile.c_str()) != 0) {
                    cout << "\n[ERROR] Error al renombrar archivo comprimido." << endl;
                    return;
                }
            }
            cout << "\n[OK] Archivo comprimido con Huffman: " << outputFile << endl;
        }
    }
}

// Función para comprimir un archivo
void compressFile() {
    string inputPath, outputPath;
    int algorithm;
    string compAlg;
    
    cout << "\n=== COMPRIMIR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Seleccione el algoritmo de compresion:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opcion: ";
    cin >> algorithm;
    
    if (algorithm == 1) {
        compAlg = "lzw";
    } else if (algorithm == 2) {
        compAlg = "huffman";
    } else {
        cout << "\n[ERROR] Opcion invalida." << endl;
        return;
    }
    
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    processCompress(inputPath, outputPath, compAlg);
}

// Función para procesar descompresión (archivo o directorio)
void processDecompress(const string& inputPath, const string& outputPath, const string& compAlg) {
    if (isDirectory(inputPath.c_str())) {
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t, compAlg]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "decompress", compAlg);
                    bool success = false;
                    
                    if (compAlg == "lzw") {
                        filePartitioner partitioner;
                        partitioner.decompressLZW(files[i]);
                        string decompressed = files[i];
                        if (decompressed.size() > 4 && decompressed.substr(decompressed.size() - 4) == ".lzw") {
                            decompressed = decompressed.substr(0, decompressed.size() - 4);
                        } else {
                            decompressed += ".out";
                        }
                        if (decompressed != outputFile) {
                            if (rename(decompressed.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                        } else {
                            success = true;
                        }
                    } else if (compAlg == "huffman") {
                        Huffman huffman;
                        huffman.decompress(files[i]);
                        string decompressed = files[i];
                        if (decompressed.size() > 5 && decompressed.substr(decompressed.size() - 5) == ".huff") {
                            decompressed = decompressed.substr(0, decompressed.size() - 5);
                        } else {
                            decompressed += ".out";
                        }
                        if (decompressed != outputFile) {
                            if (rename(decompressed.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                        } else {
                            success = true;
                        }
                    }
                    
                    lock_guard<mutex> lock(printMutex);
                    if (success) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al descomprimir: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        // Procesar archivo individual
        string outputFile = outputPath;
        if (compAlg == "lzw") {
            filePartitioner partitioner;
            partitioner.decompressLZW(inputPath);
            string decompressed = inputPath;
            if (decompressed.size() > 4 && decompressed.substr(decompressed.size() - 4) == ".lzw") {
                decompressed = decompressed.substr(0, decompressed.size() - 4);
            } else {
                decompressed += ".out";
            }
            if (decompressed != outputFile) {
                if (rename(decompressed.c_str(), outputFile.c_str()) != 0) {
                    cout << "\n[ERROR] Error al renombrar archivo descomprimido." << endl;
                    return;
                }
            }
            cout << "\n[OK] Archivo descomprimido con LZW: " << outputFile << endl;
        } else if (compAlg == "huffman") {
            Huffman huffman;
            huffman.decompress(inputPath);
            string decompressed = inputPath;
            if (decompressed.size() > 5 && decompressed.substr(decompressed.size() - 5) == ".huff") {
                decompressed = decompressed.substr(0, decompressed.size() - 5);
            } else {
                decompressed += ".out";
            }
            if (decompressed != outputFile) {
                if (rename(decompressed.c_str(), outputFile.c_str()) != 0) {
                    cout << "\n[ERROR] Error al renombrar archivo descomprimido." << endl;
                    return;
                }
            }
            cout << "\n[OK] Archivo descomprimido con Huffman: " << outputFile << endl;
        }
    }
}

// Función para descomprimir un archivo
void decompressFile() {
    string inputPath, outputPath;
    int algorithm;
    string compAlg;
    
    cout << "\n=== DESCOMPRIMIR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Seleccione el algoritmo de compresion:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opcion: ";
    cin >> algorithm;
    
    if (algorithm == 1) {
        compAlg = "lzw";
    } else if (algorithm == 2) {
        compAlg = "huffman";
    } else {
        cout << "\n[ERROR] Opcion invalida." << endl;
        return;
    }
    
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    processDecompress(inputPath, outputPath, compAlg);
}

// Función para procesar encriptar y comprimir (archivo o directorio)
void processEncryptAndCompress(const string& inputPath, const string& outputPath, const string& key, const string& compAlg) {
    if (isDirectory(inputPath.c_str())) {
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t, compAlg, key]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "encrypt_compress", compAlg);
                    bool success = false;
                    
                    // Paso 1: Encriptar
                    string tempEnc = files[i] + ".tmp_enc";
                    if (encryptFile(files[i].c_str(), tempEnc.c_str(), key) == 0) {
                        // Paso 2: Comprimir
                        if (compAlg == "lzw") {
                            filePartitioner partitioner;
                            partitioner.compressLZW(tempEnc);
                            string tempComp = tempEnc + ".lzw";
                            if (rename(tempComp.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                            unlink(tempEnc.c_str());
                        } else if (compAlg == "huffman") {
                            Huffman huffman;
                            huffman.compress(tempEnc);
                            string tempComp = tempEnc + ".huff";
                            if (rename(tempComp.c_str(), outputFile.c_str()) == 0) {
                                success = true;
                            }
                            unlink(tempEnc.c_str());
                        }
                    }
                    
                    lock_guard<mutex> lock(printMutex);
                    if (success) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al procesar: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        // Procesar archivo individual
        string outputFile = outputPath;
        string tempEnc = inputPath + ".tmp_enc";
        
        cout << "\nEncriptando archivo..." << endl;
        if (encryptFile(inputPath.c_str(), tempEnc.c_str(), key) != 0) {
            cout << "\n[ERROR] Error al encriptar el archivo." << endl;
            return;
        }
        cout << "[OK] Archivo encriptado" << endl;
        
        cout << "\nComprimiendo archivo encriptado..." << endl;
        if (compAlg == "lzw") {
            filePartitioner partitioner;
            partitioner.compressLZW(tempEnc);
            string tempComp = tempEnc + ".lzw";
            if (rename(tempComp.c_str(), outputFile.c_str()) != 0) {
                cout << "\n[ERROR] Error al renombrar archivo final." << endl;
                unlink(tempEnc.c_str());
                return;
            }
            unlink(tempEnc.c_str());
            cout << "[OK] Archivo comprimido con LZW: " << outputFile << endl;
        } else if (compAlg == "huffman") {
            Huffman huffman;
            huffman.compress(tempEnc);
            string tempComp = tempEnc + ".huff";
            if (rename(tempComp.c_str(), outputFile.c_str()) != 0) {
                cout << "\n[ERROR] Error al renombrar archivo final." << endl;
                unlink(tempEnc.c_str());
                return;
            }
            unlink(tempEnc.c_str());
            cout << "[OK] Archivo comprimido con Huffman: " << outputFile << endl;
        }
        
        cout << "\n[OK] Proceso completado: " << outputFile << endl;
    }
}

// Función para encriptar y comprimir
void encryptAndCompress() {
    string inputPath, outputPath;
    int algorithm;
    string compAlg;
    
    cout << "\n=== ENCRIPTAR Y COMPRIMIR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Seleccione el algoritmo de compresion:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opcion: ";
    cin >> algorithm;
    
    if (algorithm == 1) {
        compAlg = "lzw";
    } else if (algorithm == 2) {
        compAlg = "huffman";
    } else {
        cout << "\n[ERROR] Opcion invalida." << endl;
        return;
    }
    
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    string key = getVigenereKey();
    if (key.empty()) {
        return;
    }
    
    processEncryptAndCompress(inputPath, outputPath, key, compAlg);
}

// Función para procesar desencriptar y descomprimir (archivo o directorio)
void processDecryptAndDecompress(const string& inputPath, const string& outputPath, const string& key, const string& compAlg) {
    if (isDirectory(inputPath.c_str())) {
        vector<string> files = getFilesInDirectory(inputPath.c_str());
        if (files.empty()) {
            cout << "[ERROR] El directorio esta vacio o no contiene archivos regulares." << endl;
            return;
        }
        
        struct stat st;
        if (stat(outputPath.c_str(), &st) != 0) {
            #ifdef _WIN32
            if (mkdir(outputPath.c_str()) != 0) {
            #else
            if (mkdir(outputPath.c_str(), 0755) != 0) {
            #endif
                cout << "[ERROR] No se pudo crear el directorio de salida." << endl;
                return;
            }
        }
        
        cout << "Procesando " << files.size() << " archivo(s) en paralelo..." << endl;
        
        atomic<int> successCount(0);
        atomic<int> failCount(0);
        mutex printMutex;
        
        size_t numThreads = thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        numThreads = min(numThreads, files.size());
        
        size_t filesPerThread = (files.size() + numThreads - 1) / numThreads;
        vector<thread> threads;
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t, compAlg, key]() {
                size_t start = t * filesPerThread;
                size_t end = min(start + filesPerThread, files.size());
                
                for (size_t i = start; i < end; i++) {
                    string outputFile = generateOutputFilename(files[i], outputPath, "decrypt_decompress", compAlg);
                    bool success = false;
                    
                    // Paso 1: Descomprimir
                    string tempDecomp;
                    if (compAlg == "lzw") {
                        filePartitioner partitioner;
                        partitioner.decompressLZW(files[i]);
                        tempDecomp = files[i];
                        if (tempDecomp.size() > 4 && tempDecomp.substr(tempDecomp.size() - 4) == ".lzw") {
                            tempDecomp = tempDecomp.substr(0, tempDecomp.size() - 4);
                        } else {
                            tempDecomp += ".out";
                        }
                    } else if (compAlg == "huffman") {
                        Huffman huffman;
                        huffman.decompress(files[i]);
                        tempDecomp = files[i];
                        if (tempDecomp.size() > 5 && tempDecomp.substr(tempDecomp.size() - 5) == ".huff") {
                            tempDecomp = tempDecomp.substr(0, tempDecomp.size() - 5);
                        } else {
                            tempDecomp += ".out";
                        }
                    }
                    
                    // Paso 2: Desencriptar
                    if (decryptFile(tempDecomp.c_str(), outputFile.c_str(), key) == 0) {
                        success = true;
                        // Limpiar archivo temporal si es diferente al de salida
                        if (tempDecomp != outputFile) {
                            unlink(tempDecomp.c_str());
                        }
                    }
                    
                    lock_guard<mutex> lock(printMutex);
                    if (success) {
                        successCount++;
                        cout << "[OK] " << files[i] << " -> " << outputFile << endl;
                    } else {
                        failCount++;
                        cout << "[ERROR] Error al procesar: " << files[i] << endl;
                    }
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        cout << "\nProcesamiento completado: " << successCount.load() << " exitoso(s), " 
             << failCount.load() << " fallido(s)" << endl;
    } else {
        // Procesar archivo individual
        string outputFile = outputPath;
        
        // Paso 1: Descomprimir
        cout << "\nDescomprimiendo archivo..." << endl;
        string tempDecomp;
        if (compAlg == "lzw") {
            filePartitioner partitioner;
            partitioner.decompressLZW(inputPath);
            tempDecomp = inputPath;
            if (tempDecomp.size() > 4 && tempDecomp.substr(tempDecomp.size() - 4) == ".lzw") {
                tempDecomp = tempDecomp.substr(0, tempDecomp.size() - 4);
            } else {
                tempDecomp += ".out";
            }
        } else if (compAlg == "huffman") {
            Huffman huffman;
            huffman.decompress(inputPath);
            tempDecomp = inputPath;
            if (tempDecomp.size() > 5 && tempDecomp.substr(tempDecomp.size() - 5) == ".huff") {
                tempDecomp = tempDecomp.substr(0, tempDecomp.size() - 5);
            } else {
                tempDecomp += ".out";
            }
        } else {
            cout << "\n[ERROR] Opcion de compresion invalida." << endl;
            return;
        }
        
        cout << "[OK] Archivo descomprimido: " << tempDecomp << endl;
        
        // Paso 2: Desencriptar
        string key = getVigenereKey();
        if (key.empty()) {
            return;
        }
        
        cout << "\nDesencriptando archivo..." << endl;
        if (decryptFile(tempDecomp.c_str(), outputFile.c_str(), key) == 0) {
            if (tempDecomp != outputFile) {
                unlink(tempDecomp.c_str());
            }
            cout << "[OK] Archivo desencriptado: " << outputFile << endl;
            cout << "\n[OK] Proceso completado: " << outputFile << endl;
        } else {
            cout << "\n[ERROR] Error al desencriptar el archivo." << endl;
        }
    }
}

// Función para desencriptar y descomprimir
void decryptAndDecompress() {
    string inputPath, outputPath;
    int algorithm;
    string compAlg;
    
    cout << "\n=== DESENCRIPTAR Y DESCOMPRIMIR ARCHIVO O DIRECTORIO ===" << endl;
    cout << "Seleccione el algoritmo de compresion:" << endl;
    cout << "1. LZW" << endl;
    cout << "2. Huffman" << endl;
    cout << "Opcion: ";
    cin >> algorithm;
    
    if (algorithm == 1) {
        compAlg = "lzw";
    } else if (algorithm == 2) {
        compAlg = "huffman";
    } else {
        cout << "\n[ERROR] Opcion invalida." << endl;
        return;
    }
    
    cout << "Ingrese la ruta de entrada (archivo o directorio): ";
    cin.ignore();
    getline(cin, inputPath);
    cout << "Ingrese la ruta de salida (archivo o directorio): ";
    getline(cin, outputPath);
    
    string key = getVigenereKey();
    if (key.empty()) {
        return;
    }
    
    processDecryptAndDecompress(inputPath, outputPath, key, compAlg);
}

// Función principal del menú
void showMenu() {
    int option;
    
    while (true) {
        cout << "\n";
        cout << "+========================================+" << endl;
        cout << "|     MENU PRINCIPAL - SISTEMA DE       |" << endl;
        cout << "|   ENCRIPTACION Y COMPRESION            |" << endl;
        cout << "+========================================+" << endl;
        cout << "|  1. Encriptar                          |" << endl;
        cout << "|  2. Desencriptar                       |" << endl;
        cout << "|  3. Comprimir                          |" << endl;
        cout << "|  4. Descomprimir                       |" << endl;
        cout << "|  5. Encriptar y comprimir              |" << endl;
        cout << "|  6. Desencriptar y descomprimir        |" << endl;
        cout << "|  0. Salir                              |" << endl;
        cout << "+========================================+" << endl;
        cout << "\nSeleccione una opcion: ";
        
        cin >> option;
        
        switch (option) {
            case 1:
                encryptFileMenu();
                break;
            case 2:
                decryptFileMenu();
                break;
            case 3:
                compressFile();
                break;
            case 4:
                decompressFile();
                break;
            case 5:
                encryptAndCompress();
                break;
            case 6:
                decryptAndDecompress();
                break;
            case 0:
                cout << "\n¡Hasta luego!" << endl;
                return;
            default:
                cout << "\n[ERROR] Opcion invalida. Por favor, seleccione una opcion del 0 al 6." << endl;
                break;
        }
        
        cout << "\nPresione Enter para continuar...";
        cin.ignore();
        cin.get();
    }
}

