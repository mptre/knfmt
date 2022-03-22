/*
 * Function arguments, honor optional line.
 */

const struct got_error *got_gitproto_match_capabilities(
    char **common_capabilities,
    struct got_pathlist_head *symrefs, char *server_capabilities,
    const struct got_capability my_capabilities[], size_t ncapa);
