/**
 * @file main.c
 * @version 1.0
 * @author Krzysztof Magiera <magiera@student.agh.edu.pl>
 *
 * Tool do obslugi biblioteki procleash - uruchamiania programów
 * na smyczy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "options.h"
#include "procleash.h"

struct option_arg args[] = {

	{OT_SHORT, 'h', 0},								// h -- wyswietla pomoc
	{OT_LONG, .argument.strarg = "version",0},		// version -- wyswietla informacje o wersji
	{OT_SHORT, 'v', 0},								// v -- tryb gadatliwy
	{OT_SHORT, 'f', OE_FVALUE},						// f file -- plik z konfiguracja
	{OT_SHORT, 't', OE_FVALUE},						// t sekundy -- limit czasu
	{OT_SHORT, 'm', OE_FVALUE}, 					// m bajty -- limit pamieci wirtualnej
	{OT_SHORT, 's', OE_FVALUE},						// s bajty -- limit wielkosci stosu
	{OT_SHORT, 'o', OE_FVALUE},						// o bajty -- limit wielkosci wygenerowanych plikow
	{OT_LONG, .argument.strarg = "deny-allow",0},	// deny-allow -- ustawia poczatkowa kolejnosc deny-allow
	{OT_LONG, .argument.strarg = "allow-deny",0},	// allow-deny -- ustawia poczatkowa kolejnosc allow-deny (domyslnie)
	{OT_LONG, .argument.strarg = "deny-",
		OE_FVALUE | OE_PREFIX},						// deny-syscall -- deaktywuje niektore syscalle
	{OT_LONG, .argument.strarg = "allow-",
		OE_FVALUE | OE_PREFIX},						// allow-syscall[,times] -- aktywuje niektore syscalle
	{OT_LONG, .argument.strarg = "allow=",
		OE_FVALUE | OE_PREFIX},						// allow=syscall[,times] -- zezwol na funkcje syscall times razy
	{OT_LONG, .argument.strarg = "deny=",
		OE_FVALUE | OE_PREFIX},						// deny=syscal -- zabrania na uzycie syscall
	{},{},{},{},{},{},{},{},{},{}					// puste miejsce na komendy

};

/**
 * Syscalle dostepne z nazwy
 */
const struct syscall {
	char*	name;
	int		number;

} syscalls[] = {
	{"fork",		__NR_fork},
	{"vfork", 		__NR_vfork},
	{"read",		__NR_read},
	{"write", 		__NR_write},
	{"open",		__NR_open},
	{"close",		__NR_close},
	{"unlink",		__NR_unlink},
	{"execve",		__NR_execve},
	{"kill",		__NR_kill},
	//{"sigaction",	__NR_sigaction},
	//{"signal",		__NR_signal},
	{"fcntl",		__NR_fcntl},
	//{"socketcall",	__NR_socketcall},
	{"fstat",		__NR_fstat},
	{"flock",		__NR_flock}
};

void display_help();
void display_version();
void a_configure(struct option_arg *argv, size_t argc, struct procl_leash *leash);
void f_configure(const char *fname, struct procl_leash *leash);

/**
 *
 */
int main(int argc, char **argv) {

	size_t	csiz = sizeof(args)/sizeof(struct option_arg), psiz = 20;
	char*		pargv[20]; // wektor argumentow dla procesu strzezonego

	if (option_convert(argv, argc, args, csiz-10, &csiz, pargv, &psiz) == OE_ARGUNVALUED) {
		// ktoras z wartosci nie byla przedstawiona
		struct option_arg *err = option_errarg();
		if (err->argtype == OT_SHORT) {
			fprintf(stderr, "Argument -%c wymaga wartości.\nUżyj %s -h, aby zobaczyc pomoc\n",
							err->argument.arg, argv[0]);
		}
		else {
			fprintf(stderr, "Argument --%s wymaga wartości.\nUżyj %s -h, aby zobaczyc pomoc\n",
							err->argument.strarg, argv[0]);
		}
		exit(EXIT_FAILURE);
	}

	/* przetwarzamy help i version */
	if (args[0].revent & OR_EXISTS)
		display_help(argv[0]);
	if (args[1].revent & OR_EXISTS)
		display_version();

	/* sprawdzamy, czy mamy co uruchamiac */
	struct stat tmp;
	if (psiz == 0 || stat(pargv[0], &tmp) != 0) {
		if (psiz == 0)
			fprintf(stderr, "Nie podano pliku\nUżyj %s -h, aby zobaczyć pomoc\n", argv[0]);
		else
			fprintf(stderr, "Nie można odnaleźć pliku %s\nUżyj %s -h, aby zobaczyć pomoc\n",
						pargv[0], argv[0]);
		exit(EXIT_FAILURE);
	}

	/* przetwarzamy verbose */
	int verbose = 0;
	if (args[2].revent & OR_EXISTS)
		verbose = 1;

	/* tworzymy nowa smycz */
	struct procl_leash leash;

	/* domyslnie kolejnosc allow-deny */
	procl_allowall(&leash);

	/* jesli jest -f, to czytamy z pliku, jesli nie, to z argumentow */
	if (args[3].revent & OR_EXISTS) {
		f_configure(args[3].value, &leash);
	}
	a_configure(args, csiz, &leash);

	/* odpalamy program */
	int ret = 0;
	if ((ret = procleash(pargv[0], pargv, &leash)) != 0) {
		/* bledy... */
		if (verbose) {
			if (ret > 0) {
				fprintf(stderr, "BŁĄD: Użycie funkcji systemowej %d\n", ret);
				ret = PLE_ILLEGAL_SYSCALL;
			}
			else {
				char *err;
				switch(ret) {
					case PLE_TIMELIMIT:
						err = "Limit czasu przekroczony";
						break;
					case PLE_MEMLIMIT:
						err = "Limit pamięci przekroczony";
						break;
					case PLE_SIGNAL:
						err = "Odebrano nieznany sygnał";
						break;
					case PLE_STACKLIMIT:
						err = "Limit stosu przekroczony";
						break;
					case PLE_EXECVE:
						err = "Nie mozna uruchomic programu";
						break;
					default:
						err = "Błąd krytyczny";
				}
				fprintf(stderr, "BŁĄD: %s\n", err);
			}
		}
	}
	exit(ret);

}

