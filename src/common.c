#include "common.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define UART_BAUDRATE    9600
#define UART_TXBUF_SIZE  128

#define UART_TXBUF_MASK (UART_TXBUF_SIZE - 1)
#define UART_BAUD_PRESCALE ((((F_CPU / 16) + \
        (UART_BAUDRATE / 2)) / (UART_BAUDRATE)) - 1)

static volatile char txbuf[UART_TXBUF_SIZE];
static volatile word txhead, txtail;

static void UART_Init(void);
static void UART_Puts(const char *str);
static void UART_Putc(char ch);

void Info(const char *fmt, ...)
{
	va_list ap;
	char msg[256];

	// Lazily initialize this module
	if ((UCSR0B & BIT(TXEN0)) == 0) {
		UART_Init();
	}

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	UART_Puts("[CORE] ");
	UART_Puts(msg);
	UART_Puts("\r\n");
}

static void UART_Init(void)
{
	txhead = 0;
	txtail = 0;

	UCSR0B = BIT(TXEN0); // Enable TX circuitry
	UCSR0C = BIT(UCSZ01) | BIT(UCSZ00); // 8-bit data, 1-bit stop, no parity
	UBRR0H = (UART_BAUD_PRESCALE >> 8); // Set baud rate upper byte
	UBRR0L = UART_BAUD_PRESCALE; // Set baud rate lower byte

	sei();
	Sleep(100);
}

static void UART_Puts(const char *str)
{
	while (*str != '\0') {
		UART_Putc(*str++);
	}
}

static void UART_Putc(char ch)
{
	word head;

	// Wrap around if end of buffer reached
	head = (txhead + 1) & UART_TXBUF_MASK;
	while (head == txtail); // Wait for space

	txbuf[head] = ch;
	txhead = head;

	// Enable interrupt
	UCSR0B |= BIT(UDRIE0);
}

// Data register empty
ISR(USART0_UDRE_vect)
{
	word tail;

	// Anything in TX buffer?
	if (txhead != txtail) {
		// Write next byte to data register
		tail = (txtail + 1) & UART_TXBUF_MASK;
		UDR0 = txbuf[tail];
		txtail = tail;
	} else {
		// Disable interrupt
		UCSR0B &= ~BIT(UDRIE0);
	}
}
