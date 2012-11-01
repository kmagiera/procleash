/**
 * @file procleash.c
 * @version 1.0
 * @author Krzysztof Magiera <magiera@student.agh.edu.pl>
 * 
 * PROCLEASH
 * 
 * Biblioteka do uruchamiania plikow na smyczy
 * 
 */
#include <stdio.h>
#include <unistd.h>
#include <linux/user.h>
#include <linux/unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>

/* dla procesorow 64 bitowych numer funkcji systemowej znajduje sie w orig_rax
 * a dla i386 w orig_eax */
# if defined __x86_64__
#  define ORIG_REG orig_rax
# elif defined __i386__
#  define ORIG_REG orig_eax
# else
#  warning This machine appears to be neither x86_64 nor i386.
# endif

#include "procleash.h"

int procleash(const char *path, char* const arg[], struct procl_leash *data) {
	
	pid_t	proc;
	int		status;
	int		error = 0;
	struct user_regs_struct regs;
	
	if((proc = fork()) == 0) {
		
		if (data->flags & PLF_CLOSESTDIN)
			if (close(STDIN_FILENO) != 0)
				_exit(1);
		
		if (data->flags & PLF_CLOSESTDOUT)
			if (close(STDOUT_FILENO) != 0)
				_exit(1);
		
		if (data->flags & PLF_CLOSESTDERR)
			if (close(STDERR_FILENO) != 0)
				_exit(1);
		
		/* ustawiamy limit pamieci wirtualnej jesli byl wskazany */
		if (data->limits.vm != PL_UNLIMITED) {
			
			struct rlimit vmlimit = {
				.rlim_cur = data->limits.vm,
				.rlim_max = RLIM_INFINITY
			};
			
			if (setrlimit(RLIMIT_AS, &vmlimit) != 0) {
				_exit(PLE_PREPARE);
			}
			
		}
		
		/* ustawiamy limit na stos */
		if (data->limits.stack != PL_UNLIMITED) {
			
			struct rlimit stacklimit = {
				.rlim_cur = data->limits.stack,
				.rlim_max = RLIM_INFINITY
			};
			
			if (setrlimit(RLIMIT_STACK, &stacklimit) != 0) {
				_exit(PLE_PREPARE);
			}
			
		}
		
		/* limit wyjscia (laczna wielkosc wszystkich plikow po ktorych piszemy) */
		if (data->limits.fout != PL_UNLIMITED) {
			
			struct rlimit foutlimit = {
				.rlim_cur = data->limits.fout,
				.rlim_max = RLIM_INFINITY
			};
			
			if (setrlimit(RLIMIT_FSIZE, &foutlimit) != 0) {
				_exit(PLE_PREPARE);
			}
			
		}
		
		/* ustawiamy limit czasu wkonania (jesli byl wybrany) */
		if (data->limits.time != PL_UNLIMITED) {
			
			struct rlimit tmlimit = {
				.rlim_cur = data->limits.time,
				.rlim_max = RLIM_INFINITY 
			};
			
			if (setrlimit(RLIMIT_CPU, &tmlimit) != 0) {
				_exit(PLE_PREPARE);
			}
		}
		
		/* ten proces bedzie sledzony: */
		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) != 0) {
			_exit(PLE_PREPARE);
		}
		
		/* wywolujemy program */
		//execl(path, arg, NULL);
		execv(path, arg);
		
		/* nieudalo sie wywolac programu: */
		_exit(PLE_EXECVE);
		
	}
	else if (proc > 0) {
		
		/* dodajemy jedno wywolanie execl dla nas w watku sledzonym */
		if (data->syscalls[__NR_execve] != PL_UNLIMITED) 
			data->syscalls[__NR_execve]++;
		
		while(1) {
			
			/* czekamy na kolejne zatrzymanie przez ptrace */
			if (waitpid(proc,&status,0) < 0) {
				return PLE_PREPARE;
			}
			
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
				/* proces sledzony zostal wstrzymany sygnalem SIGTRAP, czyli to pewnie ptrace */
				
				/* pobieramy wartosci rejestrow */
				if (ptrace(PTRACE_GETREGS, proc, NULL, &regs) != 0) {
					return 3;
				}
				
				//printf("call %d eax %d ebx %d\n",regs.ORIG_REG, regs.eax, regs.ebx);
				
				/* numer funkcji systemowej idacej w przerwaniu jest w rejestrze eax */
				if (data->syscalls[regs.ORIG_REG] == 0) {
					/* polecenie nie moze byc wykonane */
					error = regs.ORIG_REG;
					
					if (data->flags & PLF_TRYCONTINUE)
						regs.ORIG_REG = 0;
					else
						regs.ORIG_REG = __NR_exit_group;
				}
				else if (data->syscalls[regs.ORIG_REG] > 0) {
					
					/* kasujemy jedno wywolanie */
					data->syscalls[regs.ORIG_REG]--;
					
				}
				
				//getchar();
				
				/* wysylamy "poprawione" wartosci user_regs */
				if (ptrace(PTRACE_SETREGS, proc, NULL, &regs) != 0) {
					return PLE_PREPARE;
				}
				
				/* puszczamy proces sledzony dalej */
				if (ptrace(PTRACE_SYSCALL, proc, NULL, NULL) != 0) {
					return PLE_PREPARE;
				}
				
			}
			else {
				
				/* proces zakonczyl prace lub zatrzymal sie z jakiegos innego powodu */
				if (WIFEXITED(status)) {
					// wstrzymany przez nadzorce, albo zakonczony poprawnie
					data->retcode = WEXITSTATUS(status);
				}
				else if (WIFSTOPPED(status)) {
					// zatrzymany przez interesujacy sygnal
					if (WSTOPSIG(status) == SIGXCPU) {
						return PLE_TIMELIMIT;
					}
					else if (WSTOPSIG(status) == SIGXFSZ) {
						return PLE_MEMLIMIT;
					}
					else if (WSTOPSIG(status) == SIGSEGV) {
						return PLE_STACKLIMIT;
					}
					else {
						/* jakis inny sygnal nas zatrzymal */
						continue;
					}
					/* limit przekroczony, a wiec ubijamy */
					if (kill(proc, SIGKILL) != 0) {
						// moze byc ESRCH
					}
					if (waitpid(proc, &status, 0) < 0) { /* bo mogl byc juz wczesniej zabity */ }
					data->retcode = WEXITSTATUS(status);
					
				}
				else {
					/* nie moze sie wcisnac z kodem w limit pamieci */
					return PLE_MEMLIMIT;
				}
				
				/* proces sledzony zakonczony */
				break;
			}
		}
	}
	else {
		/* nie moze wykonac fork'a */
		return PLE_FORK;
	}
	
	return error;
}

void procl_reset_limits(struct procl_leash *data) {
	
	data->limits.childs = PL_UNLIMITED;
	data->limits.fds = PL_UNLIMITED;
	data->limits.fout = PL_UNLIMITED;
	data->limits.stack = PL_UNLIMITED;
	data->limits.time = PL_UNLIMITED;
	data->limits.vm = PL_UNLIMITED;
	
}

void procl_allow(int syscall, uint32_t times, struct procl_leash *data) {
	if (syscall < 0 || syscall >= SYSCALL_VECTOR_SIZE) return;
	data->syscalls[syscall] = times;
}

void procl_deny(int syscall, struct procl_leash *data) {
	procl_allow(syscall, 0, data);
}

void procl_allowall(struct procl_leash *data) {
	
	procl_reset_limits(data);
	
	int i;
	for (i = 0; i < SYSCALL_VECTOR_SIZE; i++) {
		procl_allow(i, PL_UNLIMITED, data);
	}
	
}

void procl_denyall(struct procl_leash *data) {
	
	procl_reset_limits(data);
	
	int i;
	for (i = 0; i < SYSCALL_VECTOR_SIZE; i++) {
		procl_allow(i, 0, data);
	}
	
}
