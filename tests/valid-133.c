/*
 * Hard recover regression.
 */

struct nsd			 nsd;
static char			 hostname[MAXHOSTNAMELEN];
extern config_parser_state_type	*cfg_parser;
static void version(void) ATTR_NORETURN;

static void	usage(void);
