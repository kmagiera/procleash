/**
 * @file procleash.h
 * @version 1.0
 * @author Krzysztof Magiera <magiera@student.agh.edu.pl>
 * 
 * Biblioteka procleash wspiera uruchamianie procesów "na smyczy"
 * tzn pod ściśle ustaloną ochroną.
 * 
 * Procesy uruchamiamy za pomocą funkcji procleash, przekazujac
 * odpowiednie parametry odnośnie programu, który uruchamiamy, oraz
 * smycz, która ustala to, co wolno, a czemu nie wolno procesowi
 * działającemu na niej.
 * 
 * Smyczy możemy określić
 *   * Czas trwania naszego programu
 *   * Pamiec wirtualna ktora mozemy mu przydzielic
 *   * Wielkosc stosu na ktorej moze operowac
 *   * Funkcje systemowe ktore moze wykonywac
 *   * Krotnosc wywolan funkcji systemowych
 *   * i wiele innych ;)
 * 
 */
#ifndef _PROCLEASH_H_
#define _PROCLEASH_H_

#include <sys/types.h>
#include <stdint.h>

#define SYSCALL_VECTOR_SIZE 340

struct procl_limits {
	
	uint32_t	time;	/*!< limit czasu wykonania (w sekundach) */
	uint32_t	vm;		/*!< limit pamieci wirtualnej (w bajtach) */
	uint32_t	stack;	/*!< limit stosu (w bajtach) */
	uint32_t	fout;	/*!< maksymalny rozmiar plikow, ktore proces moze utworzyc (w bajtach) */
	uint32_t	fds;	/*!< maksymalna ilosc otwartych deskryptorow */
	uint32_t	childs;	/*!< maksymalna liczba procesow, ktore proces moze utworzyc */
	
};

#define PL_UNLIMITED (1 << 31)

enum procl_flags {
	
	PLF_CLOSESTDIN = 1,		/*!< proces na smyczy bedzie mial zamkniete STDIN */
	PLF_CLOSESTDOUT = 1<<1,	/*!< proces na smyczy bedzie mial zamkniete STDOUT */
	PLF_CLOSESTDERR = 1<<2,	/*!< proces na smyczy bedzie mial zamkniete STDERR */
	PLF_TRYCONTINUE = 1<<3	/*!< nadzorca nie konczy procesu, gdy ten wywola niepoprawna funkcje systemowa */
	
};

enum procl_errors {
	
	PLE_FORK = -100,
	PLE_TIMELIMIT,
	PLE_MEMLIMIT,
	PLE_STACKLIMIT,
	PLE_ILLEGAL_SYSCALL,
	PLE_PREPARE,
	PLE_EXECVE,
	PLE_SIGNAL
};

/**
 * Smycz na ktorej mozemy wyprowadzac proces
 */
struct procl_leash {
	
	struct procl_limits	limits;							/*!< limity */
	uint32_t			syscalls[SYSCALL_VECTOR_SIZE];	/*!< tablica z dopuszczonymi funkcjami systemowymi */	
	uint32_t			flags;							/*!< dodatkowe flagi z procl_flags */
	int32_t				retcode;						/*!< kod wyjscia (ustawiany przez procleash) */
	
};

/**
 * Ustawia pozwolenie dla funkcji systemowej
 * @param syscall numer funkcji systemowej
 * @param times liczba zezwolonych wywolan (lub stala PL_UNLIMITED)
 * @param data smycz
 */
void procl_allow(int syscall, uint32_t times, struct procl_leash *data);

/**
 * Zabrania na wywolanie konkretnej funkcji systemowej
 * @param syscall numer funkcji ktora chcemy zabronic
 * @param data smycz
 */
void procl_deny(int syscall, struct procl_leash *data);

/**
 * Ustawia pozwolenie dla wszystkich funkcji systemowych.
 * Nalezy wykonac je przed uzyciem procleash (ewentualnie 
 * wykonac allowall). Funckja resetuje flagi na smyczy oraz 
 * wszystkie limity
 * @param data smycz 
 */
void procl_allowall(struct procl_leash *data);

/**
 * Zabrania na wykonywanie jakiejkolwiek funkcji systemowej
 * procesowi bedacemu na smyczy. Nalezy wykonac je przed uzyciem
 * procleash (ewentualnie wykonac allowall). Funckja resetuje flagi 
 * na smyczy oraz wszystkie limity
 * @param data smycz
 */
void procl_denyall(struct procl_leash *data);

/**
 * Uruchamia proces "na smyczy" kontrolujac wszytkie wywolania funkcji systemowych.
 * @param path sciezka pliku wykonywalnego
 * @param arg argumenty dla uruchamianego procesu (podobnie jak w funkcji execl)
 * @param data smycz
 * @return zwraca 0 jesli wszystko pojdzie wporzadku, jeden z bledow z procl_errors
 * lub liczbe dodatnia oznaczajaca numer funkcji systemowej na ktorej proces zostal
 * zatrzymany. W strukturze data zostaje zapisany kod wyjscia procesu
 */
int procleash(const char *path, char* const arg[], struct procl_leash *data);


#endif
