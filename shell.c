/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "OS Concepts Essentials", Silberschatz et al.
 *
 * To compile and run the program:
 *   $ gcc shell.c job_control.c -o shell
 *   $ ./shell
 *	(then type ^D to exit program)
**/

// job = tarea

#include "job_control.h"   /* Remember to compile with module job_control.c */

#define MAX_LINE 256 // Máximo número de caracteres que puede tener un comando


job *tareas;   // declara globalmente lista de tareas

void manejador(int senal){
    job *item = tareas->next;
    job *next_item;
    int status, info, pid_wait;
    enum status status_res;


    block_SIGCHLD();       // Cada vez que accedés o modificás la lista tareas (blockear y desbloquear) (add_job, delete_job, get_item_bypos, etc)

    while (item != NULL) {

        next_item = item->next; //  Guardas el siguiente antes de borrar el actual
        pid_wait = waitpid(item->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);   // hacer un wait a cada comando y recoger 
        // el waitpid recoge solo si pid_wait cambia de valor. pid_wait guarda el proceso q recoge waitpid
        
        if (pid_wait == item->pgid){
        status_res = analyze_status(status, &info);  // ver estado actual real despues del cambio

            if (status_res == SUSPENDED){
                printf("\nBackground pid: %d, command: %s, %s, info: %d\n", 
                    item->pgid, item->command, status_strings[status_res], info);
                item->state = STOPPED;    // cambiamos estado a detenido

            } else if (status_res == CONTINUED) {  // tareas que vuelven a ejecutarse despues de estar suspendidas
                printf("\nBackground pid: %d, command: %s, %s, info: %d\n",
                    item->pgid, item->command, status_strings[status_res], info);
                item->state = BACKGROUND;  // Se reanuda ejecución en segundo plano

            } else if (status_res == EXITED){
                printf("\nBackground pid: %d, command: %s, %s, info: %d\n", 
                    item->pgid, item->command, status_strings[status_res], info);
                delete_job(tareas, item);        // borramos tarea de la lista
            } 
       }
       item = next_item; //  Pasas al siguiente elemento
    } 
    unblock_SIGCHLD();
}

