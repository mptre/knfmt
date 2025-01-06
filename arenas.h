struct arenas {
	struct arena	*eternal;
	struct arena	*scratch;
	struct arena	*doc;
	struct arena	*buffer;
	struct arena	*ruler;
};

void	arenas_init(struct arenas *);
void	arenas_free(struct arenas *);
