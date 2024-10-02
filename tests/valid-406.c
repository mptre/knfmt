/*
 * Redundant spaces in alignment of declarations.
 */

struct Forward {
	char	 *listen_host;
	int	  listen_port;
	char	 *listen_path;
	char	 *connect_host;
	int	  connect_port;
	char	 *connect_path;
	int	  allocated_port;
	int	  handle;
};
