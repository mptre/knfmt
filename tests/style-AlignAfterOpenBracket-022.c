/*
 * AlignAfterOpenBracket: Align
 */

static int
hidpp20_query_battery_info_1004(struct hidpp_device *hidpp)
{
	ret = hidpp20_unifiedbattery_get_capabilities(hidpp,
		hidpp->battery.feature_index);
}
