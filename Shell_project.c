/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include "string.h"
#include <unistd.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

job * tareas;

void manejador_sigchld(int sig){
	int status, info, pid_wait;	// DECLARO LAS VARIABLES
	enum status estado;		
	job* trabajo = tareas;
	job *aux = NULL;
	block_SIGCHLD();
	trabajo = trabajo->next;
	
	while (trabajo != NULL) {
		
		pid_wait = waitpid(trabajo->pgid, &status, WUNTRACED | WNOHANG);
		estado = analyze_status(status, &info);

		if (pid_wait == trabajo->pgid) {
			printf("\nwait realizado a proceso en background: %s, pid: %i\n",
					trabajo->command, trabajo->pgid);

			if (estado == EXITED  || estado==SIGNALED ) {
				aux = trabajo->next;
				delete_job(tareas,trabajo);
				free_job(trabajo);
				trabajo = aux;

			} else if (estado == SUSPENDED) {
				trabajo->state = STOPPED;
				trabajo = trabajo->next;

			}

		}else{
			trabajo=trabajo->next;
		}


	}

	unblock_SIGCHLD();
}

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */
	int res;
	ignore_terminal_signals();
	tareas = new_list("lista_trabajo");
	signal(SIGCHLD,manejador_sigchld);



	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command

		if(strcmp(args[0],"cd") == 0){		// COMANDO CD
			if (args[1] == NULL){			// SI CD "asecas" lleva al HOME
				chdir(getenv("HOME"));
			}else{					// Else te lleva a un lugar valido
				chdir(args[1]);
			}
			res = analyze_status(status,&info);
			if (info!=1){
				printf("Foreground pid: %i, command: %s, %s, info: %i \n",
				getpid(), args[0], status_strings[res], info); 
			}
			continue;
		}
		
		
		if(strcmp(args[0], "jobs") == 0){          // COMANDO JOBS
			if(list_size(tareas)==0){
				printf("Empty list \n");
			}else{
				print_job_list(tareas);
			}
			continue;
		}
		
				
		if(!strcmp(args[0],"bg")){		//COMANDO BG
			int posicion;
			job * aux;
			if(args[1]==NULL){		//Si args[1] no existe, cogemos la pos 1
				posicion=1;
			}else{
				posicion=atoi(args[1]);		//sino, la pos que le especifiquemos
			}
			aux = get_item_bypos(tareas,posicion);
			if(aux==NULL){
				printf("BG ERROR: JOB NOT FOUND \n");
			}else{
				if(aux->state==STOPPED){					
					aux->state=BACKGROUND;
					printf("Puesto en background el job %d que estaba suspendido, el job era: %s\n",posicion,aux->command);
					killpg(aux->pgid,SIGCONT);	
				}
			}
			continue;
		}
		
		if(!strcmp(args[0],"fg")){
			int posicion;
			enum status statusfg;
			job * aux;
			if(args[1]==NULL){		//Si args[1] no existe, cogemos la pos 1
				posicion=1;
			}else{				//sino, la pos que le especifiquemos
				posicion=atoi(args[1]);
			}
			aux=get_item_bypos(tareas,posicion);
			if(aux==NULL){
				printf("FG ERROR: JOB NOT FOUND \n");
				continue;
			}
			if(aux->state==STOPPED || aux->state==BACKGROUND){
					printf("Puesto en foreground el job %d que estaba suspendido o en background, el job era: %s\n",posicion,aux->command);
					aux->state=FOREGROUND;
					set_terminal(aux->pgid);	
					killpg(aux->pgid,SIGCONT); //manda una señal al grupo de proceso para que continue
					waitpid(aux->pgid,&status,WUNTRACED);
					set_terminal(getpid());
					statusfg=analyze_status(status,&info);
					if(statusfg==SUSPENDED){
						aux->state=STOPPED;
					}else{
						delete_job(tareas,aux);
					}
			}else{
					printf("El proceso no estaba en background o suspendido");
			}
			
			continue;
		}

		/* the steps are:
			(1) fork a child process using fork()
*/
			pid_fork = fork();
 /*
			(2) the child process will invoke execvp()*/
			
			if(pid_fork == -1){	//Fork erroneo
				printf ("Failed fork");
				continue;
			}
			
			if(pid_fork == 0){	// PROCESO HIJO
			
				new_process_group(getpid());
				restore_terminal_signals();
				execvp(args[0],args);
				printf("ERROR: command not found %s\n",args[0]);
				exit(EXIT_FAILURE);
			
			}else{			// PROCESO PADRE 
//			(3) if background == 0, the parent will wait, otherwise continue 

			new_process_group(pid_fork);

				if(background == 0){	 //EL PROCESO PADRE ESTA EN FOREGROUND
					set_terminal(pid_fork);
					pid_wait = waitpid(pid_fork,&status,WUNTRACED);
					set_terminal(getpid());
					res = analyze_status(status,&info);
					if(res==SUSPENDED){
						job *auxi2 = new_job(pid_fork,args[0],STOPPED);
						block_SIGCHLD();
						add_job(tareas,auxi2);
						unblock_SIGCHLD();
					}
					if (info!=1){
						printf("Foreground pid: %i, command: %s, %s, info: %i \n",
						pid_fork, args[0], status_strings[res], info);
					}
					


				}else{ 			 //EL PROCESO PADRE ESTA EN BACKGROUND
				
					job * aux = new_job(pid_fork,args[0],BACKGROUND);
					block_SIGCHLD();
					add_job(tareas,aux);
					unblock_SIGCHLD();
					
					if (info!=1){
						printf("Background job running... pid: %i, command: %s \n",
						pid_fork, args[0]); 

					}
				}
				

				
			}
			 
//			(4) Shell shows a status message for processed command 
			
			
				
//			 (5) loop returns to get_commnad() function
		

	} // end while
}
