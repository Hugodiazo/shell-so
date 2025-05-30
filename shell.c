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


    job *item;   // declara para poder usar en new y add job

    ignore_terminal_signals();   // ignora senales
    // SIGCHLD → lanza señal al padre cuando el hijo termina o se suspende (creo)
    signal(SIGCHLD, manejador);  //  es como un trycatch que maneja las señales de los hijos en la lista
    tareas = new_list("tareas");   // crear lista

	while (1)   // Bucle principal del shell, se ejecuta indefinidamente, (hasta ^D)
	{   		
		printf("COMMAND->");
		fflush(stdout); // Fuerza muestrar mensaje
		get_command(inputBuffer, MAX_LINE, args, &background);  /*  Lee comando del usuario*/
		
		if(args[0]==NULL) continue;   // Si no se escribio nada, continue vuelve la while

        if(!strcmp(args[0], "cd")){   // compara args y cd (0 si son iguales), y pone !0 para q sea true y siga en el if
            chdir(args[1]);
            continue;
        }
        if(!strcmp(args[0], "logout")){
            exit(0);                // termina el programa
        }

		/**
         * Pasos:
         * (1) Creamos un proceso hijo con fork()
         * (2) El hijo ejecuta el comando con execvp()
         * (3) Si es foreground(== 0), el padre espera; si es background(== 1), sigue sin esperar
         * (4) El shell muestra el estado del comando ejecutado
         * (5) Vuelve a pedir otro comando al usuario
        */

        pid_fork = fork();    // crea una copia exacta (hijo) 

        if (pid_fork > 0){       // padre

            if (background == 0){   // foreground (==0)      // wuntraced -> suspendido (en waitpid para saber)

                waitpid(pid_fork, &status, WUNTRACED);      // espera a q el hijo cambie de estado y recoge su pid, los punteros(*) los pasamos por direccion(&) 
                set_terminal(getpid());                       // shell vuelve a tomar el control del terminal
                status_res = analyze_status(status, &info);    // almacena el estado en el q termino
                if (status_res == SUSPENDED){                  // maneja estados -> suspendido
                    block_SIGCHLD();
                    item = new_job(pid_fork, args[0], FOREGROUND);  // crea un nuevo job
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
            restore_terminal_signals();  // permite que el hijo reciba señales como Ctrl+C
            execvp(args[0], args);     // cargar la libreria y sustituye codigo del hijo con el comando q le pasamos, args[0] es el comando, args (opciones)
            printf("\nError. comando %s no encontrado\n", args[0]);
            exit(-1);     // exit del error es -1 -> info = 255
        }


	} /* End while */
}


// error de violacion de segmento → fuimos a alguna parte en memoria q no deberiamos ir