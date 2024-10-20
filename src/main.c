#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

// https://book.systemsapproach.org/direct/framing.html
// https://humphryscomputing.com/Notes/Networks/data.framing.html
// https://en.wikipedia.org/wiki/Automatic_baud_rate_detection

#define MAX_TX_SIZE 128

static volatile bool txwaiting;
static volatile byte txbuf[2 * MAX_TX_SIZE];
static volatile word txsize, txindex;

void RF_Transmit(const byte *data, int size)
{
	// SYN  10101010 10101010 10101010 10101010
	// SFD  01111110
	// DAT  <VARIABLE LENGTH PAYLOAD>
	// FCS  XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
	// EFD  01111110

	// Wait until ready
	while (txwaiting);

	// Transmission buffer must have capacity for
	// storing two edge states per data bit. The
	// final byte in buffer may not use all bits.

	// TODO: Prepare edge sequence in tx buffer
	// TODO: Use bit stuffing to escape sentinels
	// TODO: Set txwaiting flag to allow interrupt

	UNUSED(data);
	UNUSED(size);
}

ISR(TIMER1_COMPA_vect)
{
	if (!txwaiting) {
		return;
	} else if (txindex < txsize) {
		word pos = txindex / 8;
		word bit = txindex % 8;
		txindex += (bit == 7) ? 1 : 0;
		// Fetch and send next edge direction
		if ((txbuf[pos] >> (7 - bit)) & 0x1) {
			PORT(D) |= BIT(5);
		} else {
			PORT(D) &= ~BIT(5);
		}
	} else { // Finished?
		txwaiting = false;
	}
}

int main(void)
{
	sei();
	const byte data[] = "FOOBAR";
	RF_Transmit(data, sizeof(data));
	return 0;
}
