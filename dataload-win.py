import os
from multiprocessing import Process, Manager
from datetime import datetime
import time
import argparse
import psutil

# Constante que define el tamaño de página en bytes (4 KB)
PAGE_SIZE = 4096


def get_cpu_core(pid):
    return 1


def buscar_archivos_csv(directorio, extension='.csv'):
    """
    Busca archivos con extensión .csv en un directorio especificado.
    """
    archivos_csv = []
    for archivo in os.listdir(directorio):
        if archivo.endswith(extension):
            archivos_csv.append(os.path.join(directorio, archivo))
    return archivos_csv


def set_cpu_affinity(cpu_id):
    """
    Establece la afinidad de la CPU para el proceso actual a una CPU específica.
    """
    p = psutil.Process(os.getpid())
    p.cpu_affinity([get_cpu_core(os.getpid())])


def get_memory_usage(pid):
    """
    Obtiene el uso de memoria de un proceso en KB.
    """
    process = psutil.Process(pid)
    memory_info = process.memory_info()
    return memory_info.rss // 1024  # Retorna la memoria en KB


def read_csv(file_path):
    """
    Función para leer un archivo CSV simulando paginación y almacenar las páginas en una lista.
    """
    pid = os.getpid()
    cpu_core = get_cpu_core(pid)
    memory_usage_start = get_memory_usage(pid)

    start_time = time.time()

    # Lista para almacenar todas las "páginas" del archivo
    pages = []

    # Abrir el archivo
    with open(file_path, 'rb') as file:
        page_number = 0
        while True:
            # Leer un fragmento de PAGE_SIZE (4096 bytes)
            page = file.read(PAGE_SIZE)
            if not page:
                break  # Salir si no queda más contenido por leer

            # Simular almacenamiento en "página"
            pages.append(page)
            page_number += 1

    end_time = time.time()
    memory_usage_end = get_memory_usage(pid)

    elapsed_time = (end_time - start_time) * 1000  # Convertir a milisegundos

    # Formatear la salida en forma de tabla
    print(
        f"{pid:<10} {cpu_core:<5} {os.path.basename(file_path):<20} {page_number:<10} {elapsed_time:<20.6f} {memory_usage_start:<15} {memory_usage_end:<15}"
    )

    return pages


def main():
    """
    Función principal que controla el flujo del programa.
    """
    parser = argparse.ArgumentParser(
        description="Simulación de lectura de archivos CSV con paginación.")
    parser.add_argument('-s',
                        '--single',
                        action='store_true',
                        help="Single mode.")
    parser.add_argument('-m',
                        '--multi',
                        action='store_true',
                        help='Multi mode.')
    parser.add_argument('-f',
                        '--folder',
                        type=str,
                        help='Folder to search for CSV files.')

    args = parser.parse_args()
    folder = args.folder if args.folder else os.getcwd()

    if args.single and args.multi:
        print(
            "Cannot set both flags at the same time (python dataload.py -s -m -f FOLDER)"
        )
        return 1

    if not os.path.isdir(folder):
        print(f"The directory '{folder}' does not exist.")
        return 1

    # Obtener una lista de todos los archivos CSV en el directorio
    csv_files = buscar_archivos_csv(folder)

    if not csv_files:
        print(f"No CSV files were found in the '{folder}' directory.")
        return 1

    pid = os.getpid()
    cpu_core = get_cpu_core(pid)
    if cpu_core is not None:
        print(f"Process {pid} is running on core {cpu_core}.")

    processes = []

    if args.single:
        set_cpu_affinity(cpu_core)

    start_time_program = time.time()
    print(
        f"The program starts at {time.strftime('%H:%M:%S', time.localtime(start_time_program))}"
    )

    print(
        f"{'PID':<10} {'Core':<5} {'File':<20} {'Pages':<10} {'Time (ms)':<20} {'Mem Start (KB)':<15} {'Mem End (KB)':<15}"
    )

    start_time_first_file = time.time()

    for file_path in csv_files:
        process = Process(target=read_csv, args=(file_path, ))
        processes.append(process)
        process.start()

    for process in processes:
        process.join()

    end_time_last_file = time.time()
    print(f"Start time of the first file load: {time.strftime('%H:%M:%S', time.localtime(start_time_first_file))}")
    print(
        f"The program ended at {time.strftime('%H:%M:%S', time.localtime(end_time_last_file))}"
    )

    total_time = end_time_last_file - start_time_program
    minutes, seconds = divmod(total_time, 60)
    print(
        f"The time used to read {len(csv_files)} files is: {minutes:02}:{seconds:06.3f} seconds"
    )

    return 0


if __name__ == '__main__':
    print('Return', main())
