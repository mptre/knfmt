/*
 * Declarations using sys/tree.h.
 */

static RB_HEAD(appl_internal_objects, appl_internal_object)
    appl_internal_objects = RB_INITIALIZER(&appl_internal_objects),
    appl_internal_objects_conf = RB_INITIALIZER(&appl_internal_objects_conf);
RB_PROTOTYPE_STATIC(appl_internal_objects, appl_internal_object, entry,
    appl_internal_object_cmp);
