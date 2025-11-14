#ifndef TASKPARTITIONER_H
#define TASKPARTITIONER_H

#include <iostream>
#include <vector>
#include <pthread.h>
//#include <functional>

struct ThreadWorkload {
    int thread_id; //el id del hilo, pues sí
    long long start_byte_offset; //donde el hilo tiene que empezar a leer
    long long end_byte_offset; //lo mismo pero para dejar de leer
    long long num_units; //los bloques que procesa el hilo

    //Según parece hay que meter punteros a los datos (input  output buffer, llave (key))
};

class TaskPartitioner {
    public:
        using WorkerFunction = void* (*)(void*); //papi qué putas???
        TaskPartitioner(long long totalBytes, int unitSize, int threads, WorkerFunction func);
        bool execute();
    
    private:
        long long total_data_bytes;
        int unit_size_bytes;
        int num_threads;
        WorkerFunction worker_func;
}

#endif //TASKPARTITIONER_H