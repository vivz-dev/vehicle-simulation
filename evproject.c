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

typedef struct {
    int velocidad_crucero;
    int tasa_aceleracion;
} Configuracion;

typedef struct {
    int id;
    int status;
} Motor;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char message[256];
    int message_ready;
} Status;

// Mutex y variables de condición
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t apagarMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ruedasMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_principal = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_bateria = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_controlador = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_ruedas[NUM_RUEDAS];
pthread_cond_t cond_apagar = PTHREAD_COND_INITIALIZER;

int principal_ready = 0;
int bateria_ready = 0;
int controlador_ready = 0;
int ruedas_ready[NUM_RUEDAS] = {0};
int ruedasReadyCont = 0;
int apagarMotores = 0;

Status statPrincipalControlador;
Status statControladorRueda[NUM_RUEDAS];

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

// HILOS
// motores
void *rueda_thread(void *arg) {

    // esperar controlador
    pthread_mutex_lock(&mutex);
    while (!controlador_ready) {
        pthread_cond_wait(&cond_controlador, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    Motor *motor = (Motor *)arg;

    pthread_mutex_lock(&mutex);
    ruedas_ready[motor->id - 1] = 1;
    pthread_cond_signal(&cond_ruedas[motor->id - 1]);
    printf("✔️️ Motor %d iniciado!\n", motor->id);
    ruedasReadyCont++;
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&statControladorRueda[motor->id].mutex);
    snprintf(statControladorRueda[motor->id].message, sizeof(statControladorRueda[motor->id].message), "%d", motor->status);
    statControladorRueda[motor->id].message_ready = 1;
    pthread_cond_signal(&statControladorRueda[motor->id].cond);
    pthread_mutex_unlock(&statControladorRueda[motor->id].mutex);
    
    // lock para terminar el proceso
    pthread_mutex_lock(&apagarMutex);
    while (!apagarMotores) {
        pthread_cond_wait(&cond_apagar, &apagarMutex);
    }
    pthread_mutex_unlock(&apagarMutex);

    pthread_exit(NULL);
}

// controlador de motores
void *controlador_motores_thread(void *arg) {
    
    // esperar bateria
    pthread_mutex_lock(&mutex);
    while (!bateria_ready) {
        pthread_cond_wait(&cond_bateria, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    printf("Iniciando Controlador de Motores...\n");
    //sleep(1);

    pthread_mutex_lock(&mutex);
    controlador_ready = 1;
    pthread_cond_broadcast(&cond_controlador);
    pthread_mutex_unlock(&mutex);

    printf("✔️️ Controlador de motores listo!\n");

    pthread_t hilos_motores[NUM_RUEDAS];
    Motor motores[NUM_RUEDAS];

    for (int i = 0; i < NUM_RUEDAS; i++) {
        motores[i].id = i + 1;
        motores[i].status = SIN_EFECTO;
        printf("Esperando motor %d...\n", i+1);
        //sleep(1);
        pthread_create(&hilos_motores[i], NULL, rueda_thread, (void *)&motores[i]);
    }

        // Espera por un mensaje del hilo padre
        pthread_mutex_lock(&statPrincipalControlador.mutex);
        while (!statPrincipalControlador.message_ready) {
            pthread_cond_wait(&statPrincipalControlador.cond, &statPrincipalControlador.mutex);
        }
        // Procesa el mensaje recibido
        printf("CONTROLADOR COMUNICA: %s\n", statPrincipalControlador.message);
        pthread_mutex_unlock(&statPrincipalControlador.mutex);

        // Responde al padre
            // Recibir números de los sub-hijos y enviarlos al padre
        int total = 0;
        for (int i = 0; i < NUM_RUEDAS; ++i) {
            pthread_mutex_lock(&statControladorRueda[i].mutex);
            while (!statControladorRueda[i].message_ready) {
                pthread_cond_wait(&statControladorRueda[i].cond, &statControladorRueda[i].mutex);
            }
            total += atoi(statControladorRueda[i].message);
            pthread_mutex_unlock(&statControladorRueda[i].mutex);
        }

        // Enviar resultado al padre
    pthread_mutex_lock(&statPrincipalControlador.mutex);
    snprintf(statPrincipalControlador.message, sizeof(statPrincipalControlador.message), "%d", total);
    statPrincipalControlador.message_ready = 1;
    pthread_cond_signal(&statPrincipalControlador.cond);
    pthread_mutex_unlock(&statPrincipalControlador.mutex);
    
    // Esperar a que todos los hilos de los motores terminen
    for (int i = 0; i < NUM_RUEDAS; i++) {
        pthread_join(hilos_motores[i], NULL);
        printf("✔️️ proceso || Rueda %d || terminado!\n", i+1);
    }

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
    //sleep(1);

    pthread_mutex_lock(&mutex);
    bateria_ready = 1;
    pthread_cond_broadcast(&cond_bateria);
    pthread_mutex_unlock(&mutex);

    printf("✔️️ Batería lista!\n");
    pthread_exit(NULL);
}

// FUNCIONALIDADES
void acelerar(){
    printf("Acelerando....");

}

void apagar(){
    pthread_mutex_lock(&ruedasMutex);
    apagarMotores = 1;
    pthread_cond_broadcast(&cond_apagar);
    pthread_mutex_unlock(&ruedasMutex);

    principal_ready = 0;
    bateria_ready = 0;
    controlador_ready = 0;
    for (int i = 0; i < NUM_RUEDAS; i++) {
        ruedas_ready[i] = 0;
    }
}

void flush_stdin(){
    while (getchar() != '\n');
}

void fijar_configuracion(Configuracion *config){
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
            config->velocidad_crucero = velocidad_crucero;
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
            config->tasa_aceleracion = tasa_aceleracion;
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

void status_ruedas(){

    // Enviar mensaje del hijo CONTROLADOR
    // EL CONTROLADOR RECIBE LOS 4 MENSAJES Y ME DA UN RESUMEN

    // Inicializa la estructura de comunicación
    pthread_mutex_init(&statPrincipalControlador.mutex, NULL);
    pthread_cond_init(&statPrincipalControlador.cond, NULL);
    statPrincipalControlador.message_ready = 0;

    for (int i = 0; i < NUM_RUEDAS; ++i) {
        pthread_mutex_init(&statControladorRueda[i].mutex, NULL);
        pthread_cond_init(&statControladorRueda[i].cond, NULL);
        statControladorRueda[i].message_ready = 0;
    }

        // Enviar mensaje al hijo
    pthread_mutex_lock(&statPrincipalControlador.mutex);
    snprintf(statPrincipalControlador.message, sizeof(statPrincipalControlador.message), "Mensaje del padre");
    statPrincipalControlador.message_ready = 1;
    pthread_cond_signal(&statPrincipalControlador.cond);
    pthread_mutex_unlock(&statPrincipalControlador.mutex);

    // Esperar resultado del hijo
    pthread_mutex_lock(&statPrincipalControlador.mutex);
    while (!statPrincipalControlador.message_ready) {
        pthread_cond_wait(&statPrincipalControlador.cond, &statPrincipalControlador.mutex);
    }
    printf("Padre recibió total: %s\n", statPrincipalControlador.message);
    pthread_mutex_unlock(&statPrincipalControlador.mutex);



    // printf("%s\n", statPrincipalControlador.message); //respuesta dle controlador
    // pthread_mutex_unlock(&statPrincipalControlador.mutex);

}

void fallo_motor(){
    // TODO 
    printf("Simulando en fallo de un motor... \n");
    printf("Elija un motor 1-4... \n");
    printf("El proceso x ha terminado... \n");
    printf("Redistribuyendo cargas... \n");
    printf("Mostrar bateria de motores y bateria principal... \n");
    printCalavera();
}

// Sistema principal
void *sistema_principal_thread(void *arg) {
    int instruccion = 0;
    pthread_t controladorMotores, bateria;
    Configuracion config;

    // Controlador de motores y batería
    pthread_create(&controladorMotores, NULL, controlador_motores_thread, NULL);
    pthread_create(&bateria, NULL, bateria_thread, NULL);

    printf("Iniciando sistema de control...\n");
    //sleep(1);

    // Actualizar el estado y señalizar a otros hilos
    pthread_mutex_lock(&mutex);
    principal_ready = 1;
    pthread_cond_broadcast(&cond_principal);
    pthread_mutex_unlock(&mutex);

    printf("✔️️ Sistema de control iniciado!\n");

    esperar_ruedas();

    printCar();
    
    fijar_configuracion(&config);

    // printf("CONFIG, %d \n", config.tasa_aceleracion);
    // printf("CONFIG, %d \n", config.velocidad_crucero);

    // TODO: FRENAR

    while(instruccion == 0){
        status_ruedas();
        
        flush_stdin();
        printf(" ________________________________ \n");
        printf("| 1. Acelerar                    |\n");
        printf("| 2. Apagar                      |\n");
        printf("| 3. Simular fallo de un motor   |\n");
        printf(" ________________________________\n");
        printf("Ingrese una opcion: ");

        if (scanf("%d", &instruccion) != 1) {
            printf("✘ Entrada no válida: Ingrese nuevamente.\n");
            instruccion = 0;
        }

        switch(instruccion){
        // manejar opciones incorrectas
        case 1:
            acelerar();
            break;
        case 2:
            apagar();
            break;
        case 3:
            fallo_motor();
            break;
        default:
            printf("✘ Opción incorrecta: Ingrese nuevamente.\n");
            instruccion = 0;
            break;
        }
    }

    printf("ESPERANDO A LOS DEMAS HILOS -> %d\n", instruccion);

    // controlador de motores y batería terminen
    pthread_join(controladorMotores, NULL);
    printf("✔️️ proceso || Controlador de Motores || terminado!\n");
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
                    printf("");
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