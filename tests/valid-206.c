/*
 * Function arguments, honor optional line.
 */

const struct got_error *got_deltify_file_mem(
    struct got_delta_instruction **deltas, int *ndeltas,
    FILE *f, off_t fileoffset, off_t filesize, struct got_delta_table *dt,
    uint8_t *basedata, off_t basefile_offset0, off_t basefile_size);
