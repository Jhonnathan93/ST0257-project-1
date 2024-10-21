<h1 class="code-line" data-line-start=0 data-line-end=1 ><a id="Paralelismo_en_un_ambiente_controlado_0"></a>Paralelismo en un ambiente controlado</h1>
<h3 class="code-line" data-line-start=1 data-line-end=2 ><a id="Integrantes_del_equipo_1"></a>Integrantes del equipo:</h3>
<ul>
<li class="has-line-data" data-line-start="2" data-line-end="3">Manuela Castaño Franco</li>
<li class="has-line-data" data-line-start="3" data-line-end="4">Juan Diego Lopez Guisao</li>
<li class="has-line-data" data-line-start="4" data-line-end="6">Jhonnathan Stiven Ocampo Díaz</li>
</ul>
<p class="has-line-data" data-line-start="6" data-line-end="7">El proyecto está compuesto por dos versiones: una en C y otra en Python. Ambos programas leen procesan archivos CSV de un directorio, midiendo el tiempo de ejecución y el uso de memoria. Los archivos se dividen en bloques (“páginas”) para simular la gestión de memoria. Puede ejecutarse en un solo núcleo de CPU o distribuir la carga entre varios núcleos para comparar el rendimiento. Al finalizar, se generan resultados con tiempos y uso de recursos.</p>
<h2 class="code-line" data-line-start=8 data-line-end=9 ><a id="Estructura_del_proyecto_8"></a>Estructura del proyecto</h2>
<ul>
<li class="has-line-data" data-line-start="9" data-line-end="10">main.c: Código fuente principal para la versión en C.</li>
<li class="has-line-data" data-line-start="10" data-line-end="11"><a href="#">dataload.py</a>: Código fuente principal para la versión en Python (Para sistemas operativos basados en unix).</li>
<li class="has-line-data" data-line-start="11" data-line-end="12"><a href="#">dataload-win.py</a>: Código fuente principal para la versión en Python (Para el sistema operativo windows).</li>
<li class="has-line-data" data-line-start="12" data-line-end="14">Files: Carpeta con todos los archivos propuestos a leer en el proyecto</li>
<li class="has-line-data" data-line-start="12" data-line-end="14">Utils: Carpeta con funciones auxiliares e información (constantes) en las que se apoyan algunas funciones</li>
<li class="has-line-data" data-line-start="12" data-line-end="14">Processing: Carpeta con funciones centrados en el procesamiento de datos</li>
</ul>
<h1 class="code-line" data-line-start=14 data-line-end=15 ><a id="Compilacin__C__14"></a>Compilación ( C )</h1>
<p class="has-line-data" data-line-start="16" data-line-end="17">Para compilar el programa en C, usa el siguiente comando:</p>
<pre><code class="has-line-data" data-line-start="19" data-line-end="21">gcc -o dataload main.c utils/utils.c utils/reader.c processing/metrics.c
</code></pre>
<p class="has-line-data" data-line-start="22" data-line-end="23">Esto generará un ejecutable llamado dataload.</p>
<h2 class="code-line" data-line-start=24 data-line-end=25 ><a id="Ejecucin_24"></a>Ejecución</h2>
<h3 class="code-line" data-line-start=25 data-line-end=26 ><a id="Versin_en_C_25"></a>Versión en C</h3>
<p class="has-line-data" data-line-start="27" data-line-end="28">El programa en C puede ejecutarse en diferentes modos según la opción que se pase al ejecutarlo.</p>
<ul>
<li class="has-line-data" data-line-start="29" data-line-end="30">Modo Secuencial (por defecto):</li>
</ul>
<pre><code class="has-line-data" data-line-start="31" data-line-end="33">./dataload -f &lt;directorio&gt;
</code></pre>
<ul>
<li class="has-line-data" data-line-start="33" data-line-end="34">Modo Single-core (-s)</li>
</ul>
<pre><code class="has-line-data" data-line-start="35" data-line-end="37">./dataload -s -f &lt;directorio&gt;
</code></pre>
<ul>
<li class="has-line-data" data-line-start="37" data-line-end="38">Modo Multi-core (-m)</li>
</ul>
<pre><code class="has-line-data" data-line-start="39" data-line-end="41">./dataload -m -f &lt;directorio&gt;
</code></pre>
<h3 class="code-line" data-line-start=42 data-line-end=43 ><a id="Versin_en_Python_42"></a>Versión en Python</h3>
<p class="has-line-data" data-line-start="44" data-line-end="45">El programa en C puede ejecutarse en diferentes modos según la opción que se pase al ejecutarlo.</p>
<ul>
<li class="has-line-data" data-line-start="46" data-line-end="47">Modo Secuencial (por defecto):</li>
</ul>
<pre><code class="has-line-data" data-line-start="48" data-line-end="50">python dataload.py -f &lt;directorio&gt;
</code></pre>
<ul>
<li class="has-line-data" data-line-start="50" data-line-end="51">Modo Single-core (-s)</li>
</ul>
<pre><code class="has-line-data" data-line-start="52" data-line-end="54">python dataload.py -s -f &lt;directorio&gt;
</code></pre>
<ul>
<li class="has-line-data" data-line-start="54" data-line-end="55">Modo Multi-core (-m)</li>
</ul>
<pre><code class="has-line-data" data-line-start="56" data-line-end="58">python dataload.py -m -f &lt;directorio&gt;
</code></pre>
<p class="has-line-data" data-line-start="59" data-line-end="60"><em>En caso de estar ejecutando el archivo para windows cambiar <a href="#">dataload.py</a> por <a href="#">dataload-win.py</a></em></p>
<h2 class="code-line" data-line-start=61 data-line-end=62 ><a id="Parmetros_61"></a>Parámetros</h2>
<ul>
<li class="has-line-data" data-line-start="62" data-line-end="63">-f &lt;directorio&gt;: Especifica el directorio donde se encuentran los archivos CSV. Si no se especifica, se utilizará el directorio actual.</li>
</ul>
