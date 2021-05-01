/*
 * Attribute alias.
 */

struct ubcmtp_finger {
	uint16_t	origin;
	uint16_t	abs_x;
	uint16_t	abs_y;
	uint16_t	rel_x;
	uint16_t	rel_y;
	uint16_t	tool_major;
	uint16_t	tool_minor;
	uint16_t	orientation;
	uint16_t	touch_major;
	uint16_t	touch_minor;
	uint16_t	unused[2];
	uint16_t	pressure;
	uint16_t	multi;
} __packed __attribute((aligned(2)));
