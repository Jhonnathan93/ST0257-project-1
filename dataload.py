import os
import csv
from multiprocessing import Process
import time
import argparse

# Constante que define el tamaño de página en bytes (4 KB)
PAGE_SIZE = 4096

def get_cpu_core(pid):
    """
    Obtiene el núcleo de la CPU en el que se está ejecutando un proceso.
    """
    try:
        with open(f'/proc/{pid}/stat') as f:
            fields = f.read().split()
            cpu_core = fields[38]  # El índice 38 corresponde al núcleo de la CPU
            return int(cpu_core)
    except FileNotFoundError:
        print(f"El proceso con PID {pid} no existe.")
        return None
    except Exception as e:
        print(f"Error al leer el núcleo de la CPU para el proceso {pid}: {e}")
        return None

def buscar_archivos_csv(directorio, extension='.csv'):
    """
    Busca archivos con extensión .csv en un directorio especificado.
    """
    archivos_csv = []
    for raiz, _, archivos in os.walk(directorio):
        for archivo in archivos:
            if archivo.endswith(extension):
                archivos_csv.append(os.path.join(raiz, archivo))
    return archivos_csv

def set_cpu_affinity(cpu_id):
    """
    Establece la afinidad de la CPU para el proceso actual a una CPU específica.
    """
    pid = os.getpid()
    os.sched_setaffinity(pid, {cpu_id})
    
def read_csv(file_path):
    """
    Función para leer un archivo CSV simulando paginación.
    """
    inicio = time.time()
    
    # Abrir el archivo
    with open(file_path, errors='ignore') as file:
        page_number = 0
        while True:
            # Leer un fragmento de PAGE_SIZE (4096 bytes)
            page = file.read(PAGE_SIZE)
            if not page:
                break  # Salir si no queda más contenido por leer

            # Simular almacenamiento en "página"
            print(f"Archivo: {file_path}, Página: {page_number}, Bytes leídos: {len(page)}")
            page_number += 1
    
    fin = time.time()
    print(f"El tiempo utilizado para leer el archivo {file_path} fue: {fin - inicio} segundos")

def main():
    """
    Función principal que controla el flujo del programa.
    """
    parser = argparse.ArgumentParser(description="Simulación de lectura de archivos CSV con paginación.")
    parser.add_argument('-s', '--single', action='store_true', help="Single mode.")
    parser.add_argument('-m', '--multi', action='store_true', help='Multi mode.')
    parser.add_argument('-f', '--folder', type=str, help='Folder to search for CSV files.')

    args = parser.parse_args()
    folder = args.folder if args.folder else os.getcwd()

    if args.single and args.multi:
        print("No se pueden establecer ambas banderas al mismo tiempo (python dataload.py -s -m -f FOLDER)")
        return 1

    if not os.path.isdir(folder):
        print(f"El directorio '{folder}' no existe.")
        return 1

    # Obtener una lista de todos los archivos CSV en el directorio
    csv_files = buscar_archivos_csv(folder)

    if not csv_files:
        print(f"No se encontraron archivos CSV en el directorio '{folder}'.")
        return 1

    pid = os.getpid()
    cpu_core = get_cpu_core(pid)
    if cpu_core is not None:
        print(f"El proceso {pid} se está ejecutando en el núcleo {cpu_core}.")

    processes = []

    if args.single:
        set_cpu_affinity(cpu_core)

    start_time_program = time.time()
    start_time_first_file = time.time()

    for file_path in csv_files:
        process = Process(target=read_csv, args=(file_path,))
        processes.append(process)
        process.start()

    for process in processes:
        process.join()

    end_time_last_file = time.time()
    print(f"Archivos terminados de cargar en {end_time_last_file - start_time_first_file} segundos")

    end_time_program = time.time()
    total_time = end_time_program - start_time_program
    minutes, seconds = divmod(total_time, 60)
    print(f"Programa terminado en {minutes:02}:{seconds:02} segundos")

    return 0

if __name__ == '__main__':
    print('Return', main())
