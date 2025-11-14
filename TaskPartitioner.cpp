#include <iostream>
#include "TaskPartitioner.h"
#include <pthread.h>

using namespace std;

TaskPartitioner ::TaskPartitioner(long long totalBytes, int unitSize, int threads, WorkerFunction func) : total_data_bytes(totalBytes), unit_size_bytes(unitSize), num_threads(threads), worker_func(func)
{
    if(unitSize <= 0 || threads <= 0) {
        //Lanzar error
    }
}

bool TaskPartitioner :: execute() {
    if(total_data_bytes <= 0) {
        //Lanzar error
        return true;
    }

    //Cálculo de cómo se quiere que se distribuya la carga
    long long total_units = total_data_bytes / unit_size_bytes;
    long long unit_per_thread = total_units / num_threads;
    long long remaining_units = total_units % num_threads;

    cout << "Iniciando paralelización: " <<total_units<< " unidades a procesar en " << num_threads<< " hilos.\n";

    //Contenedores gesstiona hilos y argmentos
    vector<pthread_t> threads(num_threads);
    vector<ThreadWorkload> workloads(num_threads);
    long long current_byte_offset = 0;

    for (int i = 0; i < num_threads; i++)
    {
        long long current_units = unit_per_thread;

        //Si sobran los asigna al último hilo
        if(i == num_threads - 1) {
            current_units += remaining_units;
        }

        long long current_byte_count = current_units * unit_size_bytes;

        workloads[i].thread_id = i + 1;
        workloads[i].start_byte_offset = current_byte_offset;
        workloads[i].end_byte_offset = current_byte_offset + current_byte_count;
        workloads[i].num_units = current_units;

        //Asignar los punteros a los otros datos como input_buffer, key, etc.

        int result = pthread_create(&threads[i], nullptr, worker_func, &workloads[i]);

        if(result != 0){
            //tirar error e intentar cancelar o limpiar los hilos creados
            return false;
        }

        //Mover el offset de inicio para el siguiente hilo
        current_byte_offset += current_byte_count;
    }

    cout << "Esperando que todos los hilos terminen... \n";
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], nullptr);
    }
    
    cout << "Fin de la paralelización. \n";

    if (current_byte_offset != total_data_bytes) {
        //Lanzar error o advertir
        cerr << "El tamaño procesado no coincide con el total de datos. \n";
    }
    
    return true;
}