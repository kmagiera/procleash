#include "options.h"
#include <string.h>
#include <stdio.h>

/**
 * Zapamietany wskaznik na bledy obiekt
 */
struct option_arg *__option_err = NULL;

/**
 *
 */
struct option_arg* option_errarg() {
	return __option_err;
}

/**
 *
 */
int option_convert(char **argv,
				   size_t argc,
				   struct option_arg* args,
				   size_t argspsiz,
				   size_t* argsvsiz,
				   char **unrec,
				   size_t* unrecsiz) {

	/* mamy argv, argc. w args jest argpsiz szukanych argumentow
	 * do argvsiz wpiszemy ile znaleźliśmy */

	__option_err = NULL;

	size_t				it = 0, i = 0, j = 0, last = argspsiz, unreclast = 0;
	uint32_t			len = 0;
	char*				arg;
	struct option_arg*	valuefor = NULL;

	for (it = 0; it < argspsiz; it++) {

		args[it].revent = OR_NOTFOUND | OR_NOVALUE;
		args[it].value = NULL;

	}

	for (it = 1; it < argc; it++) {

		arg = argv[it];

		len = strlen(arg);

		if (len > 0 && arg[0] == '-') { // napewno argument

			valuefor = NULL; // kasujemy czekajacego na wartosc, bo teraz mamy argument

			if (len > 2 && arg[1] == '-') { // argument dlugi

				for (j = 0; j < argspsiz; j++) {

					if (args[j].argtype == OT_LONG) {

						if (args[j].event & OE_PREFIX) { // argument prefixowy

							i = strlen(args[j].argument.strarg);
							if (i < len-2 && strncmp(arg+2, args[j].argument.strarg, i) == 0) {
								// przypasowalo
								if (args[j].value == NULL) {
									// wolny, mozemy dodawac tutaj
									args[j].revent &= ~OR_NOTFOUND;
									args[j].revent &= ~OR_NOVALUE;
									args[j].revent |= OR_EXISTS | OR_HASVALUE;
									args[j].value = arg+2+i;
								}
								else if (last < *argsvsiz) {
									// musimy dodawac na koniec
									memcpy(args+last, args+j, sizeof(struct option_arg));
									args[last].value = arg+2+i;
									last++;
								}
								break;
							}

						}
						else if(strcmp(arg+2, args[j].argument.strarg) == 0) {
							// argument nieprefixowy
							args[j].revent &= ~OR_NOTFOUND;
							args[j].revent |= OR_EXISTS;
							valuefor = args+j;
							break;
						}

					}

				}

			}
			else if (len > 1 && arg[1] != '-') { // argument krotki

				for (i = 1; i < len; i++) {
					valuefor = NULL; // nadchodzi nastepny argument...
					for (j = 0; j < argspsiz; j++) {
						if (args[j].argtype == OT_SHORT &&
							args[j].argument.arg == arg[i]) {

							args[j].revent &= ~OR_NOTFOUND;	// wywalamy NOTFOUND jesli byla
							args[j].revent |= OR_EXISTS;	// dodajemy EXISTS
							valuefor = args+j;
							break;
						}
					}
				}

			}
			else {
				//same minusy..
				break;
			}

		}
		else if (valuefor != NULL && valuefor->event & (OE_VALUE | OE_FVALUE)) {
			// wartosc parametru

			valuefor->revent &= ~OR_NOVALUE;	// wywalamy NOVALUE jesli bylo
			valuefor->revent |= OR_HASVALUE;	// zaznaczamy flage wartosci
			valuefor->value = arg;

			valuefor = NULL; // odebralismy wartosc dla tego parametru, wiec kasujemy
		}
		else {	//wartosc inna

			valuefor = NULL; // kasujemy czekajacego na wartosc

			/* dodajemy do wektora nierozpoznanych */
			if (unreclast < *unrecsiz) {
				unrec[unreclast++] = arg;
			}

		}

	}

	/* jesli zostaly jeszcze jakies, to przekierowujemy je do nierozpoznanych */
	for (it++ ; it < argc; it++) {
		if (unreclast < *unrecsiz) {
			unrec[unreclast++] = argv[it];
		}
		else break;
	}

	/* ustaw NULL na koncu tablicy unrec */
	if (unreclast < *unrecsiz) {
		unrec[unreclast] = NULL;
	} else {
		unrec[*unrecsiz - 1] = NULL;
	}

	/* zapisujemy argumenty - wartosci */
	*unrecsiz = unreclast;
	*argsvsiz = last;

	i = 0;

	for (it = 0; it < last; it++) {

		if ((args[it].event & OE_ESSENTIAL) && (args[it].revent & OR_NOTFOUND)) {
			i = OE_ARGABSENT;
			__option_err = args+it;
			break;
		}
		else if ((args[it].event & OE_FVALUE) && (args[it].revent & OR_EXISTS) && (args[it].revent & OR_NOVALUE)) {
			i = OE_ARGUNVALUED;
			__option_err = args+it;
			break;
		}

	}

	return i;
}
