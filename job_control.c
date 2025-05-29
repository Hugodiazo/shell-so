/**
 * Linux Control Job Shell Project
 * job_control module
 *
 * Operating Systems
 * Grados Ing. Informatica, Computadores & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "Operating System Concepts Essentials", Silberschatz et al.
 **/
#include "job_control.h"

// Lee el comando ingresado por el usuario y separa los argumentos. args como una cadena terminada en nulo.
void get_command(char inputBuffer[], int size, char *args[],int *background)
{
	int length, /* Cantidad de caracteres leidos desde teclado */
		i,      /* Indice para recorrer inputBuffer array */
		start,  /* Indice donde empieza un argumento */
		ct;     /* Indice de la posicion donde se guarda el proximo argumento en args[] */
		        // va contando cuantos argumentos ya pusimos en args

	ct = 0;
	*background=0;

	// Leemos lo que el usuario escribe por teclado
	length = read(STDIN_FILENO, inputBuffer, size);  

	start = -1;
	if (length == 0)
	{
		printf("\nBye\n");
		exit(0);            // Si el usuario escribe Ctrl+D, se termina el shell
	} 
	if (length < 0){
		perror("error reading the command");
		exit(-1);           // Error al leer del teclado
	}

	// Analizamos cada caracter que escribió el usuario
	int end = 0, iesc=0;
	for (i=0;i<length;i++) 
	{
		if (end) break;
        if (i>1 && i>iesc) inputBuffer[i-1-iesc] = inputBuffer[i-1];
		switch (inputBuffer[i])
		{
		case ' ':
		case '\t' :               /* Separador de argumentos */
			if(start != -1)
			{
				args[ct] = &inputBuffer[start];    /* Set up pointer */
				ct++;
			}
			inputBuffer[i] = '\0'; /* Ponemos caracter nulo; fin de string, corte */
			start = -1;                // terminamos ese argumento, así que reseteamos start
			inputBuffer[i-iesc] = '\0'; /* repetimos el corte */
			iesc = 0;
			break;
		case '#':                  /* Comentario */
            if (i>0 && '\\' == inputBuffer[i-1]){
                iesc++;            /* Escaped comment symbol */
			    if (start == -1) start = i;  // comienzo del nuevo argumento
                break;
            }
		case '\n':                 // Fin de línea
			if (start != -1)
			{
				args[ct] = &inputBuffer[start];     
				ct++;
			}
			inputBuffer[i] = '\0';
			args[ct] = NULL; 	   /* No hay mas argumentos para este comando */
			end = 1;
			break;
		default :             /* Otro caracter */
			if (inputBuffer[i] == '&')  // Si termina en '&', es background
			{
				*background  = 1;
				if (start != -1)
				{
					args[ct] = &inputBuffer[start];     
					ct++;
				}
				inputBuffer[i] = '\0';
				args[ct] = NULL; /* No hay mas argumentos para este comando */
				i=length; 		 /* Asegurar que el bucle for termine */
			}
			else if (start == -1) start = i;  /* Inicio de nuevo argumento */
		}  
	} 

	args[ct] = NULL; /* Por si la línea de entrada fuera > MAXLINE */
	if (i>1 && i>iesc) inputBuffer[i-1-iesc] = inputBuffer[i-1];
}


/**
 * Analizar los operadores de redirección '<' '>' una vez construida la estructura args.
 * Llamar a la función inmediatamente después de get_commad():
 *      ...
 *     while(...){
 *          // Shell main loop
 *          ...
 *          get_command(...);
 *          char *file_in, *file_out;
 *          parse_redirections(args, &file_in, &file_out);
 *          ...
 *     }
 *
 * Para una redirección válida, se requiere un espacio en blanco antes y después 
 * de los operadores de redirección '<' o '>'.
 **/
void parse_redirections(char **args,  char **file_in, char **file_out){
    *file_in = NULL;
    *file_out = NULL;
    char **args_start = args;
    while (*args) {
        int is_in = !strcmp(*args, "<");
        int is_out = !strcmp(*args, ">");
        if (is_in || is_out) {
            args++;
            if (*args){
                if (is_in)  *file_in = *args;
                if (is_out) *file_out = *args;
                char **aux = args + 1;
                while (*aux) {
                   *(aux-2) = *aux;
                   aux++;
                }
                *(aux-2) = NULL;
                args--;
            } else {
                /* Syntax error */
                fprintf(stderr, "syntax error in redirection\n");
                args_start[0] = NULL; // Do nothing
            }
        } else {
            args++;
        }
    }
    /* Debug:
     * *file_in && fprintf(stderr, "[parse_redirections] file_in='%s'\n", *file_in);
     * *file_out && fprintf(stderr, "[parse_redirections] file_out='%s'\n", *file_out);
	 */
}

