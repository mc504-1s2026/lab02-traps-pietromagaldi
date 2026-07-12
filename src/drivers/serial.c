#include <kernel/serial.h>
#include <kernel/types.h>

#include <arch/csr.h>
#include <arch/io.h>
#include <arch/plic.h>
#include <arch/spinlock.h>

/* no SMP yet: everything runs on hart 0 */
#define BOOT_HART	0

#define SERIAL_BUF_SIZE	256

/* internal driver state, shared between the interrupt handler (producer)
 * and serial_read() (consumer), hence protected by a spinlock */
static struct serialdev {
	char buf[SERIAL_BUF_SIZE];
	size_t len;
	struct spinlock lock;
} dev;

/* the serial device is identity-mapped at SERIAL_BASE (see mm.c), so we
 * can address its registers directly */
static u8 __always_inline serial_reg_read(u64 reg)
{
	return ioread8((void *)((u64)SERIAL_BASE + reg));
}

static void __always_inline serial_reg_write(u64 reg, u8 val)
{
	iowrite8(val, (void *)((u64)SERIAL_BASE + reg));
}

void serial_init()
{
	spin_init(&dev.lock);
	dev.len = 0;

	/* mask device interrupts while we configure it */
	serial_reg_write(SERIAL_IER, 0);

	/* enable and flush the RX/TX FIFOs so we can grab a few bytes
	 * per interrupt instead of trapping on every single byte */
	serial_reg_write(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE
			 | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);

	/* enable the "received data available" interrupt (RX only) */
	serial_reg_write(SERIAL_IER, SERIAL_IER_ERBFI);

	/* route the serial IRQ through the PLIC to this hart:
	 * - give it a non-zero priority (0 means "never interrupt")
	 * - set the hart threshold to 0 so it accepts any priority > 0
	 * - enable the IRQ line for this hart */
	plic_irq_set_priority(IRQ_SERIAL, 1);
	plic_hart_set_threshold(BOOT_HART, 0);
	plic_hart_enable_irq(BOOT_HART, IRQ_SERIAL);
}

void serial_irq_enable()
{
	/* enable external interrupts (sie[SEIE]) and interrupts globally */
	csr_set(CSR_SIE, CSR_SIE_SEIE);
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void serial_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	/* We run with interrupts disabled inside the trap handler (the
	 * hardware clears sstatus[SIE] on trap entry), so a plain spin_lock
	 * can't deadlock against ourselves here. Main-context code that
	 * touches `dev` must use the *_irqsave variants. */
	spin_lock(&dev.lock);

	/* drain the RX FIFO while there is data available */
	while (serial_reg_read(SERIAL_LSR) & SERIAL_LSR_DTR) {
		char c = (char)serial_reg_read(SERIAL_RBR);
		if (dev.len < SERIAL_BUF_SIZE)
			dev.buf[dev.len++] = c;
		/* else: buffer full, drop the byte */
	}

	spin_unlock(&dev.lock);
}

size_t serial_read(char *buf)
{
	u64 flags = spin_lock_irqsave(&dev.lock);

	size_t len = dev.len;
	for (size_t i = 0; i < len; i++)
		buf[i] = dev.buf[i];
	dev.len = 0;

	spin_unlock_irqrestore(&dev.lock, flags);
	return len;
}

void serial_putc(char c)
{
	/* poll until the transmitter holding register is empty, then write */
	while (!(serial_reg_read(SERIAL_LSR) & SERIAL_LSR_THRE)) {}
	serial_reg_write(SERIAL_THR, (u8)c);
}

void serial_puts(char *s)
{
	while (*s != '\0')
		serial_putc(*s++);
}
