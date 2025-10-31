// main.cpp
// GSEA - Gestión Segura y Eficiente de Archivos
// Versión: Compresión Huffman con soporte para directorios y concurrencia
// Autor: A. Figueroa
// Fecha: Octubre 2025

#include <iostream>
#include <getopt.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include "huffman.hpp"

using namespace std;

// Estructura para pasar datos a cada hilo
struct ThreadTask {
    string inputPath;
    string outputPath;
    bool compressMode;
};

// Muestra ayuda de uso
void printUsage() {
    cout << "Uso: gsea [opciones]\n"
         << "Opciones:\n"
         << "  -c, --compress                Comprimir archivo o directorio\n"
         << "  -d, --decompress              Descomprimir archivo o directorio\n"
         << "  -i, --input <ruta>            Archivo o directorio de entrada\n"
         << "  -o, --output <ruta>           Archivo o directorio de salida\n"
         << "  --comp-alg <algoritmo>        Algoritmo de compresión (solo 'huffman' por ahora)\n"
         << "  -h, --help                    Mostrar esta ayuda\n";
}

// Comprueba si la ruta corresponde a un directorio
bool isDirectory(const string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

// Crea un directorio de salida si no existe
void ensureDirectory(const string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
}

// Función que ejecuta cada hilo (comprimir o descomprimir)
void* processFileThread(void* arg) {
    ThreadTask* task = (ThreadTask*)arg;

    if (task->compressMode) {
        string output = task->outputPath + ".huf";
        cout << "Comprimiendo (hilo): " << task->inputPath << " -> " << output << endl;
        if (!compressFile(task->inputPath.c_str(), output.c_str())) {
            cerr << "Error al comprimir: " << task->inputPath << endl;
        }
    } else {
        if (task->inputPath.size() > 4 &&
            task->inputPath.substr(task->inputPath.size() - 4) == ".huf") {
            string output = task->outputPath.substr(0, task->outputPath.size() - 4) + "_out";
            cout << "Descomprimiendo (hilo): " << task->inputPath << " -> " << output << endl;
            if (!decompressFile(task->inputPath.c_str(), output.c_str())) {
                cerr << "Error al descomprimir: " << task->inputPath << endl;
            }
        } else {
            cerr << "Archivo omitido (no es .huf): " << task->inputPath << endl;
        }
    }

    delete task;
    pthread_exit(nullptr);
    return nullptr;
}

// Función recursiva para procesar directorios con hilos
void processDirectoryConcurrent(
    const string& inputDir,
    const string& outputDir,
    bool compress_mode)
{
    DIR* dir = opendir(inputDir.c_str());
    if (!dir) {
        perror(("Error abriendo directorio: " + inputDir).c_str());
        return;
    }

    ensureDirectory(outputDir);

    vector<pthread_t> threads;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;

        if (name == "." || name == "..")
            continue;

        string inputPath = inputDir + "/" + name;
        string outputPath = outputDir + "/" + name;

        struct stat st;
        if (stat(inputPath.c_str(), &st) != 0) {
            perror(("Error al acceder a: " + inputPath).c_str());
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            processDirectoryConcurrent(inputPath, outputPath, compress_mode);
        } else if (S_ISREG(st.st_mode)) {
            ThreadTask* task = new ThreadTask{inputPath, outputPath, compress_mode};
            pthread_t tid;

            int ret = pthread_create(&tid, nullptr, processFileThread, task);
            if (ret != 0) {
                cerr << "Error creando hilo para: " << inputPath << endl;
                delete task;
            } else {
                threads.push_back(tid);
            }
        }
    }

    for (pthread_t tid : threads) {
        pthread_join(tid, nullptr);
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    bool compress_flag = false;
    bool decompress_flag = false;
    string input_path;
    string output_path;
    string comp_alg;

    static struct option long_options[] = {
        {"compress", no_argument, 0, 'c'},
        {"decompress", no_argument, 0, 'd'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"comp-alg", required_argument, 0, 0},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;

    while ((opt = getopt_long(argc, argv, "cdi:o:h", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'c': compress_flag = true; break;
            case 'd': decompress_flag = true; break;
            case 'i': input_path = optarg; break;
            case 'o': output_path = optarg; break;
            case 'h': printUsage(); return 0;
            case 0:
                if (strcmp(long_options[long_index].name, "comp-alg") == 0)
                    comp_alg = optarg;
                break;
            default:
                printUsage();
                return 1;
        }
    }

    // Validaciones básicas
    if (compress_flag && decompress_flag) {
        cerr << "Error: No puede usar -c y -d simultáneamente.\n";
        return 1;
    }
    if (!compress_flag && !decompress_flag) {
        cerr << "Error: Debe usar -c (comprimir) o -d (descomprimir).\n";
        printUsage();
        return 1;
    }
    if (input_path.empty() || output_path.empty()) {
        cerr << "Error: Faltan -i (entrada) o -o (salida).\n";
        printUsage();
        return 1;
    }
    if (comp_alg.empty()) {
        cout << "Usando 'huffman' por defecto.\n";
        comp_alg = "huffman";
    }
    if (comp_alg != "huffman") {
        cerr << "Error: Solo 'huffman' está implementado por ahora.\n";
        return 1;
    }

    // Ejecución principal
    if (isDirectory(input_path)) {
        cout << "Procesando directorio concurrente: " << input_path << endl;
        processDirectoryConcurrent(input_path, output_path, compress_flag);
    } else {
        cout << "Procesando archivo: " << input_path << endl;
        bool ok = false;
        if (compress_flag) {
            ok = compressFile(input_path.c_str(), output_path.c_str());
        } else {
            ok = decompressFile(input_path.c_str(), output_path.c_str());
        }
        if (!ok) {
            cerr << "Error en la operación.\n";
            return 1;
        }
    }

    cout << "Operación completada correctamente.\n";
    return 0;
}
