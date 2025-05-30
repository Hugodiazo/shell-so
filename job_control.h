/**
 * Linux Job Control Shell Project
 * Function prototypes, macros and type declarations for job_control module
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "Operating System Concepts Essentials", Silberschatz et al.
 **/
#ifndef _JOB_CONTROL_H
#define _JOB_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/wait.h>

// estados en proceso y tareas
enum status { SUSPENDED, SIGNALED, EXITED, CONTINUED};  // declara nombres asociado a numeros 
enum job_state { FOREGROUND, BACKGROUND, STOPPED };
// Strings para mostrar por pantalla
static char* status_strings[] = { "Suspended", "Signaled", "Exited", "Continued"};
static char* state_strings[] = { "Foreground", "Background", "Stopped" };

// Estructura que representa una tarea (job)
typedef struct job_   // item
{
	pid_t pgid; // ID de grupo de procesos
    char * command; // Nombre del comando
    enum job_state state; // Estado actual de la tarea
    struct job_ *next; // Apuntador al siguiente elemento de la lista
} job;


typedef job * job_iterator; // Para recorrer la lista de jobs

// Funciones publicas
void get_command(char inputBuffer[], int size, char *args[],int *background);
void parse_redirections(char **args,  char **file_in, char **file_out);
job * new_job(pid_t pid, const char * command, enum job_state state);
void add_job(job * list, job * item);
int delete_job(job * list, job * item);
job * get_item_bypid(job * list, pid_t pid);
job * get_item_bypos(job * list, int n);
enum status analyze_status(int status, int *info);


// Funciones privadas (mejor usarlas con macros)
void print_item(job * item);
void print_list(job * list, void (*print)(job *));
void terminal_signals(void (*func) (int));
void block_signal(int signal, int block);


// Macros publicos utiles para trabajar con la lista y las señales
#define list_size(list)    list->pgid     // Tamano de la lista
#define empty_list(list)   !(list->pgid)  // Verdadero si la lista esta vacia

#define new_list(name)     new_job(0,name,FOREGROUND)  // Crea una lista vacia

#define get_iterator(list)   list->next   // Devuelve el primer elemento real
#define has_next(iterator)   iterator     // Verifica si hay siguiente
#define next(iterator)       ({job_iterator old = iterator; iterator = iterator->next; old;})

#define print_job_list(list)   print_list(list, print_item)

#define restore_terminal_signals()  terminal_signals(SIG_DFL) // Restaura señales por defecto
#define ignore_terminal_signals() 	terminal_signals(SIG_IGN) // Ignora señales de terminal

#define set_terminal(pid)        tcsetpgrp (STDIN_FILENO,pid) // Asigna terminal a proceso
#define new_process_group(pid)   setpgid (pid, pid) // Crea nuevo grupo de procesos

#define block_SIGCHLD()   	 block_signal(SIGCHLD, 1) // Bloquea señal SIGCHLD
#define unblock_SIGCHLD() 	 block_signal(SIGCHLD, 0) // Desbloquea señal SIGCHLD

// Macro para debug (útil para imprimir valores)
#define debug(x,fmt) fprintf(stderr,"\"%s\":%u:%s(): --> %s= " #fmt " (%s)\n", __FILE__, __LINE__, __FUNCTION__, #x, x, #fmt)

#endif