int main(void)
{
	char inputBuffer[MAX_LINE];       // Guarda el texto que escribe el usuario
    int background;                   // Vale 1 si el comando termina en '&' (segundo plano)
    char *args[MAX_LINE/2];           // Guarda los argumentos separados del comando

    int pid_fork, pid_wait;           // PID del hijo creado con fork, y del hijo que se espera
    int status;                       // Estado devuelto por waitpid
    enum status status_res;           // Estado interpretado (salió bien, fue detenido, etc.)
    int info;                         // Información adicional sobre cómo terminó el proceso
    char *file_in = NULL;
    char *file_out = NULL;


    job *item;   // declara para poder usar en new y add job
    int primerplano = 0;   // variable bool (false 0, true 1)

    ignore_terminal_signals();   // ignora senales
    // SIGCHLD → lanza señal al padre cuando el hijo termina o se suspende (creo)
    signal(SIGCHLD, manejador);  //  es como un trycatch que maneja las señales de los hijos en la lista
    tareas = new_list("tareas");   // crear lista

	while (1)   // Bucle principal del shell, se ejecuta indefinidamente, (hasta ^D)
	{   		
		printf("COMMAND->");
		fflush(stdout); // Fuerza muestrar mensaje
		get_command(inputBuffer, MAX_LINE, args, &background);  /*  Lee comando del usuario*/

        parse_redirections(args, &file_in, &file_out);     // detectar si hay > o < (redirecciones) en el comando. y guarda en file_in y/o file_out los nombres de los archivos.
		
		if(args[0]==NULL) continue;   // Si no se escribio nada, continue vuelve la while

        if(!strcmp(args[0], "cd")){   // compara args y cd (0 si son iguales), y pone !0 para q sea true y siga en el if
            chdir(args[1]);
            continue;
        }
        if(!strcmp(args[0], "logout")){
            exit(0);                // termina el programa

        }
        if(!strcmp(args[0], "jobs")){   // imprime la lista de tareas en segundo plano y suspendidas
            block_SIGCHLD();
            print_job_list(tareas);
            unblock_SIGCHLD();
            if (empty_list(tareas)) {
                printf("No hay tareas en segundo plano ni suspendidas\n");
            }
            continue;              

        }
        if(!strcmp(args[0], "bg")){     // reanuda un proceso detenido(ctrl+Z), pero deja q se ejecute en segundo plano
            block_SIGCHLD();
            int pos = 1;            // empieza en 1
            if (args[1] != NULL){   // introducio un num
                pos = atoi(args[1]);  // convierte args[1] (string) a un entero. pos es lo q introdujo el usuario
            }
            item = get_item_bypos(tareas, pos);     // asegura pos correcta. y saca el item (que es un puntero a job)
            if ((item != NULL) && (item->state == STOPPED)){  
                item->state = BACKGROUND;             // cambiamos estado a background
                killpg(item->pgid, SIGCONT);         //  sigcont (señal q reanuda proceso detenido). killpg (kill process group). Reanuda todos los procesos del grupo que fueron detenidos
                printf("Reanudando en segundo plano: pid %d, comando %s\n", item->pgid, item->command);
            } else {
                printf("No hay tarea suspendida en esa posición.\n");
            }
            unblock_SIGCHLD();
            continue;

        }
        if(!strcmp(args[0], "fg")){   // pone en primer plano una tarea q estaba en segundo plano o suspendida
            block_SIGCHLD();
            int pos = 1;         
            primerplano = 1;    // true (indica q cuando llega a la parte del padre hay ya un proceso en primer plano)  
            if (args[1] != NULL){   
                pos = atoi(args[1]);  
            }
            item = get_item_bypos(tareas, pos);     
            if(item != NULL){          // solo chequeamos eso pq en la lista solo guardamos los q estan en background o suspendidos
                set_terminal(item->pgid);             // ceder terminal 
                if (item->state == SUSPENDED){
                    killpg(item->pgid, SIGCONT);       // señal q reanuda proceso detenido
                }
                pid_fork = item->pgid;                //  el nuevo hijo es el item, q se usa para todo lo e abajo con el padre
                delete_job(tareas, item);             // elimina o borra item de la lista pq va a primerplano, no hace falta actualizar estado a foreground 
            } else {
                printf("No hay tarea en esa posición para primer plano.\n");
            }
            unblock_SIGCHLD();                     // desbloquea y no pone continue para que siga con la parte del padre  
        }    

		/**
         * Pasos:
         * (1) Creamos un proceso hijo con fork()
         * (2) El hijo ejecuta el comando con execvp()
         * (3) Si es foreground(== 0), el padre espera; si es background(== 1), sigue sin esperar
         * (4) El shell muestra el estado del comando ejecutado
         * (5) Vuelve a pedir otro comando al usuario
        */

        if (!primerplano){   // == 0 (false). si no hay procesos en primerplano hace el fork, y si hay no lo hace
            pid_fork = fork();    // crea una copia exacta (hijo) 
        }
        
        if (pid_fork > 0){       // padre. hace las cosas o sobre el pid_fork de arriba o sobre el q esta en fg (item)

            if (background == 0){   // foreground (==0)      // wuntraced -> suspendido (en waitpid para saber)

                waitpid(pid_fork, &status, WUNTRACED);      // espera a q el hijo cambie de estado y recoge su pid, los punteros(*) los pasamos por direccion(&) 
                set_terminal(getpid());                       // shell vuelve a tomar el control del terminal
                status_res = analyze_status(status, &info);    // almacena el estado en el q termino

                if (status_res == SUSPENDED){                  // maneja estados -> suspendido
                    block_SIGCHLD();
                    item = new_job(pid_fork, args[0], STOPPED);  // crea un nuevo job
                    add_job(tareas, item);                          // mete job a la lista
                    unblock_SIGCHLD();
                    printf("\nForeground pid: %d, command: %s, %s, info: %d\n", 
                    pid_fork, args[0], status_strings[status_res], info);   // status_strings muestra estado

                } else if (status_res == EXITED){
                    if(info != 255){    // si no es comando de error imprime, sino se termina
                    printf("\nForeground pid: %d, command: %s, %s, info: %d\n", 
                    pid_fork, args[0], status_strings[status_res], info);
                    } 
                }
                primerplano = 0;      // ya no hay procesos en primer plano (lo ponemos en 0)


            } else {    // Background (==1)
                block_SIGCHLD();
                item = new_job(pid_fork, args[0], BACKGROUND);  // crea un nuevo job
                add_job(tareas, item);                          // mete job a la lista
                unblock_SIGCHLD();
                printf("\nBackground job running... pid: %d, command: %s\n", pid_fork, args[0]); // %s (string cualquier), %d (decimal)
            }

        } else {                // hijo == 0 -> lanza los comandos
            new_process_group(getpid());   // crear grupo de procesos para q el hijo reciba senales sin afectar al shell y pueda gestionarse en back y fore
            // pid_fork es el PID del hijo visto desde el padre (dentro del hijo pid_fork vale 0). Dentro del hijo, getpid() devuelve el PID del hijo.
            if (background == 0){   // foreground
                set_terminal(getpid());  // toma control del terminal
            } 

            // Redirección de entrada (stdin)  " < archivo"
            if (file_in != NULL) {
                FILE *f = fopen(file_in, "r");        // fopen(...) -> Abre el archivo en modo lectura 
                if (f != NULL) {
                    dup2(fileno(f), STDIN_FILENO);  // duplica el descriptor sobre stdin. "todo lo que se lea por stdin (como scanf, cat, o wc) va a salir del archivo file_in en lugar del teclado"
                    fclose(f);  // ya no se necesita el FILE*
                } else {
                    perror("Error al abrir archivo de entrada");
                    exit(EXIT_FAILURE);  // si falla, aborta el hijo
                }
            }

            // Redirección de salida (stdout)  " > archivo".  (muestra "   " leyendo del archivo)
            if (file_out != NULL) {
                FILE *f = fopen(file_out, "w");      // fopen(...) -> Abre el archivo en modo escritura (crea o sobreescribe)
                if (f != NULL) {
                    dup2(fileno(f), STDOUT_FILENO);  // duplica sobre stdout. “Todo lo que se imprima por stdout, mandalo al archivo abierto”
                    fclose(f);
                } else {
                    perror("Error al abrir archivo de salida");
                    exit(EXIT_FAILURE);
                }
            }

            // printf("Hijo listo, pid: %d, background: %d\n", getpid(), background);
            restore_terminal_signals();  // permite que el hijo reciba señales como Ctrl+C
            execvp(args[0], args);     // cargar la libreria y sustituye codigo del hijo con el comando q le pasamos, args[0] es el comando, args (opciones)
            printf("\nError. comando %s no encontrado\n", args[0]);
            exit(-1);     // exit del error es -1 -> info = 255
        }


	} /* End while */
}


