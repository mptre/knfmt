/*
 * Loop construct hidden behind cpp followed by a statement which is a sole
 * expression.
 */

int
main(void)
{
	SLIST_FOREACH(sensor, &report->sensors, rep_next)
		upd_sensor_update(sc, sensor, data, len);
}
