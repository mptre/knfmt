/*
 * Sense cpp macro alignment.
 */

#define CIRCQ_CONCAT(fst, snd) do {		\
	if (!CIRCQ_EMPTY(snd)) {		\
		(fst)->prev->next = (snd)->next;\
		(snd)->next->prev = (fst)->prev;\
		(snd)->prev->next = (fst);      \
		(fst)->prev = (snd)->prev;      \
		CIRCQ_INIT(snd);		\
	}					\
} while (0)
