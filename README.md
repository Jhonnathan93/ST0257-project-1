
# **Parallelism in a Controlled Environment**

### **Team Members:**
- Manuela Castaño Franco  
- Juan Diego Lopez Guisao  
- Jhonnathan Stiven Ocampo Díaz  

The project consists of two versions: one in C and another in Python. Both programs read and process CSV files from a directory, measuring execution time and memory usage. Files are divided into blocks (“pages”) to simulate memory management. The program can run on a single CPU core or distribute the load across multiple cores to compare performance. At the end, results are generated showing time and resource usage.

## **Project Structure**
- `main.c`: Main source code for the C version.  
- `dataload.py`: Main source code for the Python version (Unix-based systems).  
- `dataload-win.py`: Main source code for the Python version (Windows systems).  
- `Files`: Folder containing all the CSV files to be read by the project.  

# **Compilation (C)**

To compile the C program, use the following command:

```bash
gcc -o dataload main.c
```

This will generate an executable named `dataload`.

## **Execution**

### **C Version**

The C program can be executed in different modes depending on the option passed:

- **Sequential Mode (default):**
```bash
./dataload -f <directory>
```

- **Single-core Mode (-s):**
```bash
./dataload -s -f <directory>
```

- **Multi-core Mode (-m):**
```bash
./dataload -m -f <directory>
```

### **Python Version**

The Python program can also be executed in different modes:

- **Sequential Mode (default):**
```bash
python dataload.py -f <directory>
```

- **Single-core Mode (-s):**
```bash
python dataload.py -s -f <directory>
```

- **Multi-core Mode (-m):**
```bash
python dataload.py -m -f <directory>
```

*If running on Windows, replace `dataload.py` with `dataload-win.py`.*

## **Parameters**
- `-f <directory>`: Specifies the directory where the CSV files are located. If not specified, the current directory will be used.

