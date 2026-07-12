#include <arch/timer.h>
#include <arch/csr.h>

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	/* enable the supervisor timer interrupt source... */
	csr_set(CSR_SIE, CSR_SIE_STIE);
	/* ...and interrupts globally for this hart */
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	/* the time CSR ticks at TIMER_FREQ Hz; a timer interrupt fires
	 * whenever time >= stimecmp */
	u64 deadline = csr_read(CSR_TIME) + secs * TIMER_FREQ;
	csr_write(CSR_STIMECMP, deadline);
}

void timer_irq()
{
	/* Silence the timer: the interrupt keeps firing while
	 * time >= stimecmp, so push the comparator far into the future.
	 *
	 * We don't do the actual "alarm" bookkeeping here: the interrupt's
	 * only job is to wake the hart (e.g. out of wfi) at the scheduled
	 * time. Whoever scheduled the alarm decides what to do once awake. */
	csr_write(CSR_STIMECMP, ~0UL);
}
