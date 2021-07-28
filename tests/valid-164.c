/*
 * Function pointer without arguments.
 */

int
main(void)
{
	task_set(&crp->crp_task, (void (*))crypto_invoke, crp);
}