/**
 * Rozpoznaje numer funkcji systemowej po nazwie
 *
 */
int syscall_toi(const char *syscall) {

	int i, len = sizeof(syscalls)/sizeof(struct syscall);
	for (i = 0; i < len; i++) {
		if (strcmp(syscalls[i].name, syscall) == 0) {
			return syscalls[i].number;
		}
	}
	return -1;
}

/**
 * Czyta konfiguracje ze struktury argumentow
 */
void a_configure(struct option_arg *argv, size_t argc, struct procl_leash *leash) {

	/* sprawdzamy, czy ktos chce zmieniac order */
	if (args[8].revent & OR_EXISTS)
		procl_denyall(leash);

	/* przetwarzamy limity */
	if (argv[4].revent & OR_HASVALUE) {
		leash->limits.time = atoi(argv[4].value);
	}
	if (argv[5].revent & OR_HASVALUE) {
		leash->limits.vm = atoi(argv[5].value);
	}
	if (argv[6].revent & OR_HASVALUE) {
		leash->limits.stack = atoi(argv[6].value);
	}
	if (argv[7].revent & OR_HASVALUE) {
		leash->limits.fout = atoi(argv[7].value);
	}

	/* przetwazamy pozostale */
	int i;
	char *token;
	uint32_t times, callnum;
	for(i = 0; i < argc; i++) {
		//if (argv[i].argtype == OT_LONG) printf("analizuje %s\n", argv[i].argument.strarg);
		if (argv[i].argtype != OT_LONG || (argv[i].revent & OR_NOVALUE)) continue;

		callnum = -1;
		times = PL_UNLIMITED;

		if (strncmp(argv[i].argument.strarg, "allow=", 6) == 0) {
			token = strtok(argv[i].value, ",");
			if (token != NULL) {
				callnum = atoi(token);
			}
			token = strtok(NULL, ",");
			if (token != NULL) {
				times = atoi(token);
			}
			procl_allow(callnum, times, leash);

		}

		else if (strncmp(argv[i].argument.strarg, "allow-", 6) == 0) {
			token = strtok(argv[i].value, ",");
			if (token != NULL) {
				callnum = syscall_toi(token);
			}
			token = strtok(NULL, ",");
			if (token != NULL) {
				times = atoi(token);
			}
			procl_allow(callnum, times, leash);

		}

		else if (strncmp(argv[i].argument.strarg, "deny=", 6) == 0) {
			callnum = atoi(argv[i].value);
			procl_deny(callnum, leash);

		}

		else if (strncmp(argv[i].argument.strarg, "deny-", 6) == 0) {
			callnum = syscall_toi(argv[i].value);
			procl_deny(callnum, leash);
		}

	}

}

/**
 * Czyta konfiguracje z pliku.
 * Plik konfiguracyjny moze skladac sie z nastepujacych elementow:
 *
 * # - na poczatku linii oznacza, ze dana linia nie jest interpretowana
 *
 * order deny-allow - oznacza, ze przyjmujemy kolejnosc deny-allow
 *
 * allow name [times] - zezwalamy funkcje systemowa name times razy
 *
 * allow num [times] - podobnie, ale podajemy numer funkcji systemowej
 *
 * deny num - zabraniamy funkcje o danym numerze
 *
 * deny name - zabraniamy funkcje identyfikowana przez nazwe name
 *
 * timelimit num - limit czasu
 *
 * stacklimit num - limit na stos
 *
 * outputlimit num - limit na wyjscie
 *
 * memlimit num - limit czasu
 *
 */
