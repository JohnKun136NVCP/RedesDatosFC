#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#define BACKLOG        16      // cola de conexiones pendientes en listen()
#define RECV_BUF       8192    // tamaño de bloque para recv() del cuerpo de archivo
#define SLOT_MS_DFLT   6000    // duración por defecto del slot de turno 
#define SKIP_FRAC      6       // fracción para ventana  slot/6

#define MCAST_GRP  "239.255.42.42"  
#define MCAST_PORT 42000            // puerto UDP para TURN/STATE/SKIP

// Volatile porque lo acceden hilos distintos sin locks 
static volatile int  busy = 0;                 // 1 mientras se recibe un archivo 
static volatile int  waiting_for_conn = 0;     // 1 cuando empieza mi slot y aún no llega PUT
static char my_id[8]       = "s01";            // identificador  s01..s04
static char current_turn[8]= "s01";            // de quién es el turno actual
static int  is_coordinator = 0;                // s01 coordina la rueda RR
static char base_dir[256]  = "data";           // carpeta raíz para guardar
static int  slot_ms        = SLOT_MS_DFLT;     // duración del slot de turno
static int  skip_grace_ms  = SLOT_MS_DFLT / SKIP_FRAC; // ventana para SKIP

// Orden fijo del round-robin
static const char *ORDER[4] = {"s01","s02","s03","s04"};

// Sockets UDP para enviar/recibir en el grupo multicast
static int udp_rx = -1, udp_tx = -1;
static struct sockaddr_in mcast_addr;

// Mapea id s0X a índice en ORDER
static int id_to_idx(const char *id){ for(int i=0;i<4;i++) if(strcmp(id,ORDER[i])==0) return i; return 0; }
// Siguiente id en la rueda
static const char* next_id(const char *id){ return ORDER[(id_to_idx(id)+1)&3]; }

// Crea socket TCP en port, reusa dirección, y escucha
static int bind_listen_port(uint16_t port){
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return -1;
    int opt=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY; a.sin_port=htons(port);
    if(bind(fd,(struct sockaddr*)&a,sizeof(a))<0){ close(fd); return -1; }
    if(listen(fd,BACKLOG)<0){ close(fd); return -1; }
    return fd;
}

// Busca un puerto libre >= start para usar como efímero de datos
static int alloc_port_above(uint16_t start,uint16_t *out_port){
    for(uint32_t p=start;p<=60000;p++){
        int lfd=bind_listen_port((uint16_t)p);
        if(lfd>=0){ *out_port=(uint16_t)p; return lfd; }
    }
    return -1;
}

// Acepta una conexión entrante bloqueante
static int accept_one(int lfd){ struct sockaddr_in ca; socklen_t cl=sizeof(ca); return accept(lfd,(struct sockaddr*)&ca,&cl); }

// Lee una línea terminada en \n del socket
static ssize_t recv_line(int fd,char *buf,size_t max){
    size_t i=0; char c;
    while(i+1<max){ ssize_t r=recv(fd,&c,1,0); if(r<=0) return r; if(c=='\n'){ buf[i]=0; return (ssize_t)i; } buf[i++]=c; }
    buf[i]=0; return (ssize_t)i;
}

// Asegura la existencia de un directorio relativo 
static int ensure_dir(const char *subdir){
    char path[512]; const char *home=getenv("HOME"); if(!home) home=".";
    snprintf(path,sizeof(path),"%s/%s",home,subdir);
    struct stat st; if(stat(path,&st)==-1) if(mkdir(path,0700)==-1){ perror("mkdir"); return -1; }
    return 0;
}

// Abre el archivo destino como ~/data/s0X/nombre
static FILE *open_dst_in_base(const char *name){
    char path[512]; const char *home=getenv("HOME"); if(!home) home=".";
    snprintf(path,sizeof(path),"%s/%s/%s/%s",home,base_dir, my_id, name);
    return fopen(path,"wb");
}

// Anuncia turno vigente a todos los servidores
static void send_turn(const char *id){
    char msg[32]; snprintf(msg,sizeof(msg),"TURN %s",id);
    sendto(udp_tx,msg,strlen(msg),0,(struct sockaddr*)&mcast_addr,sizeof(mcast_addr));
}

// Hilo coordinador solo s01: emite TURN cada slot_ms
static void *coordinator_thread(void *_){
    const char *id=ORDER[0];
    for(;;){ send_turn(id); id=next_id(id); usleep(slot_ms*1000); }
    return NULL;
}

