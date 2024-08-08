import os
import time
import argparse
from datetime import datetime
from multiprocessing import Process, Manager
from rich.console import Console
from rich.table import Table

def get_cpu_core(pid):
    """
    Obtiene el núcleo de la CPU en el que se está ejecutando un proceso.

    Args:
        pid (int): ID del proceso del cual se quiere conocer el núcleo de la CPU.

    Returns:
        int: Número del núcleo de la CPU.
        None: Si no se encuentra el proceso o hay un error.
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
    Función creada para buscar archivos con extensión .csv en un directorio
    especificado, esto mediante el uso de la librería OS.

    Args:
        directorio (str): String correspondiente al directorio en el que debemos hacer la búsqueda de los archivos.
        extension (str | Opcional): Por defecto la extensión que usamos es para archivos .csv unicamente, en caso de que se pida realizar con archivos .txt o demás formatos de texto se podría cambiar este
        parametro.
    Returns:
        La función retorna una lista de elementos que corresponde a las direcciones de cada archivo cuya extensión coincide con la requerida.
    
    Example: 
        ["fork/JPvideos.csv", "fork/USvideos.csv", ... , "sistemas/MXvideos.csv"]
    """
    archivos_csv = []
    for archivo in os.listdir(directorio):
        if archivo.endswith(extension):
            archivos_csv.append(os.path.join(directorio, archivo))
    return archivos_csv

def set_cpu_affinity(cpu_id):
    """
    Establece la afinidad de la CPU para el proceso actual a una CPU específica.

    Args:
        cpu_id (int): Número de la CPU a la cual se quiere asignar el proceso.
    """
    pid = os.getpid()
    os.sched_setaffinity(pid, {cpu_id})

def read_csv(file_path):
    """
    Función para leer un archivo CSV.

    Args:
        file_path (str): Ruta del archivo CSV a leer.
    """
    with open(file_path, errors='ignore') as file:
        file.read()

def measure_time(file_path, durations):
    """
    Mide el tiempo que se tarda en leer un archivo CSV y lo guarda en una lista compartida.

    Args:
        file_path (str): Ruta del archivo CSV a leer.
        durations (list): Lista compartida en la cual se almacenarán los tiempos de lectura.
    """
    start_time = time.time()
    read_csv(file_path)
    end_time = time.time()
    duration = end_time - start_time
    durations.append((file_path, start_time, end_time, duration))

def show_results(durations, total_time, log_messages):
    """
    Muestra los resultados usando la librería rich.

    Args:
        durations (list): Lista de duraciones de lectura de archivos CSV.
        total_time (float): Tiempo total que tomó la ejecución del programa.
        log_messages (list): Lista de mensajes de log a mostrar.
    """
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
    """
    Función principal que controla el flujo del programa. Lee argumentos de línea de comandos,
    busca archivos CSV, mide los tiempos de lectura y muestra los resultados.
    """
    parser = argparse.ArgumentParser(description="Options to read files")
    parser.add_argument('-s', '--single', action='store_true', help="Single mode.")
    parser.add_argument('-m', '--multi', action='store_true', help='Multi mode.')
    parser.add_argument('-f', '--folder', type=str, help='Folder to search for CSV files.')
    args = parser.parse_args()
    folder = args.folder if args.folder else os.getcwd()
    
    if args.single and args.multi:
        print("You can't set two flags at the same time (python dataload.py -s -m -f FOLDER)")
        return 1

    if not os.path.isdir(folder):
        print(f"Directory '{folder}' does not exist.")
        return 1
    
    csv_files = buscar_archivos_csv(folder)
    if not csv_files:
        print(f"No CSV files found in directory '{folder}'.")
        return 1

    pid = os.getpid()
    cpu_core = get_cpu_core(pid)
    if cpu_core is not None:
        print(f"Process {pid} is running on CPU core {cpu_core}.")

    manager = Manager()
    durations = manager.list()
    log_messages = []

    start_time_program = time.time()
    log_messages.append(f"[bold yellow]Hora de inicio del programa: {datetime.fromtimestamp(start_time_program).strftime('%H:%M:%S')}[/bold yellow]")

    processes = []
    if args.single:
        set_cpu_affinity(cpu_core)

    start_time_first_file = time.time()
    log_messages.append(f"[bold yellow]Hora de inicio de la carga del primer archivo: {datetime.fromtimestamp(start_time_first_file).strftime('%H:%M:%S')}[/bold yellow]")

    for file_path in csv_files:
        if args.multi or args.single:
            process = Process(target=measure_time, args=(file_path, durations))
            processes.append(process)
            process.start()
        else:
            measure_time(file_path, durations)

    for process in processes:
        process.join()

    end_time_last_file = time.time()
    log_messages.append(f"[bold yellow]Hora de finalización de la carga del último archivo: {datetime.fromtimestamp(end_time_last_file).strftime('%H:%M:%S')}[/bold yellow]")

    end_time_program = time.time()
    total_time = end_time_program - start_time_program

    show_results(durations, total_time, log_messages)
    return 0

if __name__ == '__main__':
    main()

    """
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
    """    