void f_configure(const char *fname, struct procl_leash *leash) {

	FILE *conf;
	char buf[30], str[30];
	int syscall, times, line = 0;

	if ((conf = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "Nie można otworzyć pliku z konfiguracją (%s)\n", fname);
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, 30, conf) != NULL) {

		line++;

		if (sscanf(buf, "%s", str) < 1) continue;

		if (str[0] == '#') continue; // komentarz

		if (strcmp("allow", str) == 0) {
			sscanf(buf, "%*s %s", str);
			syscall = syscall_toi(str);
			if (syscall < 0) syscall = atoi(str);
			if (sscanf(buf, "%*s %*s %d\n", &times) <= 0) {
				times = PL_UNLIMITED;
			}
			procl_allow(syscall, times, leash);
		}

		else if (strcmp("deny", str) == 0) {
			sscanf(buf, "%*s %s", str);
			syscall = syscall_toi(str);
			if (syscall < 0) syscall = atoi(str);
			procl_deny(syscall, leash);
		}

		else if (strcmp("order", str) == 0) {
			sscanf(buf, "%*s %s", str);
			if (strcmp(str, "deny-allow") == 0) {
				procl_denyall(leash);
			}
		}

		else if (strcmp("timelimit", str) == 0) {
			sscanf(buf, "%*s %d", &times);
			leash->limits.time = times;
		}

		else if (strcmp("memlimit", str) == 0) {
			sscanf(buf, "%*s %d", &times);
			leash->limits.vm = times;
		}

		else if (strcmp("stacklimit", str) == 0) {
			sscanf(buf, "%*s %d", &times);
			leash->limits.stack = times;
		}

		else if (strcmp("outputlimit", str) == 0) {
			sscanf(buf, "%*s %d", &times);
			leash->limits.fout = times;
		}

		else {
			fprintf(stderr, "Błąd w trakcie przetwarzania pliku konfiguracyjnego (%s:%d)\n",
					fname, line);
		}

	}

	fclose(conf);

}

/**
 * Wyświetla pomoc
 */
void display_help(char *pname) {

	static const char* help =
		"PROCLEASH (C) Krzysztof Magiera <magiera@student.agh.edu.pl>\n"
		"Użycie: %s [options] program [-- [program_arguments]]\n"
		"  Uruchamia program w odpowiednio skonfigurowanym środkowisku\n"
		"Informacje:\n"
		"  -h         - wyświetla tą pomoc i kończy\n"
		"  --version  - wyświetla informacje o wersji\n"
		"  -v         - uruchamia tryb gadatliwy\n"
		"Konfiguracja środowiska:\n"
		"  -f plik       - czyta konfiguracje z pliku\n"
		"  -t sec        - ustawia limit czasu wykonania procesu (w sekundach)\n"
		"  -m bytes      - ustawia limit pamieci wirtualnej (w bajtach)\n"
		"  -s bytes      - ustawia limit na stos (w bajtach)\n"
		"  -o bytes      - ustawia limit na wielkosc plikow generowanych\n"
		"  --deny-allow  - przed ustawianiem reguł dla funkcji systemowych\n"
		"                  wszystkie funkcje są zablokowane\n"
		"  --allow-deny  - przed ustawianiem reguł dla funkcji systemowych\n"
		"                  wszystkie funkcje są dostępne dla programu\n"
		"  --deny-<name> - blokuje funkcje o nazwie <name> (dostępne nazwy\n"
		"                  znajdują się poniżej\n"
		"  --allow-<name>,times"
		               " - udostępnia funkcje o nazwie <name> do wykonania\n"
		"                  times razy (jeśli nie podany times, funkcja będzie\n"
		"                  dostępna dowolną liczbę razy\n"
		"  --deny=<num>  - blokuje funkcje systemową o numerze num\n"
		"  --allow=<num>,times"
		               " - udostępnia funkcje o numerze num, na takich zasadach\n"
		"                  jak dla argumentu --allow-<name>\n"
		"Funkcje dotępne poprzez nazwy:\n"
		"%s\n"
		"Pełna lista numerów funkcji znajduje się w nagłówku <linux/unistd.h>\n";

	int scnames = 0, i, siz = sizeof(syscalls)/sizeof(struct syscall);
	for (i = 0; i < siz; i++) {
		scnames += strlen(syscalls[i].name) + 3;
	}

	char *schelp;
	if ((schelp = malloc(scnames+1)) == NULL) exit(EXIT_FAILURE);

	scnames = 0;
	for (i = 0; i < siz; i++) {
		sprintf(schelp+scnames, "  %s\n", syscalls[i].name);
		scnames += strlen(syscalls[i].name) + 3;
	}

	printf(help, pname, schelp);

	free(schelp);

	exit(EXIT_SUCCESS);
}

/**
 * Wyswietla informacje o wersji
 */
void display_version() {

	static const char* versioninfo =
		"PROCLEASH 1.0 (2008.01.19)\n";

	printf(versioninfo);

	exit(EXIT_SUCCESS);
}
