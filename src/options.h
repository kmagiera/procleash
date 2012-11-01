/**
 * @file options.h
 * @version 1.0
 * @author Krzysztof Magiera <magiera@student.agh.edu.pl>
 * 
 * Biblioteka options wspiera przetwarzanie argumentow
 * przekazanych ze srodowiska do naszego programu.
 * 
 * Argumenty dzielimy na dwa podstawowe typy:
 *   * argumenty dlugie (takie po podwujnej kresce np: --order)
 *   * argumenty krotkie (wystepuja po pojedynczej kresce '-a'
 * 		lub w ciagu argumentow '-avf')
 * 
 * Dzieki tej bibliotece mozemy wylawiac argumenty krotkie i ich
 * wartosci. Mozemy takze wylapywac argumenty prefiksowe tj. takie
 * w ktorych znamy tylko poczatek. Na przyklad jesli argument
 * '--with-' oznaczymy jako prefixowy, to po podaniu --with-mod
 * lub --with-user jako wartosci tych argumentow otrzymamy 'mod' i 'user'
 * 
 */
#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include <sys/types.h>
#include <stdint.h>

enum option_event {
	
	OE_ESSENTIAL = 1,	/*!< argument jest wymagany		*/
	OE_VALUE	 = 2,	/*!< argument moze miec wartosc	*/
	OE_FVALUE	 = 4,	/*!< argument musi posiadać wartosc */
	OE_PREFIX	 = 8	/*!< podany argument moze byc traktowany jako prefix */
	
};

enum option_revent {
	
	OR_EXISTS	 = 1,	/*!< argument pojawil sie w liscie argumentow */
	OR_HASVALUE	 = 2,	/*!< argument posiada wartosc */
	OR_NOTFOUND	 = 4,	/*!< argument nie zostal odnaleziony w liscie */
	OR_NOVALUE	 = 8,	/*!< argument nie posiada wartosci */
	OR_POSTFIX	 = 16	/*!< wartosc jest postfixem argumentu */
	
};

enum option_errors {
	
	OE_ARGABSENT,		/*!< gdy ktorys z wymaganych argumentow nie byl obecny */
	OE_ARGUNVALUED		/*!< gdy ktorys z argumentow z wymagana wartoscia nie mial wartosci */
	
};

enum option_type {
	
	OT_SHORT,			/*!< argument krotki */
	OT_LONG				/*!< argument dlugi */
	
};

/**
 * Reprezentuje argumenty. Argumenty moga byc dlugie (takie po podwujnej
 * kresce) lub krotkie (takie po pojedynczej kresce)
 * Pola:
 * argtype	- OT_LONG dla argumentow dlugich, OT_SHORT dla krotkich
 * argument	- dla krotkich jest tam pojedynczy znak, dla dlugich
 * 			  lancuch znakow
 * event	- flaga ze zdarzeniami (zdarzenia z option_event)
 * revent	- pole z wynikami przetowrzenia (opcjie z option_revent)
 * value	- wartosc parametru
 */
struct option_arg {
	
	uint8_t		argtype;/*!< typ argumentu (dlugi albo krotki) */
	
	union {
		uint8_t		arg;	/*!< argument krotki - jedna literka */
		const char*	strarg;	/*!< argument dlugi */
	} argument;			/*!< argument */ 
	
	uint8_t		event;	/*!< akcja ktorej oczekujemy na argumencie */
	uint8_t		revent;	/*!< wynik */
	char		*value;	/*!< wartosc parametru, lub postfix wyrazenia */
	
};

/**
 * Przetwarza argumenty ze standardowego wektora argumentow
 * do tablicy struktur option_arg
 * @param argv standardowy wektor argumentow
 * @param argc dlugosc wektora argumentow
 * @param args wektor ze strukturami option_arg
 * @param argspsiz ilosc danych w wektorze args
 * @param argsvsiz ilosc miejsca w wektorze args, ktore bedzie zapelniane
 * dodatkowymi argumentami prefiksowymi (jesli pojawia sie wiecej niz raz)
 * argument przekazany pod argsvsiz zmieni swoja wielkosc na calkowita
 * zapelniona wielkosc wektora args
 * @param unrec wekrot z argumentami nierozpoznanymi
 * @param unrecsiz parametr-wynik - wielkosc wektora argumentow nierozpoznanych
 * @return zwraca 0 jesli wszystko przebiegnie poprawnie lub jedna
 * z wartosci z option_errors w przypadku bledu
 */
int option_convert(char **argv,
				   size_t argc,
				   struct option_arg* args,
				   size_t argspsiz,
				   size_t* argsvsiz,
				   char **unrec,
				   size_t* unrecsiz);

/**
 * Zwraca argument, dla ktorego wystapil blad przy przetwarzaniu
 * @return wskaznik na blednie przetworzony argument
 */
struct option_arg* option_errarg();

#endif
