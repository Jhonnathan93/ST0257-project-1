import os
import pandas as pd
import csv
from multiprocessing import Process, Manager
from datetime import datetime, timedelta
import time
import argparse
from rich.console import Console
from rich.table import Table

def get_cpu_core(pid):
    
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
    Función creada para buscar archivos con extensión .csv en un directorio
    especificado, esto mediante el uso de la librería OS.

    Args:
        directorio (str): String correspondiente al directorio en el que debemos hacer la busqueda de los archivos.
        extension (str | Opcional): Por defecto la extensión que usamos es para archivos .csv unicamente, en caso de que se pida realizar con archivos .txt o demás formatos de texto se podría cambiar este
        parametro.
    Returns:
        La función retorna una lista de elementos que corresponde a las direcciones de cada archivo cuya extensión coincide con la requerida.
    
    Example: 
        ["fork/JPvideos.csv", "fork/USvideos.csv", ... , "sistemas/MXvideos.csv"]
    """
    archivos_csv = []
    for raiz, _, archivos in os.walk(directorio):
        for archivo in archivos:
            if archivo.endswith(extension):
                archivos_csv.append(os.path.join(raiz, archivo))
    return archivos_csv

def set_cpu_affinity(cpu_id):
    """ Set the CPU affinity for the current process to a specific CPU. """
    pid = os.getpid()
    os.sched_setaffinity(pid, {cpu_id})

def read_csv(file_path):
    """ Function to read a CSV file. """
    
    # pid = os.getpid()
    # cpu_core = get_cpu_core(pid)
    
    # if cpu_core is not None:
    #    print(f"El proceso {pid} se está ejecutando en el núcleo {cpu_core}.")
    """inicio = time.time()
    file = open(file_path, errors='ignore')
    file.read()
    # df = pd.read_csv(file_path, encoding='utf-8', encoding_errors='ignore')
    fin = time.time()
    print(f"El tiempo utilizado para leer el archivo {file_path} fue: {fin - inicio}")"""

    file = open(file_path, errors='ignore')
    file.read()

def measure_time(file_path, durations):
    start_time = time.time()
    read_csv(file_path)
    end_time = time.time()
    duration = end_time - start_time
    durations.append((file_path, start_time, end_time, duration))    

def show_results(durations, total_time, log_messages):
    # Printing summary with Rich library
    console = Console()
    
    for message in log_messages:
        console.print(message)
    
    table = Table(title="Resumen de Tiempos de Carga")

    table.add_column("Archivo", justify="left", style="cyan")
    table.add_column("Hora de inicio", justify="left", style="magenta")
    table.add_column("Hora de fin", justify="left", style="green")
    table.add_column("Duración", justify="right", style="red")

    for file_path, start_time, end_time, duration in durations:
        start_time_str = datetime.fromtimestamp(start_time).strftime('%H:%M:%S')
        end_time_str = datetime.fromtimestamp(end_time).strftime('%H:%M:%S')
        duration_str = f"{duration:.6f} segundos"
        table.add_row(file_path, start_time_str, end_time_str, duration_str)

    console.print(table)
    
    console.print(f"[bold yellow]Tiempo total del programa: {total_time:.8f}  segundos[/bold yellow]")

def main():
    
    parser = argparse.ArgumentParser(description="Options to read files")

    parser.add_argument('-s', '--single', action='store_true', help="Single mode.")
    parser.add_argument('-m', '--multi', action='store_true', help='Multi mode.')
    parser.add_argument('-f', '--folder', type=str, action='store')

    args = parser.parse_args()
    folder = args.folder
    
    if args.single and args.multi:
        print("You can't set two flags at the same time (python dataload.py -s -m -f FOLDER)")
        return 1

    # Verify if FOLDER exists
    if not os.path.isdir(folder):
        print(f"Directory '{folder}' does not exist.")
        return 1
    
    # Get a list of all CSV files in the directory
    csv_files = buscar_archivos_csv(folder)
    
    if not csv_files:
        print(f"No CSV files found in directory '{folder}'.")
        return 1

    pid = os.getpid()
    cpu_core = get_cpu_core(pid)
    #if cpu_core is not None:
        #print(f"El proceso {pid} se está ejecutando en el núcleo {cpu_core}.")

    manager = Manager()
    durations = manager.list()
    log_messages = []

    # Manager to share data between processes
    if args.multi or args.single:
        processes = []
        
        if args.single:
            set_cpu_affinity(cpu_core)
    

        # start_time_program = datetime.now()
        start_time_program = time.time()
        log_messages.append(f"[bold yellow]Hora de inicio del programa: {datetime.fromtimestamp(start_time_program).strftime('%H:%M:%S')}[/bold yellow]")
        # print(f"Program started at {start_time_program.strftime('%H:%M:%S')}")
        # start_time_first_file = datetime.now()
        start_time_first_file = time.time()
        log_messages.append(f"[bold yellow]Hora de inicio de la carga del primer archivo: {datetime.fromtimestamp(start_time_first_file).strftime('%H:%M:%S')}[/bold yellow]")
        # print(f"Files started loading at {start_time_first_file.strftime('%H:%M:%S')}")
        # Create and start a process for each CSV file
        for file_path in csv_files:
            process = Process(target=measure_time, args=(file_path, durations))
            processes.append(process)
            process.start()
        
        # Wait for all processes to complete
        for process in processes:
            process.join()
        
        # end_time_last_file = datetime.now()
        end_time_last_file = time.time()
        print(f"Files finished loading in {end_time_last_file - start_time_first_file} seconds")
        
        # end_time_program = datetime.now()
        end_time_program = time.time()
        total_time = end_time_program - start_time_program
        # total_seconds = int(total_time.total_seconds())
        # minutes, seconds = divmod(total_time, 60)
        # print(f"Program ended in {minutes:02}:{seconds:02} seconds")
    else:
        start_time_program = time.time()
        log_messages.append(f"[bold yellow]Hora de inicio del programa: {datetime.fromtimestamp(start_time_program).strftime('%H:%M:%S')}[/bold yellow]")
        
        start_time_first_file = time.time()
        log_messages.append(f"[bold yellow]Hora de inicio de la carga del primer archivo: {datetime.fromtimestamp(start_time_first_file).strftime('%H:%M:%S')}[/bold yellow]")

        for file in csv_files:
            measure_time(file, durations)
        
        end_time_last_file = time.time()
        log_messages.append(f"[bold yellow]Hora de finalización de la carga del último archivo: {datetime.fromtimestamp(end_time_last_file).strftime('%H:%M:%S')}[/bold yellow]")
        
        end_time_program = time.time()
        total_time = end_time_program - start_time_program

    show_results(durations, total_time, log_messages)
    
    return 0
    

if __name__ == '__main__':
    print('Return',main())