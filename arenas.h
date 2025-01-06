#ifndef ARENAS_H
#define ARENAS_H

struct arenas {
	struct arena	*eternal;
	struct arena	*scratch;
	struct arena	*doc;
	struct arena	*buffer;
	struct arena	*ruler;
};

void	arenas_init(struct arenas *);
void	arenas_free(struct arenas *);

#endif /* !ARENAS_H */
