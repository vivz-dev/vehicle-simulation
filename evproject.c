#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define NUM_RUEDAS 4
#define VELOCIDAD_MINIMA 1
#define VELOCIDAD_MAXIMA 250
#define CAPACIDAD_BATERIA 100
#define CONSUMO_POR_CICLO 1
#define TASA_DESCARGA 0.1
#define TASA_CARGA 0.05
#define SEGUNDOS_REGENERACION 4
#define ACELERACION_MAXIMA 10
#define ACELERACION_MINIMA 1
//status ruedas
#define ACELERANDO 1
#define REGENERANDO 2
#define SIN_EFECTO 0
//status bateria
#define DESCARGANDO 0
#define CARGANDO 1
#define ESTABLE 2

typedef struct {
    int velocidad_crucero;
    int tasa_aceleracion;
} Configuracion;

typedef struct {
    int id;
    int status;
    int velocidad;
    int activo;
} Motor;

typedef struct {
    double porcentaje;
    int status;
} Bateria;

typedef struct {
    int frenando;
    int acelerando;
} SistemaPrincipal;

// Mutex y variables de condición
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t apagarMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ruedasMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t acelerarMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t frenarMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_principal = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_bateria = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_controlador = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_ruedas[NUM_RUEDAS];
pthread_cond_t cond_apagar = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_acelerar[NUM_RUEDAS];
pthread_cond_t cond_frenar[NUM_RUEDAS];

int principal_ready = 0;
int bateria_ready = 0;
int controlador_ready = 0;
int ruedas_ready[NUM_RUEDAS] = {0};
pthread_t hilos_motores[NUM_RUEDAS];
Motor motores[NUM_RUEDAS];
Bateria bateria;
Configuracion config;
SistemaPrincipal sistema;
int ruedasReadyCont = 0;
int apagarMotores = 0;
int actualizarStatus = 0;
int fallos = 0;

void printWelcome() {
    printf("\n");
    printf("██╗    ██╗███████╗██╗      ██████╗ ██████╗ ███╗   ███╗███████╗\n");
    printf("██║    ██║██╔════╝██║     ██╔════╝██╔═══██╗████╗ ████║██╔════╝\n");
    printf("██║ █╗ ██║█████╗  ██║     ██║     ██║   ██║██╔████╔██║█████╗  \n");
    printf("██║███╗██║██╔══╝  ██║     ██║     ██║   ██║██║╚██╔╝██║██╔══╝  \n");
    printf("╚███╔███╔╝███████╗███████╗╚██████╗╚██████╔╝██║ ╚═╝ ██║███████╗\n");
    printf(" ╚══╝╚══╝ ╚══════╝╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝\n");
}

void printCar() {
    printf("\n");
    printf("                  .\n");
    printf("    __            |\\\n");
    printf(" __/__\\___________| \\_\n");
    printf("|   ___    |  ,|   ___`-.\n");
    printf("|  /   \\   |___/  /   \\  `-.\n");
    printf("|_| (O) |________| (O) |____|\n");
    printf("   \\___/          \\___/\n");
}

void printLlave(){
    printf("   ooo\n");
    printf("    .---.\n");
    printf(" o`  o   /    |\\________________\n");
    printf("o`   'oooo()  | ________   _   _)\n");
    printf("`oo   o` \\    |/        | | | |\n");
    printf("  `ooo'   `---'         \"-\" |_|\n");
    printf("                                   \n");
}

void printCalavera() {
    printf("\n");
    printf("888888888888888888888888888888888888888888888888888888888888\n");
    printf("888888888888888888888888888888888888888888888888888888888888\n");
    printf("8888888888888888888888888P\"\"  \"\"9888888888888888888888888888\n");
    printf("8888888888888888P\"88888P          988888\"9888888888888888888\n");
    printf("8888888888888888  \"9888            888P\"  888888888888888888\n");
    printf("888888888888888888bo \"9  d8o  o8b  P\" od88888888888888888888\n");
    printf("888888888888888888888bob 98\"  \"8P dod88888888888888888888888\n");
    printf("888888888888888888888888    db    88888888888888888888888888\n");
    printf("88888888888888888888888888      8888888888888888888888888888\n");
    printf("888888888888888888888888P\"9bo  odP\"9888888888888888888888888\n");
    printf("88888888888888888888P\" od88888888bo \"98888888888888888888888\n");
    printf("888888888888888888   d88888888888888b   88888888888888888888\n");
    printf("8888888888888888888oo8888888888888888oo888888888888888888888\n");
    printf("888888888888888888888888888888888888888888888888888898888888\n");
}