// Hilo temporizador de gracia si en el slot no llegó PUT, pido SKIP
static void *skip_timer_thread(void *_){
    usleep(skip_grace_ms*1000);
    if(waiting_for_conn && !busy && strcmp(current_turn,my_id)==0){
        char msg[32]; snprintf(msg,sizeof(msg),"SKIP %s",my_id);
        sendto(udp_tx,msg,strlen(msg),0,(struct sockaddr*)&mcast_addr,sizeof(mcast_addr));
    }
    return NULL;
}

// Hilo receptor de control procesa TURN y SKIP
static void *udp_listener_thread(void *_){
    for(;;){
        char buf[128]; struct sockaddr_in from; socklen_t fl=sizeof(from);
        int n=recvfrom(udp_rx,buf,sizeof(buf)-1,0,(struct sockaddr*)&from,&fl); if(n<=0) continue; buf[n]=0;
        char tok[16],arg[16]; if(sscanf(buf,"%15s %15s",tok,arg)!=2) continue;

        if(strcmp(tok,"TURN")==0){
            // Actualiza el dueño del turno y arranca gracia si soy es valido
            strncpy(current_turn,arg,sizeof(current_turn)-1); current_turn[sizeof(current_turn)-1]=0;
            fprintf(stderr,"[%s] TURN=%s\n",my_id,current_turn);
            if(strcmp(current_turn,my_id)==0){ waiting_for_conn=1; pthread_t th; pthread_create(&th,NULL,skip_timer_thread,NULL); pthread_detach(th); }
            else waiting_for_conn=0;
            continue;
        }
        if(strcmp(tok,"SKIP")==0){
            // Solo el coordinador adelanta turno de inmediato
            if(is_coordinator && strcmp(arg,current_turn)==0){ const char *nid=next_id(current_turn); send_turn(nid); }
            continue;
        }
    }
}

// Inicializa UDP TX/RX y se une al grupo multicast
static int mcast_setup(void){
    // TX
    udp_tx=socket(AF_INET,SOCK_DGRAM,0); if(udp_tx<0) return -1;
    int ttl=1, loop=1; setsockopt(udp_tx,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl));
    setsockopt(udp_tx,IPPROTO_IP,IP_MULTICAST_LOOP,&loop,sizeof(loop));
    memset(&mcast_addr,0,sizeof(mcast_addr)); mcast_addr.sin_family=AF_INET; mcast_addr.sin_port=htons(MCAST_PORT);
    mcast_addr.sin_addr.s_addr=inet_addr(MCAST_GRP);

    // RX
    udp_rx=socket(AF_INET,SOCK_DGRAM,0); if(udp_rx<0) return -1;
    int yes=1; setsockopt(udp_rx,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in local; memset(&local,0,sizeof(local));
    local.sin_family=AF_INET; local.sin_port=htons(MCAST_PORT); local.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(udp_rx,(struct sockaddr*)&local,sizeof(local))<0) return -1;
    struct ip_mreq mreq; mreq.imr_multiaddr.s_addr=inet_addr(MCAST_GRP); mreq.imr_interface.s_addr=htonl(INADDR_ANY);
    if(setsockopt(udp_rx,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))<0) return -1;
    return 0;
}

