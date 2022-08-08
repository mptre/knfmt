/*
 * Mixing field and ordinary brace initializers.
 */

static struct uart_class uart_pl011_class = {
	"uart_pl011",
	uart_pl011_methods,
	sizeof(struct uart_pl011_softc),
	.uc_ops = &uart_pl011_ops,
	.uc_range = 0x48,
	.uc_rclk = 0,
	.uc_rshift = 2
};