void printBye(){
    printf(" █████                        \n");
    printf("░░███                         \n");
    printf(" ░███████  █████ ████  ██████ \n");
    printf(" ░███░░███░░███ ░███  ███░░███\n");
    printf(" ░███ ░███ ░███ ░███ ░███████ \n");
    printf(" ░███ ░███ ░███ ░███ ░███░░░  \n");
    printf(" ████████  ░░███████ ░░██████ \n");
    printf("░░░░░░░░    ░░░░░███  ░░░░░░  \n");
    printf("            ███ ░███           \n");
    printf("           ░░██████            \n");
    printf("            ░░░░░░             \n");

}

void print_help(char *program_name) {
    printf("Uso: %s\n", program_name);
    printf("El programa no admite parametros adicionales\n");
}

// motores
void *rueda_thread(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // esperar bateria
    pthread_mutex_lock(&mutex);
    while (!bateria_ready) {
        pthread_cond_wait(&cond_bateria, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    Motor *motor = (Motor *)arg;

    // para que se inicie uno a la vez y actualizar la variable ruedasReadyCont
    pthread_mutex_lock(&mutex);
    ruedas_ready[motor->id - 1] = 1;
    pthread_cond_signal(&cond_ruedas[motor->id - 1]);
    printf("✔️️ Motor %d iniciado!\n", motor->id);
    ruedasReadyCont++;
    pthread_mutex_unlock(&mutex);

    while(!apagarMotores){
        pthread_mutex_lock(&acelerarMutex);
        // pthread_cond_wait(&cond_acelerar[motor->id - 1], &acelerarMutex);
        if(sistema.acelerando==1 && sistema.frenando == 0){
            // printf("ENTRO AL IF 1\n");
            if(motor->velocidad < config.velocidad_crucero && motor->activo == 1){
                motor->velocidad++;
                motor->status=ACELERANDO;
                double descarga = (TASA_DESCARGA * motor->velocidad) / config.velocidad_crucero;
                bateria.porcentaje-=descarga;
                bateria.status=DESCARGANDO;
                // printf("Descargando motor ->%d. Batería:->%lf\n",motor->id, bateria.porcentaje);
                // printf("Descarga: ->%lf\n",descarga);
                // descarga = 0;
            }
        }else if(sistema.frenando==1 && sistema.acelerando == 0 && motor->activo == 1){
            while(motor->velocidad > 0){
                motor->status=REGENERANDO;
                double recarga = (TASA_CARGA * motor->velocidad) / config.velocidad_crucero;
                if(recarga > 0.0){
                    bateria.porcentaje+=recarga;
                    bateria.status=CARGANDO;
                    motor->velocidad--;
                    // printf("Recargando motor ->%d. Batería:->%lf\n",motor->id, bateria.porcentaje);
                    // printf("Recarga: ->%lf\n",recarga);
                }
            }
        }
        // printf("sale de terminar la funcion\n");
        pthread_mutex_unlock(&acelerarMutex);
    }

    // //lock para terminar el proceso
    // while (!apagarMotores) {
    //     continue;
    // }

    pthread_exit(NULL);
}

// bateria
void *bateria_thread(void *arg) {
    // esperar sistema princip.
    pthread_mutex_lock(&mutex);
    while (!principal_ready) {
        pthread_cond_wait(&cond_principal, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    printf("Iniciando Batería...\n");
    sleep(1);

    pthread_mutex_lock(&mutex);
    bateria_ready = 1;
    bateria.status=ESTABLE;
    bateria.porcentaje = 100;
    pthread_cond_broadcast(&cond_bateria);
    pthread_mutex_unlock(&mutex);

    printf("✔️️ Batería lista!\n");
    pthread_exit(NULL);
}

// FUNCIONALIDADES
void acelerar(){
    printf("Acelerando...\n");
    sistema.acelerando = 1;
    sistema.frenando = 0;
    // acelerando 
    pthread_mutex_lock(&acelerarMutex);
    for (int i = 0; i < ruedasReadyCont; i++) {
        pthread_cond_signal(&cond_acelerar[motores[i].id - 1]);
    }
    pthread_mutex_unlock(&acelerarMutex);

    sleep(1);
}

void frenar(){
    printf("Frenando...\n");
    sleep(1);
    sistema.acelerando = 0;
    sistema.frenando = 1;

    pthread_mutex_lock(&acelerarMutex);
    for (int i = 0; i < ruedasReadyCont; i++) {
        // pthread_mutex_lock(&acelerarMutex);
        pthread_cond_signal(&cond_acelerar[motores[i].id - 1]);
        // pthread_mutex_unlock(&acelerarMutex);
    }
    pthread_mutex_unlock(&acelerarMutex);
}

void apagar(){
    printf("Apagando....\n");
    apagarMotores = 1;
    principal_ready = 0;
    bateria_ready = 0;
    controlador_ready = 0;
    for (int i = 0; i < NUM_RUEDAS; i++) {
        if(ruedas_ready[i] != 0){
            ruedas_ready[i] = 0;
        }
    }
}

void flush_stdin(){
    while (getchar() != '\n');
}

void fijar_configuracion(){
    int velocidad_crucero = 0;
    int tasa_aceleracion = 0;
    
    while(velocidad_crucero == 0){
        flush_stdin();
        printf("Fijar velocidad crucero >>");
        if (scanf("%d", &velocidad_crucero) != 1) {
            printf("✘ Entrada no válida: Ingrese nuevamente.\n");
            velocidad_crucero = 0;
        }else if(velocidad_crucero < VELOCIDAD_MINIMA || velocidad_crucero > VELOCIDAD_MAXIMA){
            printf("✘ Ingrese un valor entero entre %d y %d.\n",VELOCIDAD_MINIMA, VELOCIDAD_MAXIMA);
            velocidad_crucero = 0;
        }else{
            printf("✔️️ Velocidad crucero -->%d\n", velocidad_crucero);
            config.velocidad_crucero = velocidad_crucero;
        }
    }

    while(tasa_aceleracion == 0){
        flush_stdin();
        printf("Fijar tasa de aceleración >>");
        if (scanf("%d", &tasa_aceleracion) != 1) {
            printf("✘ Entrada no válida: Ingrese nuevamente.\n");
            tasa_aceleracion = 0;
        }else if(tasa_aceleracion < ACELERACION_MINIMA || tasa_aceleracion > ACELERACION_MAXIMA){
            printf("✘ Ingrese un valor entero entre %d y %d.\n",ACELERACION_MINIMA, ACELERACION_MAXIMA);
            tasa_aceleracion = 0;
        }else{
            printf("✔️️ Aceleracion -->%d\n", tasa_aceleracion);
            config.tasa_aceleracion = tasa_aceleracion;
        }
    }
}

void esperar_ruedas(){
    pthread_mutex_lock(&ruedasMutex);
    while(ruedasReadyCont != NUM_RUEDAS){
        continue;
    }
    pthread_mutex_unlock(&ruedasMutex);
}

void status_general(){
    printf(" ____________________________________ \n");
    printf("        INFORMACION   GENERAL       \n");
    printf("  Velocidad crucero:   [%d] km/h.\n", config.velocidad_crucero);
    for (int i = 0; i < ruedasReadyCont; i++) {
        printf("  Velocidad del motor %d: [%d] km/h.\n", i+1, motores[i].velocidad);
    }
    printf("  Porcentaje Bateria:    %lf         \n", bateria.porcentaje);
    printf(" ____________________________________ \n");
}

void status_ruedas(){
    
    printf(" ________________________________ \n");
    printf("|        STATUS DE MOTORES       |\n");
    for (int i = 0; i < ruedasReadyCont; i++) {
        if(motores[i].id){
            switch(motores[i].status){
            case 0:
                printf("| Motor %d: SIN EFECTO            |\n",motores[i].id);
                break;
            case 1:
                printf("| Motor %d: ACELERANDO            |\n",motores[i].id);
                break;
            case 2: 
                printf("| Motor %d: REGENERANDO           |\n",motores[i].id);
                break;
            default:
                printf("| Motor %d: SIN INFORMACION       |\n",motores[i].id);
                break;
            }
        }
    }
    printf(" ________________________________\n");
}

void eliminar_motor(int indice){
    if (indice < 0 || indice >= NUM_RUEDAS) {
        printf("Índice fuera de rango.\n");
        return;
    }
    for (int i = indice; i < ruedasReadyCont - 1; i++) {
        motores[i] = motores[i + 1];
    }
    // (*n)--;
}

void fallo_motor(){
    fallos++;
    int indice = ruedasReadyCont-1;
    printf("Simulando en fallo de un motor... \n");
    printCalavera();
    ruedas_ready[indice] = 0;
    motores[indice].activo = 0;
    eliminar_motor(indice);
    pthread_cancel(hilos_motores[indice]);
    printf("Hilo cancelado...\n");
    ruedasReadyCont--;
    sleep(1);
}

// Sistema principal
void *sistema_principal_thread(void *arg) {
    int instruccion = 0;
    pthread_t bateria;

    pthread_create(&bateria, NULL, bateria_thread, NULL);
    printf("Iniciando sistema de control...\n");
    sleep(1);

    for (int i = 0; i < NUM_RUEDAS; i++) {
        pthread_cond_init(&cond_acelerar[i], NULL);
    }

    for (int i = 0; i < NUM_RUEDAS; i++) {
        motores[i].id = i + 1;
        motores[i].status = SIN_EFECTO;
        motores[i].velocidad = 0;
        motores[i].activo = 1;
        printf("Esperando motor %d...\n", i+1);
        sleep(1);
        pthread_create(&hilos_motores[i], NULL, rueda_thread, (void *)&motores[i]);
    }
    // Actualizar el estado y señalizar a otros hilos
    pthread_mutex_lock(&mutex);
    principal_ready = 1;
    pthread_cond_broadcast(&cond_principal);
    pthread_mutex_unlock(&mutex);

    printf("✔️️ Sistema de control iniciado!\n");
    esperar_ruedas();
    printCar();
    fijar_configuracion();

    // printf("CONFIG, %d \n", config.tasa_aceleracion);
    // printf("CONFIG, %d \n", config.velocidad_crucero);

    // TODO: FRENAR

    while(instruccion == 0){
        status_ruedas();
        status_general();
        
        flush_stdin();
        printf(" ________________________________ \n");
        if(!sistema.acelerando){
            printf("| 1. Acelerar                    |\n");
        }
        printf("| 2. Apagar                      |\n");
        if(fallos<2){
            printf("| 3. Simular fallo de un motor   |\n");
        }
        if(sistema.acelerando){
            printf("| 4. Frenar                      |\n");
        }
        printf(" ________________________________\n");
        printf("Ingrese una opcion: ");

        if (scanf("%d", &instruccion) != 1) {
            printf("✘ Entrada no válida: Ingrese nuevamente.\n");
            instruccion = 0;
        }

        switch(instruccion){
        // manejar opciones incorrectas
        case 1:
            if(!sistema.acelerando){
                acelerar();
            }else{
                printf("✘ Usted no puede realizar esa accion!\n");
                sleep(1);
            }
            instruccion = 0;
            break;
        case 2:
            apagar();
            break;
        case 3:
            if(fallos<2){
                fallo_motor();
            }else{
                printf("✘ Usted ya ha eliminado 2 motores!\n");
                sleep(1);
            }
            instruccion = 0;
            break;
        case 4:
            if(sistema.acelerando){
                frenar();
            }else{
                printf("✘ Usted no puede realizar esa accion!\n");
                sleep(1);
            }
            instruccion = 0;
            break;
        default:
            printf("✘ Opción incorrecta: Ingrese nuevamente.\n");
            instruccion = 0;
            sleep(1);
            break;
        }
    }

    for (int i = 0; i < NUM_RUEDAS; i++) {
        pthread_cond_destroy(&cond_acelerar[i]);
    }

    // controlador de motores y batería terminen
    // Esperar a que todos los hilos de los motores terminen
    for (int i = 0; i < ruedasReadyCont; i++) {
        if(hilos_motores[i]){
            pthread_join(hilos_motores[i], NULL);
            printf("✔️️ proceso || Rueda %d || terminado!\n", i+1);
        }
    }
    pthread_join(bateria, NULL);
    printf("✔️️ proceso || Bateria || terminado!\n");
    
    pthread_exit(NULL);
}

// cuando enciende se crean todos los hilos
void encender() {
    printLlave();

    pthread_t sistemaControl;
    pthread_create(&sistemaControl, NULL, sistema_principal_thread, NULL);

    //espera hasta que termine el hilo del sistema del control principal
    //el hilo se termina cuando los demas se terminan
    //luego de recibir la senal de apagar de las ruedas
    pthread_join(sistemaControl, NULL);
    printf("✔️️ proceso || Sistema de Control Principal || terminado!\n");
    printBye();
}

int main(int argc, char *argv[]){

    printWelcome();

    // TODO: poner un tamanio al input del usuario para controlar y usar programacion defensiva
    // TODO: usar lo mismo para todos los inputs del usuario

    // TODO: asegurarme que el paso entre mensajes es CONTROL <-> CONTROLADOR <-> RUEDAS
    // CONTROL <-> BATERIA
    // Y NO SALTARME CADENAS DE PASO DE MENSAJES
    // QUE IPC ESTOY USANDO????

    int opcion;

    // verificar que no se ingresan parametros adicionales
    if (argc > 1) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }else{

        while(1){
            printf(" _____________\n");
            printf("| 1. Encender |\n");
            printf("| 2. Apagar   |\n");
            printf(" -------------\n");
            printf("Ingrese una opcion: ");

            //controlar que se ingrese un numero
            if (scanf("%d", &opcion) != 1) {
                while (getchar() != '\n');
                printf("✘ Entrada no válida: Ingrese nuevamente.\n");
                continue;
            }
            switch(opcion){
                // manejar opciones incorrectas
                case 1:
                    encender();
                    break;
                case 2:
                    apagar();
                    break;
                default:
                    printf("✘ Opción incorrecta: Ingrese nuevamente.\n");
                    continue;
            }
            break;
        }
    }

    return 0;
}