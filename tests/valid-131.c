/*
 * Expression cpp branch regression.
 */

int
power_status(int fd, int force, struct apm_power_info *pinfo)
{
	if (ioctl(fd, APM_IOC_GETPOWER, &bstate) == 0) {
	/* various conditions under which we report status:  something changed
	 * enough since last report, or asked to force a print */
		if (bstate.ac_state == APM_AC_ON)
			acon = 1;
		if (bstate.battery_state == APM_BATT_CRITICAL &&
		    bstate.battery_state != last.battery_state)
			priority = LOG_EMERG;
		if (force) {
#ifdef __powerpc__
			if (bstate.minutes_left != 0 &&
			    bstate.battery_state != APM_BATT_CHARGING)
#else
			if ((int)bstate.minutes_left > 0)
#endif
				logmsg(priority, "__powerpc__");
			else
				logmsg(priority, "! __powerpc__");
		}
	}
}
