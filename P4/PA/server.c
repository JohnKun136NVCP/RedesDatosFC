// servidor.c
#include <stdio.h>      
#include <stdlib.h>    
#include <string.h>     
#include <unistd.h>    
#include <arpa/inet.h>  
#include <time.h>

#define PUERTO_BASE 49200      // Puerto base donde el servidor escucha la primera conexión 
#define TAM_BUFFER 1024        // Tamaño del buffer para mensajes, pequeño pero funcional

int main() {
    int socket_escucha;			// estará siempre escuchando en el PUERTO_BASE para aceptar clientes
    int socket_base;		 // por cada cliente nuevo, se crea temporalmente para enviarle el número del puerto asignado
    struct sockaddr_in dir_servidor, dir_cliente;		// estructuras de dirección para servidor y cliente
    socklen_t tam_cliente = sizeof(dir_cliente);	
    char buffer[TAM_BUFFER];	
    int siguiente_puerto = PUERTO_BASE + 1;      // puerto que se asignará a cada cliente, incrementa su valor cada nuevo cliente

    // creamos el socket principal, con IPv4 y TCP
    if ((socket_escucha = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket principal");
        exit(1);
    }

    // preparamos la dirección del servidor
    memset(&dir_servidor, 0, sizeof(dir_servidor));
    dir_servidor.sin_family = AF_INET;                // definimos familia IPv4
    dir_servidor.sin_addr.s_addr = INADDR_ANY;        // escuchamos en todas las interfaces (0.0.0.0)
    dir_servidor.sin_port = htons(PUERTO_BASE);       // seleccionamos el puerto que ya definimos anteriormente
    
    // asociamos el socket a la dirección/puerto
    if (bind(socket_escucha, (struct sockaddr*)&dir_servidor, sizeof(dir_servidor)) < 0) {
        perror("Error al asociar el socket principal");
        exit(1);
    }

    // aquí ya empieza su labor eterna de escuchar y aceptar clientes, el límite de la cola es 5
    listen(socket_escucha, 5);
    printf("Servidor escuchando en puerto %d...\n", PUERTO_BASE);

    // bucle para atender clientes
    while (1) {
        // aceptamos la conexión inicial en el puerto base
        socket_base = accept(socket_escucha, (struct sockaddr*)&dir_cliente, &tam_cliente);
        if (socket_base < 0) {
            perror("Error al aceptar la conexión con el cliente");
            continue;
        }

		// imprimimos en la terminal el aviso y la dirección del cliente
        printf("Cliente conectado desde %s\n", inet_ntoa(dir_cliente.sin_addr));

        // asignamos un puerto nuevo para este nuevo cliente
        int puerto_asignado = siguiente_puerto++;
        snprintf(buffer, sizeof(buffer), "%d", puerto_asignado);

        // enviamos el número que le tocó por medio del socket_base, la interacción del nuevo cliente y socket_escucha ya hasta aquí llega
        send(socket_base, buffer, strlen(buffer), 0);
        printf("Puerto asignado %d enviado al cliente\n", puerto_asignado);

        // lo dormimos para que el cliente tenga chance de recibirlo
        sleep(1);

        // la interacción del nuevo cliente y el socket_base ya hasta aquí llega, solo le manda el puerto asignado
        close(socket_base);
        
        // creamos un socket en el nuevo puerto para ahora sí comunicarnos
        int socket_datos = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dir_datos;
        memset(&dir_datos, 0, sizeof(dir_datos));
        dir_datos.sin_family = AF_INET;
        dir_datos.sin_addr.s_addr = INADDR_ANY;
        dir_datos.sin_port = htons(puerto_asignado);

        if (bind(socket_datos, (struct sockaddr*)&dir_datos, sizeof(dir_datos)) < 0) {
            perror("Error en bind del puerto asignado");
            close(socket_datos);
            continue;
        }

        // escuchamos en el puerto asignado en lo que el cliente asociado llega
        listen(socket_datos, 1);
        printf("Esperando conexión en puerto %d...\n", puerto_asignado);

        // lo aceptamos
        int conexion = accept(socket_datos, (struct sockaddr*)&dir_cliente, &tam_cliente);
        if (conexion < 0) {
            perror("Error en accept del puerto asignado");
            close(socket_datos);
            continue;
        }

		// como lo vamos a recibir por partes, primero tomamos el nombre y definimos donde lo guardaremos y con qué nombre
		char nombre_archivo[256];
		system("mkdir -p Servidor");
		snprintf(nombre_archivo, sizeof(nombre_archivo), "Servidor/archivo_%d.txt", puerto_asignado);

		FILE *f = fopen(nombre_archivo, "w");
		if(!f){
			perror("No se pudo abrir el archivo para guardarlo");
			close(conexion);
			close(socket_datos);
			continue;
		}
		
		int total_bytes = 0;
		int leidos;
		while((leidos = recv(conexion, buffer, sizeof(buffer), 0))>0){
			fwrite(buffer, 1, leidos, f);
			total_bytes += leidos;
		}

		fclose(f);
		
		printf("Archivo guardado en %s\n", nombre_archivo);
        send(conexion, "Archivo recibido\n", 18, 0);

        // fin de la comunicación
        close(conexion);
        close(socket_datos);
    }

    // como tenemos el while(1) no llega a cerrarse el socket principal 
    close(socket_escucha);
    return 0;
}
