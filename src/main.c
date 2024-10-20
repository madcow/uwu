#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

// Edge direction
#define D_RISING   1
#define D_FALLING  0

// Decoder state
#define S_IDLE  0
#define S_SYNC  1
#define S_DATA  2

// TODO: Framing with either bit or byte based approach:
// - Bit stuffing approach is completely data agnostic
// - Sentinel bytes with escape characters are simpler
// - https://book.systemsapproach.org/direct/framing.html
// - https://humphryscomputing.com/Notes/Networks/data.framing.html

static volatile bool   txready;     // Aligned with clock?
static volatile int    rxstate;     // Current decoder state
static volatile word   edgecap;     // Current edge capture time
static volatile word   edgedir;     // Current edge direction
static volatile int    lastbit;     // Previously read logic value
static volatile word   numsync;     // Number of preamble bits read
static volatile word   numdata;     // Number of data bits read
static volatile bool   needmid;     // Expect short interval next
static volatile byte   rxbuf[128];  // Data buffer for decoder
static volatile byte * rxhead;      // Write position for decoder
static volatile byte * rxtail;      // End of decoder buffer
static volatile bool   rxdone;      // Finished receiving data
static volatile int    ntxseq;      // TX end sequence counter
static volatile int    nrxseq;      // RX end sequence counter

static void SendByte(byte data);
static void SendTerminator(void);
static void WaitPulse(void);
static void HandleEdge(void);
static void Synchronize(void);
static void ReadDataBit(void);
static void ReadShortPeriod(void);
static void ReadLongPeriod(void);
static void WriteBit(int val);

void RF_Init(void)
{
	// Set pins for line coded data
	DDR(D) |=  BIT(5); // Modulator
	DDR(B) &= ~BIT(5); // Demodulator

	// Calculate end of decoder buffer
	rxtail = rxbuf + sizeof(rxbuf);

	// Data rate = 4000 Hz
	// Baud rate = 8000 Hz (edge changes)
	// Bit period = 1/f = 1/4000 = 250us
	// T (mid-bit time) = 125us

	// Initialize TIMER1 to generate encoder clock pulses
	// at half of bit period which equals mid-bit time T.

	TIMSK1  = 0x00;  // Disable timer interrupts
	TIFR1   = 0x27;  // Clear all interrupt flags
	TCCR1B  = 0x02;  // Prescale /8 = 1MHz = 1us per step
	OCR1A   = 125;   // Generate interrupt every T steps
	TCNT1   = 0;     // Reset counter value to zero
	TCCR1A  = 0x00;  // Timer not connected to port
	TCCR1C  = 0x00;  // Do not force compare match
	TIMSK1  = 0x02;  // Enable compare interrupt

	// Initialize TIMER3 to interrupt when a rising edge on
	// PB5 is detected and when the counter value overflows.

	TIMSK3  = 0x00;  // Disable timer interrupts
	TIFR3   = 0x27;  // Clear all interrupt flags
	TCCR3B  = 0x02;  // Prescale /8 = 1MHz = 1us per step
	TCCR3B |= 0x40;  // Trigger capture event on rising edge
	OCR3A   = 0;     // Not using output compare interrupt
	TCNT3   = 0;     // Reset counter value to zero
	TCCR3A  = 0x00;  // Timer not connected to port
	TCCR3C  = 0x00;  // Do not force compare match
	TIMSK3  = 0x20;  // Enable input capture interrupt
	TIMSK3 |= 0x01;  // Enable overflow interrupt
}

void RF_Transmit(const byte *data, int size)
{
	const byte *head = data;
	const byte *tail = data + size;

	// The preamble with its alternating symbols is
	// line coded with only the actual meat-and-potato
	// transitions in the middle of the bit period and
	// none of those pesky boundary transitions. This
	// makes it possible for the decoder to align the
	// clock phase before receiving any data.

	// Preamble for clock synchronization
	SendByte(0xAA); // AA = 1010 1010

	while (head < tail) {
		SendByte(*head++);
	}

	// EOT sentinel marking the end
	SendTerminator(); // FF = 1111 1111
}

int RF_Receive(byte *data, int size)
{
	int n = 0;

	if (!rxdone) {
		return 0;
	}

	// There is no race condition here since the decoder
	// will pause until all data has been processed and
	// the rxdone flag is unset.

	while (1) {
		if (n == size || n == (int) numdata) {
			break; // Finished copying
		}
		data[n] = rxbuf[n];
		n++;
	}

	rxdone = false;
	return n;
}