/**
 * Devuelve un puntero a un elemento de lista con sus campos inicializados.
 * Devuelve NULL si falla la asignación de memoria.
 **/
// Crea un nuevo job y lo devuelve
job * new_job(pid_t pid, const char * command, enum job_state state)
{
	job * aux;
	aux=(job *) malloc(sizeof(job));
	if (!aux) return NULL;
	aux->pgid=pid;
	aux->state=state;
	aux->command=strdup(command);
	aux->next=NULL;
	return aux;
}

// Inserta un job al principio de la lista
void add_job (job * list, job * item)
{
	job * aux=list->next;
	list->next=item;
	item->next=aux;
	list->pgid++;

}

/**
 * Elimina de la lista el elemento pasado como segundo argumento.
 * Devuelve 0 si el elemento no existe.
 **/
// Elimina un job de la lista
int delete_job(job * list, job * item)
{
	job * aux=list;
	while(aux->next!= NULL && aux->next!= item) aux=aux->next;
	if(aux->next)
	{
		aux->next=item->next;
		free(item->command);
		free(item);
		list->pgid--;
		return 1;
	}
	else
		return 0;

}


// Busca un job (item) por su PID, devuelve null si no lo encuentra
job * get_item_bypid  (job * list, pid_t pid)
{
	job * aux=list;
	while(aux->next!= NULL && aux->next->pgid != pid) aux=aux->next;
	return aux->next;
}


// Busca un job por su posición (empieza en 1) (pq 0 se usa para almacenar el nombre y el número de items en la lista)
job * get_item_bypos( job * list, int n)
{
	job * aux=list;
	if(n<1 || n>list->pgid) return NULL;
	n--;
	while(aux->next!= NULL && n) { aux=aux->next; n--;}
	return aux->next;
}


// Muestra la información de un job(item): pid, command name and state
void print_item(job * item)
{
	printf("pid: %d, command: %s, state: %s\n", item->pgid, item->command, state_strings[item->state]);
}

// Recorre y muestra(print) todos los jobs(items) en la lista
void print_list(job * list, void (*print)(job *))
{
	int n=1;
	job * aux=list;
	printf("Contents of %s:\n",list->command);
	while(aux->next!= NULL) 
	{
		printf(" [%d] ",n);
		print(aux->next);
		n++;
		aux=aux->next;
	}
}


// Analiza el estado devuelto por waitpid
enum status analyze_status(int status, int *info)
{
	/* Suspended process */
	if (WIFSTOPPED (status))
	{
		*info=WSTOPSIG(status);
		return(SUSPENDED);
	}
	/* Continued process */
    else if (WIFCONTINUED(status))
    { 
        *info=0; 
        return(CONTINUED);
    }
    /* Terminated process by signal*/
	else if (WIFSIGNALED (status))
	{
		*info=WTERMSIG (status);
		return(SIGNALED);
	}
	/*Terminated process by exit */
	else if (WIFEXITED (status))
	{
		*info=WEXITSTATUS(status);
		return(EXITED);
	}
	/* Should never get here*/
	return -1;
}

// Cambia la accion predeterminada para las señales relacionadas con la terminal
void terminal_signals(void (*func) (int))
{
	signal (SIGINT,  func); /* crtl+c Interrumpir desde el teclado Termina */
	signal (SIGQUIT, func); /* ctrl+\ Salir desde el teclado Termina y volcado de memoria*/
	signal (SIGTSTP, func); /* crtl+z Detener la escritura en el teclado Suspende proceso en primer plano */
	signal (SIGTTIN, func); /* Background process intenta una entrada (leer) del terminal */
	signal (SIGTTOU, func); /* Background process intenta una salida (escribir) en el terminal */
}		

/**
 * Blocks or masks (enmascara) a signal.
 * La ejecucion del controlador de la señal se pospone hasta que se desbloquea.
 * Si la señal se ejecuta varias veces después de ser bloqueada, al desbloquearse, 
 * el controlador de esa señal se ejecuta solo una vez.
 **/
// Bloquea o desbloquea una señal (por ejemplo SIGCHLD)
void block_signal(int signal, int block)
{
	/* Declara e inicializa mascaras de señal */
	sigset_t block_sigchld;
	sigemptyset(&block_sigchld);
	sigaddset(&block_sigchld,signal);
	if(block)
	{
		/* Blocks signal */
		sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
	}
	else
	{
		/* Unblocks signal */
		sigprocmask(SIG_UNBLOCK, &block_sigchld, NULL);
	}
}
