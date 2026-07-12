#include <kernel/printf.h>
#include <kernel/mm.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <arch/timer.h>

#define LINE_MAX	256
#define READ_MAX	256

/* pending-alarm state, owned entirely by the shell. we track the deadline
 * ourselves (in timer ticks) instead of relying on the timer module: the
 * timer interrupt's only job is to wake us up at the right time. */
static bool alarm_active = false;
static u64 alarm_deadline = 0;

/* true once a scheduled alarm's deadline has elapsed */
static bool alarm_due()
{
	return alarm_active && timer_read() >= alarm_deadline;
}

static void shell_prompt()
{
	serial_puts("> ");
}

/*
 * run a single command line (NUL-terminated, without the trailing CR).
 * supported commands: uptime, echo [str], alarm [time]
 */
static void shell_run(char *line)
{
	if (strcmp(line, "uptime") == 0) {
		char out[32];
		snprintf(out, sizeof(out), "%lus\r\n", timer_read() / TIMER_FREQ);
		serial_puts(out);
	} else if (strncmp(line, "echo ", 5) == 0) {
		serial_puts(line + 5);
		serial_puts("\r\n");
	} else if (strcmp(line, "echo") == 0) {
		serial_puts("\r\n");
	} else if (strncmp(line, "alarm ", 6) == 0) {
		/* schedule the alarm: record when it should fire and arm the
		 * timer interrupt. the "alarm" string is printed later, from
		 * the main loop, once the deadline elapses. */
		u64 secs = strtou64(line + 6, 10);
		alarm_deadline = timer_read() + secs * TIMER_FREQ;
		alarm_active = true;
		timer_set_alarm(secs);
	} else if (line[0] != '\0') {
		serial_puts("unknown command: ");
		serial_puts(line);
		serial_puts("\r\n");
	}
}

static void shell()
{
	char line[LINE_MAX];
	char in[READ_MAX];
	size_t len = 0;

	shell_prompt();

	while (1) {
		/* a previously-scheduled alarm may have elapsed */
		if (alarm_due()) {
			alarm_active = false;
			serial_puts("\r\nalarm\r\n");
			/* redraw the prompt and whatever the user had typed */
			shell_prompt();
			for (size_t i = 0; i < len; i++)
				serial_putc(line[i]);
		}

		/*
		 * Drain the input buffer with interrupts disabled. Reading
		 * under a disabled-interrupt window (instead of peeking with a
		 * helper) is what lets us avoid the classic lost-wakeup race
		 * below without adding any new driver function.
		 */
		hart_irq_disable();
		size_t n = serial_read(in);

		if (n == 0 && !alarm_due()) {
			/*
			 * Nothing to do: sleep until the next interrupt instead
			 * of busy-waiting. Interrupts are still disabled here, so
			 * a byte/timer that fires now is NOT serviced, it just
			 * stays pending -- and wfi wakes on any pending *enabled*
			 * interrupt (sie[STIE]/[SEIE]) even with sstatus[SIE]=0.
			 * Re-enabling afterwards lets the handler actually run.
			 *
			 * The two wakeup conditions were already re-checked under
			 * this same disabled window: serial_read() returned the
			 * current buffer contents, and alarm_due() reads the time
			 * CSR directly (level-triggered, so it can't be missed).
			 */
			__asm__ __volatile__("wfi");
			hart_irq_enable();
			continue;
		}

		hart_irq_enable();

		for (size_t i = 0; i < n; i++) {
			char c = in[i];

			if (c == '\r' || c == '\n') {
				serial_puts("\r\n");
				line[len] = '\0';
				shell_run(line);
				len = 0;
				shell_prompt();
			} else if (c == 0x7f || c == 0x08) {
				/* backspace/delete: erase last char if any */
				if (len > 0) {
					len--;
					serial_puts("\b \b");
				}
			} else if (len < LINE_MAX - 1) {
				line[len++] = c;
				serial_putc(c); /* echo */
			}
		}
	}
}

extern int _hartid[];
void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	/* make sure interrupts are globally enabled for this hart */
	hart_irq_enable();

	info("starting shell\n");
	shell();
}
