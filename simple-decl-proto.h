struct lexer;
struct token;

struct simple_decl_proto	*simple_decl_proto_enter(struct lexer *);
void				 simple_decl_proto_leave(
    struct simple_decl_proto *);
void				 simple_decl_proto_free(
    struct simple_decl_proto *);

void	simple_decl_proto_arg(struct simple_decl_proto *);
void	simple_decl_proto_arg_ident(struct simple_decl_proto *, struct token *);