static void SendByte(byte data)
{
	// Manchester code always has a transition at the
	// middle of each bit period and may (depending on
	// the information to be transmitted) have one at
	// the start of the period also. The direction of
	// the mid-bit transition indicates the data.

	// Transitions at the period boundaries do not carry
	// information. They only place the signal in the
	// correct state to allow the mid-bit transition.

	for (int bit = 0; bit < 8; bit++) {
		if (data & (0x80 >> bit)) {
			WaitPulse();
			PORT(D) &= ~BIT(5);
			WaitPulse();
			// Rising edge
			PORT(D) |= BIT(5);

			// Any time seven consecutive 1s have
			// been transmitted from the body of
			// the message, the sender inserts a
			// 0 before the next bit. This makes
			// it possible to distinguish the
			// end of frame sequence from bit
			// patterns in the data.

			ntxseq++;
			if (ntxseq == 7) {
				WaitPulse();
				WaitPulse();
				PORT(D) &= ~BIT(5);
				ntxseq = 0;
			}
		} else {
			WaitPulse();
			PORT(D) |= BIT(5);
			WaitPulse();
			// Falling edge
			PORT(D) &= ~BIT(5);
			ntxseq = 0;
		}
	}
}

static void SendTerminator(void)
{
	// FF = 1111 1111
	for (int i = 0; i < 8; i++) {
		WaitPulse();
		PORT(D) &= ~BIT(5);
		WaitPulse();
		PORT(D) |= BIT(5);
	}
}

static void WaitPulse(void)
{
	txready = false;
	while (!txready);
}

static void HandleEdge(void)
{
	if (rxdone || edgedir != D_RISING) {
		return; // Ignore this edge
	}

	rxstate = S_SYNC;
	numsync = 1;
}

static void Synchronize(void)
{
	// Preamble only has middle transitions
	if (edgecap < 200 || edgecap > 300) {
		rxstate = S_IDLE; // Wrong timing
		return;
	}

	numsync++;
	if (numsync == 8) {
		numdata = 0;
		lastbit = 0;
		needmid = false;
		rxhead  = rxbuf;
		rxstate = S_DATA;
	}
}

static void ReadDataBit(void)
{
	if (edgecap >= 75 && edgecap <= 175) {
		ReadShortPeriod();
		return;
	}

	if (edgecap >= 200 && edgecap <= 300) {
		ReadLongPeriod();
		return;
	}

	// Wrong timing
	rxstate = S_IDLE;
}

static void ReadShortPeriod(void)
{
	// The period length gives us enough information to
	// know what the bit value is without even looking
	// at the edge direction.

	if (needmid) {
		WriteBit(lastbit);
		needmid = false;
	} else {
		needmid = true;
	}
}

static void ReadLongPeriod(void)
{
	// If there was a boundary transition we must expect
	// to receive another transition after mid-bit time,
	// otherwise something went wrong...

	if (needmid) {
		rxstate = S_IDLE;
		return;
	}

	lastbit = !lastbit;
	WriteBit(lastbit);
}

static void WriteBit(int val)
{
	int bit;

	if (rxhead == rxtail) {
		rxstate = S_IDLE;
		return;
	}

	if (nrxseq == 7) {
		nrxseq = 0;
		if (val == 0) {
			return;
		}
		// End of frame
		numdata -= 7;
		rxdone   = true;
		rxstate  = S_IDLE;
		nrxseq   = 0;
	} else {
		if (val == 1) {
			nrxseq++;
		} else {
			nrxseq = 0;
		}
	}

	bit = numdata % 8;
	numdata++;

	*rxhead &= ~(0x80 >> bit);
	*rxhead |= (val << (7 - bit));

	if (bit == 7) {
		rxhead++;
	}
}

// Encoder clock pulse
ISR(TIMER1_COMPA_vect)
{
	TCNT1 = 0;
	txready = true;
}

// Decoder edge capture
ISR(TIMER3_CAPT_vect)
{
	TCNT3 = 0;
	edgecap = ICR3;
	edgedir = (PIN(B) & BIT(5)) ? D_RISING : D_FALLING;

	// Must not simply toggle the edge direction bit since
	// we can miss very quick edge changes and run out of
	// sync with the actual port state.

	TCCR3B = (edgedir == D_RISING) ? 0x02 : 0x42;

	if (rxstate == S_IDLE) {
		HandleEdge();
	} else if (rxstate == S_SYNC) {
		Synchronize();
	} else if (rxstate == S_DATA) {
		ReadDataBit();
	}
}

// Decoder overflow
ISR(TIMER3_OVF_vect)
{
	TCNT3 = 0;
	edgecap = 0xFFFF;
	if (rxstate == S_SYNC) {
		Synchronize();
	} else if (rxstate == S_DATA) {
		ReadDataBit();
	}
}

int main(void)
{
	bool running = true;
	const byte testmsg[] = "FOOBAR";
	byte recvbuf[128];

	RF_Init();
	sei();

	while (running) {
		Info("Sending phase encoded message...");
		RF_Transmit(testmsg, sizeof(testmsg));

		Sleep(1000);

		if (RF_Receive(recvbuf, sizeof(recvbuf))) {
			Info("Received message '%s'.", recvbuf);
		}

		Sleep(1000);
	}

	return 0;
}
