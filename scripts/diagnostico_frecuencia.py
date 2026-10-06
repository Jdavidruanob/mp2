#!/usr/bin/env python3
"""Fase 6, punto 1: muestrea la frecuencia de CPU (scaling_cur_freq, el
equivalente en sysfs de lo que expone /proc/cpuinfo) de todos los núcleos
mientras corre un binario, y promedia esa frecuencia dentro de las
ventanas de tiempo de lectura/filtrado/escritura que el propio programa
reporta en su línea CSV. Se usa para investigar la anomalía de escritura
documentada en docs/reportes/FASE_6.md.

Uso: scripts/diagnostico_frecuencia.py <binario> <entrada> <salida> [args...]
Imprime, por fase, la frecuencia promedio (GHz) de cada núcleo lógico y el
promedio general.
"""
import subprocess
import sys
import threading
import time
from pathlib import Path

GLOB_FREQ = sorted(Path("/sys/devices/system/cpu").glob("cpu[0-9]*/cpufreq/scaling_cur_freq"))
INTERVALO = 0.004  # 4 ms entre muestras


def leer_frecuencias():
    valores = []
    for ruta in GLOB_FREQ:
        try:
            valores.append(int(ruta.read_text().strip()))
        except OSError:
            valores.append(None)
    return valores


def main():
    if len(sys.argv) < 4:
        print(f"Uso: {sys.argv[0]} <binario> <entrada> <salida> [args...]", file=sys.stderr)
        return 1

    binario = sys.argv[1]
    comando = sys.argv[1:]

    muestras = []  # (t_relativo, [freq_core0, freq_core1, ...])
    parar = threading.Event()

    def muestreador(t0):
        while not parar.is_set():
            t = time.monotonic() - t0
            muestras.append((t, leer_frecuencias()))
            time.sleep(INTERVALO)

    t0 = time.monotonic()
    hilo = threading.Thread(target=muestreador, args=(t0,))
    hilo.start()

    proc = subprocess.run(comando, capture_output=True, text=True)

    parar.set()
    hilo.join()

    if proc.returncode != 0:
        print("ERROR ejecutando el binario:", proc.stderr, file=sys.stderr)
        return 1

    lineas = [l for l in proc.stdout.strip().splitlines() if l]
    if len(lineas) < 2:
        print("Salida inesperada:", proc.stdout, file=sys.stderr)
        return 1
    datos = lineas[1].split(",")  # primera línea de datos (un solo filtro con --f)
    # encabezado: ejecutor,entrada,salida,filtro,ancho,alto,lectura_real_s,
    # lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,escritura_real_s,...
    lectura_s = float(datos[6])
    filtrado_s = float(datos[8])
    escritura_s = float(datos[10])

    # Las muestras se tomaron desde ANTES de lanzar el proceso (incluyen el
    # fork/exec y el arranque dinámico del binario), así que hay un offset
    # constante pequeño antes de que el programa empiece a leer. Se estima
    # ese offset como (duración total de las muestras) - (lectura+filtrado+
    # escritura reportados por el programa), y se descarta del inicio.
    t_fin_muestras = muestras[-1][0] if muestras else 0.0
    offset = max(0.0, t_fin_muestras - (lectura_s + filtrado_s + escritura_s))

    ventanas = {
        "lectura": (offset, offset + lectura_s),
        "filtrado": (offset + lectura_s, offset + lectura_s + filtrado_s),
        "escritura": (offset + lectura_s + filtrado_s, offset + lectura_s + filtrado_s + escritura_s),
    }

    n_cores = len(GLOB_FREQ)
    print(f"binario={binario} nucleos_logicos={n_cores} muestras={len(muestras)} "
          f"offset_estimado_s={offset:.4f}")
    print(f"lectura_s={lectura_s:.6f} filtrado_s={filtrado_s:.6f} escritura_s={escritura_s:.6f}")

    for nombre, (ini, fin) in ventanas.items():
        en_ventana = [f for (t, f) in muestras if ini <= t < fin]
        if not en_ventana:
            print(f"  {nombre}: sin muestras en la ventana")
            continue
        n = len(en_ventana)
        por_core = [sum(m[c] for m in en_ventana if m[c] is not None) /
                    max(1, sum(1 for m in en_ventana if m[c] is not None))
                    for c in range(n_cores)]
        promedio_general = sum(por_core) / n_cores / 1e6
        maximo_core = max(por_core) / 1e6
        print(f"  {nombre}: n_muestras={n} freq_promedio_todos_los_nucleos={promedio_general:.3f} GHz "
              f"freq_max_de_un_nucleo={maximo_core:.3f} GHz")
        detalle = ", ".join(f"cpu{c}={v/1e6:.2f}" for c, v in enumerate(por_core))
        print(f"    por núcleo (GHz): {detalle}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
