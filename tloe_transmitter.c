#include <stdio.h>
#include <stddef.h>
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "timeout.h"

TloeFrame *TX(TloeFrame *tloeframe, TloeEther *ether) {
	TloeFrame *returnframe = NULL;
	if (!is_queue_empty(ack_buffer)) {
		TloeFrame *ackframe = NULL;

		returnframe = tloeframe;
		ackframe = (TloeFrame *)dequeue(ack_buffer);

		// ACK/NAK (zero-TileLink)
		// Reflect the sequence number but do not store in the retransmission buffer, just send
		ackframe->seq_num = next_tx_seq;

		//printf("TX: Sending ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n",
		//    tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

		tloe_ether_send(ether, (char *)ackframe, sizeof(TloeFrame));

		free(ackframe);
	} else if (tloeframe == NULL) {
	} else if (!is_queue_full(retransmit_buffer)) {
		// If the TileLink message is empty
		if (is_ack_msg(tloeframe)) {
			printf("ERROR: %s: %d\n", __FILE__, __LINE__);
			exit(1);
		}

		// NORMAL packet
		// Reflect the sequence number, store in the retransmission buffer, and send
		// Enqueue to retransmitBuffer
		RetransmitBufferElement *e = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));

		e->tloe_frame = *tloeframe;
		e->tloe_frame.seq_num = next_tx_seq;
		e->state = TLOE_INIT;
		e->send_time = get_current_time();

		if (!enqueue(retransmit_buffer, e)) {
			free(e);
			printf("ERROR: %s: %d\n", __FILE__, __LINE__);
			exit(1);
		}

		tloeframe->seq_num = next_tx_seq;
		tloeframe->seq_num_ack = acked_seq;
		tloeframe->ack = TLOE_ACK;
		tloeframe->mask = 1;

		// Update sequence number
		next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);

#if 0
		// DEBUG: DROP packet
		if (next_tx_seq == 5) {
			printf("TX: DROPPED packet for seq_num: %d\n", next_tx_seq);
			return ;
		}
#endif
		//printf("TX: Sending TL msg with seq_num: %d, seq_num_ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack);
		tloe_ether_send(ether, (char *)tloeframe, sizeof(TloeFrame));

		e->state = TLOE_SENT;
	} else {
		//printf("retransmit_buffer is full: %s: %d\n", __FILE__, __LINE__);
		returnframe = tloeframe;
	}

	// Timeout TX 
	RetransmitBufferElement *e = getfront(retransmit_buffer);
    if (!e) return returnframe;

    if (is_timeout_tx(e->send_time)) {
        retransmit(ether, retransmit_buffer, e->tloe_frame.seq_num);
		printf("TX: Timeout TX and retranmission from seq_num: %d\n", e->tloe_frame.seq_num);
	}

    return returnframe;
}
