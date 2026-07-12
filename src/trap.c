#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <kernel/serial.h>

#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/timer.h>

/* defined in src/trap_entry.S */
extern void trap_entry();

/* we don't have SMP yet, so everything runs on hart 0 */
#define BOOT_HART	0

/**
 * handle_exception(): handle a synchronous trap (exception)
 * @scause: exception code from the scause CSR
 * @stval:  trap value (e.g. faulting address) from the stval CSR
 * @sepc:   address of the instruction that faulted, from the sepc CSR
 *
 * For now we can't recover from any exception, so we just pretty-print
 * what happened and panic.
 */
void handle_exception(u64 scause, u64 stval, u64 sepc)
{
	switch (scause) {
	case EXCEPTION_INST_ACCESS_FAULT:
		error("instruction access fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	case EXCEPTION_LOAD_ACCESS_FAULT:
		error("load access fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	case EXCEPTION_STORE_ACCESS_FAULT:
		error("store access fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	case EXCEPTION_INST_PAGE_FAULT:
		error("instruction page fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	case EXCEPTION_LOAD_PAGE_FAULT:
		error("load page fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	case EXCEPTION_STORE_PAGE_FAULT:
		error("store page fault at 0x%x, sepc = 0x%x\n", stval, sepc);
		break;
	default:
		error("uncaught exception! scause = 0x%x, stval = 0x%x, sepc = 0x%x\n",
		      scause, stval, sepc);
	}

	panic("unhandled exception\n");
}

/**
 * handle_irq(): handle an asynchronous trap (interrupt)
 * @scause: cause code from the scause CSR (with bit 63 set)
 *
 * Dispatches to the timer handler for timer interrupts, or performs the
 * PLIC claim/complete handshake and dispatches to the appropriate device
 * handler for external interrupts.
 */
void handle_irq(u64 scause)
{
	u32 irq;

	switch (scause) {
	case TRAP_TIMER_IRQ:
		timer_irq();
		break;
	case TRAP_EXTERNAL_IRQ:
		/* claim the interrupt from the PLIC; a return value of 0
		 * means there is nothing to service (or another hart got
		 * to it first, which can't happen with a single hart) */
		irq = plic_hart_claim_irq(BOOT_HART);
		if (irq == 0)
			break;

		switch (irq) {
		case IRQ_SERIAL:
			serial_irq();
			break;
		default:
			warn("unexpected external irq: %d\n", irq);
		}

		/* tell the PLIC we're done handling this interrupt */
		plic_hart_complete_irq(BOOT_HART, irq);
		break;
	default:
		warn("unexpected interrupt, scause = 0x%x\n", scause);
	}
}

/**
 * handle_trap(): trap handler entry point (called from trap_entry.S)
 *
 * Reads scause to figure out whether we're dealing with an interrupt or an
 * exception (bit 63) and forwards the work accordingly.
 */
void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);

	if (scause & TRAP_IRQ_BIT)
		handle_irq(scause);
	else
		handle_exception(scause, csr_read(CSR_STVAL), csr_read(CSR_SEPC));
}

void trap_setup()
{
	/* keep interrupts disabled while we install the handler */
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	/* point stvec at our assembly trap entry stub */
	csr_write(CSR_STVEC, trap_entry);
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	/* atomically read sstatus and clear the SIE bit, returning the
	 * previous value of that bit so it can be restored later */
	u64 sstatus = csr_read_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	return sstatus & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	if (flags & CSR_SSTATUS_SIE)
		csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
	else
		csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}