int main(int argc,char **argv){
    if(argc<4){ fprintf(stderr,"Uso: %s <port_base> <dir_base> <id_servidor> [slot_ms]\n",argv[0]); return 1; }

    // Salida sin buffer para ver logs en tiempo real
    setvbuf(stdout,NULL,_IONBF,0); setvbuf(stderr,NULL,_IONBF,0);

    // CLI puerto base, carpeta base, id servidor, y slot 
    uint16_t port_base=(uint16_t)atoi(argv[1]); const char *dir_base=argv[2];
    strncpy(my_id,argv[3],sizeof(my_id)-1); my_id[sizeof(my_id)-1]=0;
    if(argc>=5){ int v=atoi(argv[4]); if(v>=500) slot_ms=v; skip_grace_ms=(slot_ms/SKIP_FRAC>100)?slot_ms/SKIP_FRAC:100; }

    // Prepara directorios ~/data y ~/data/s0X
    snprintf(base_dir,sizeof(base_dir),"%s",dir_base);
    if(port_base==0){ fprintf(stderr,"port_base no valido\n"); return 1; }
    if(ensure_dir(base_dir)!=0) return 1;
    { char sub[512]; snprintf(sub,sizeof(sub),"%s/%s",base_dir,my_id); if(ensure_dir(sub)!=0) return 1; }

    // Al arrancar se asigna el turno.
    strcpy(current_turn,my_id);
    is_coordinator = (strcmp(my_id,"s01")==0);

    // Multicast y hilos de control
    if(mcast_setup()!=0){ perror("multicast"); return 1; }
    pthread_t th_turns,th_states;
    if(pthread_create(&th_states,NULL,udp_listener_thread,NULL)!=0){ perror("pthread listener"); return 1; }
    if(pthread_create(&th_turns,NULL,is_coordinator?coordinator_thread:(void*(*)(void*))pause,NULL)!=0){ perror("pthread turns"); return 1; }

    // Puerto base hace handshake de puerto efímero
    int lfd_base=bind_listen_port(port_base); if(lfd_base<0){ perror("bind base"); return 1; }
    fprintf(stderr,"[SERVIDOR %s] base %u listo, slot_ms=%d, grace=%d\n",my_id,port_base,slot_ms,skip_grace_ms);

    // Buffer grande para recibir archivo
    char *bigbuf=malloc(1<<20); if(!bigbuf){ fprintf(stderr,"memoria\n"); close(lfd_base); return 1; }

    // Bucle infinito acepta control, negocia puerto efímero, procesa STATUS/PUT
    for(;;){
        // Conexión base: devuelve PORT <eph>\n y cierra
        int c0=accept_one(lfd_base); if(c0<0){ perror("accept base"); continue; }
        uint16_t eph=0; int lfd_eph=alloc_port_above((uint16_t)(port_base+1),&eph);
        if(lfd_eph<0){ fprintf(stderr,"No hay puerto efimero disponible\n"); close(c0); continue; }
        char portmsg[64]; int m=snprintf(portmsg,sizeof(portmsg),"PORT %u\n",eph); send(c0,portmsg,(size_t)m,0); close(c0);

        // Segunda conexión en el puerto efímero para el comando real
        int c1=accept_one(lfd_eph); close(lfd_eph); if(c1<0){ perror("accept eph"); continue; }

        // Protocolo de texto: primera línea define comando
        char line[1024]; ssize_t n=recv_line(c1,line,sizeof(line)); if(n<=0){ close(c1); continue; }

        // STATUS: consulta rápida del estado del servidor
        if(strncmp(line,"STATUS",6)==0){
            const char *resp = busy ? "RECIBIENDO\n" : "ESPERA\n";
            send(c1,resp,strlen(resp),0); close(c1); continue;
        }

        // PUT <fname>\n<size>\n  recepción de archivo condicionada al turno
        if(strncmp(line,"PUT ",4)==0){
            waiting_for_conn=0; // llegó actividad en el slot
            fprintf(stderr,"[%s] CMD=%s\n",my_id,line);

            // Parse de nombre y tamaño esperado
            char fname[256]={0}; sscanf(line+4,"%255s",fname);
            if(recv_line(c1,line,sizeof(line))<=0){ close(c1); continue; }
            long long total=atoll(line); if(total<0) total=0;

            // Política central solo el dueño del turno procesa
            if(strcmp(current_turn,my_id)!=0){
                // Enviar WAIT y cerrar garantiza que solo uno recibe a la vez.
                send(c1,"WAIT\n",5,0);
                close(c1);
                continue;
            }

            // En turno autoriza y recibe todo el archivo 
            send(c1,"GO\n",3,0);

            FILE *out=open_dst_in_base(fname); if(!out){ close(c1); continue; }

            // Broadcast de estado: inicio de recepción
            { char msg[64]; int k=snprintf(msg,sizeof(msg),"STATE RECIBIENDO %s",my_id);
              sendto(udp_tx,msg,k,0,(struct sockaddr*)&mcast_addr,sizeof(mcast_addr)); }

            busy=1;
            fprintf(stderr,"[%s] RECIBIENDO %s (%lld bytes)\n",my_id,fname,total);

            // Bucle de lectura hasta completar total 
            long long got=0;
            while(got<total){
                ssize_t r=recv(c1,bigbuf,RECV_BUF,0);
                if(r<=0) break;
                fwrite(bigbuf,1,(size_t)r,out);
                got+=r;
            }
            fclose(out); busy=0;

            // Broadcast de estado fin de recepción
            { char msg[64]; int k=snprintf(msg,sizeof(msg),"STATE LIBRE %s",my_id);
              sendto(udp_tx,msg,k,0,(struct sockaddr*)&mcast_addr,sizeof(mcast_addr)); }

            fprintf(stderr,"[%s] LISTO %s\n",my_id,fname);
            send(c1,"OK\n",3,0);  // confirma al cliente
            close(c1); continue;
        }

        // Comando desconocido
        close(c1);
    }
}
