# Imagen única usada por los 4 nodos del clúster (maestro + 3 trabajadores,
# ver docker-compose.yml). Todos los contenedores son simétricos: la misma
# imagen, el mismo par de llaves SSH, el mismo compilador y la misma copia
# del proyecto (montada por volumen). Lo único que distingue a un nodo de
# otro es su hostname/nombre de servicio dentro de la red de Docker.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    openmpi-bin \
    openmpi-common \
    libopenmpi-dev \
    openssh-server \
    openssh-client \
    iproute2 \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# --- SSH sin contraseña entre los contenedores ---
# Los 4 contenedores corren la MISMA imagen, así que todos terminan con el
# MISMO par de llaves (generado una sola vez, en el build). Cada uno se
# agrega a sí mismo (y por lo tanto a los demás, porque es la misma clave)
# en su propio authorized_keys. Es una simplificación deliberada, aceptable
# solo porque la red es interna de Docker (docker-compose.yml la define con
# internal: true) y nunca se expone fuera del host: no es un esquema de
# llaves apto para producción, está documentado también en
# docs/reportes/FASE_4.md.
RUN mkdir -p /run/sshd /root/.ssh && \
    ssh-keygen -t ed25519 -f /root/.ssh/id_ed25519 -N "" -q && \
    cat /root/.ssh/id_ed25519.pub >> /root/.ssh/authorized_keys && \
    chmod 700 /root/.ssh && \
    chmod 600 /root/.ssh/authorized_keys /root/.ssh/id_ed25519 && \
    printf 'Host *\n\tStrictHostKeyChecking no\n\tUserKnownHostsFile=/dev/null\n' > /root/.ssh/config && \
    chmod 600 /root/.ssh/config

# Permitir login de root por llave (sin contraseña), que es con quien MPI
# necesita conectarse por SSH entre nodos en este clúster de ejemplo.
RUN sed -i 's/^#\?PermitRootLogin.*/PermitRootLogin prohibit-password/' /etc/ssh/sshd_config && \
    sed -i 's/^#\?PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config

EXPOSE 22

# El proceso principal del contenedor es sshd en primer plano: mantiene el
# contenedor vivo y accesible por SSH desde los demás nodos. La compilación
# y el mpirun real se disparan aparte, a demanda, con run_mpi.sh (ver
# docs/reportes/FASE_4.md para el flujo completo).
CMD ["/usr/sbin/sshd", "-D"]