// error de violacion de segmento → fuimos a alguna parte en memoria q no deberiamos ir



// --------  TEST  ----------------------------

// (base) Air-de-Hugo:proyecto_shell_esqueleto hugodiazo$ ./shell

// COMMAND->ls
// flujo-padre-hijo.png    frutas.txt              job_control.h           shell
// flujo-senal-zombie.png  job_control.c           resultado.txt           shell.c
// Foreground pid: 940, command: ls, Exited, info: 0

// COMMAND->sleep 5
// Foreground pid: 1072, command: sleep, Exited, info: 0

// COMMAND->sleep 10 &
// Background job running... pid: 1380, command: sleep

// COMMAND->jobs
// Contents of tareas:
//  [1] pid: 1380, command: sleep, state: Background

// COMMAND->
// Background pid: 1380, command: sleep, Exited, info: 0

// COMMAND->sleep 50  
// ^Z
// Foreground pid: 1897, command: sleep, Suspended, info: 18

// COMMAND->jobs
// Contents of tareas:
//  [1] pid: 1897, command: sleep, state: Stopped

// COMMAND->bg 1
// Reanudando en segundo plano: pid 1897, comando sleep

// COMMAND->jobs
// Contents of tareas:
//  [1] pid: 1897, command: sleep, state: Background

// COMMAND->fg 1
// Foreground pid: 1897, command: fg, Exited, info: 0

// COMMAND->jobs
// Contents of tareas:
// No hay tareas en segundo plano ni suspendidas

// COMMAND->sleep 20
// ^CCOMMAND->jobs
// Contents of tareas:
// No hay tareas en segundo plano ni suspendidas
// COMMAND->logout