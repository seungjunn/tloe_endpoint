#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "tloe_endpoint.h"
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tloe_transmitter.h"
#include "tloe_receiver.h"
#include "tilelink_msg.h"
#include "retransmission.h"
#include "timeout.h"
#include "util/circular_queue.h"
#include "util/util.h"

int test_timeout = 0;  // for test timeout

void init_tloe_endpoint(tloe_endpoint_t *e, TloeEther *ether) {
	e->is_done = 0;

	e->next_tx_seq = 0;
	e->next_rx_seq = 0;
	e->acked_seq = MAX_SEQ_NUM;
	e->acked = 0;

    e->retransmit_buffer = create_queue(WINDOW_SIZE + 1);
    e->rx_buffer = create_queue(10); // credits
	e->message_buffer = create_queue(10000);
	e->ack_buffer = create_queue(100);

	e->ether = ether;

	init_timeout_rx(&(e->timeout_rx));
}

void close_tloe_endpoint(tloe_endpoint_t *e) {
    // Join threads
    pthread_join(e->tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(e->ether);

    // Cleanup queues
    delete_queue(e->message_buffer);
    delete_queue(e->retransmit_buffer);
    delete_queue(e->rx_buffer);
    delete_queue(e->ack_buffer);
}

void *tloe_endpoint(void *arg) {
	tloe_endpoint_t *e = (tloe_endpoint_t *)arg;

	TloeFrame *request_tloeframe = NULL;
	TloeFrame *not_transmitted_frame = NULL;

	while(!e->is_done) {
		if (!request_tloeframe && !is_queue_empty(e->message_buffer)) 
			request_tloeframe = dequeue(e->message_buffer);

		not_transmitted_frame = TX(request_tloeframe, e);
		if (not_transmitted_frame) {
			request_tloeframe = not_transmitted_frame;
			not_transmitted_frame = NULL;
		} else if (!not_transmitted_frame  && request_tloeframe) { 
			free(request_tloeframe);
			request_tloeframe = NULL;
		}

		RX(e);
	}
}

int main(int argc, char *argv[]) {
	tloe_endpoint_t *e;
	TloeEther *ether;
	char input, input_count[32];
	int iter = 0;

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }

    srand(time(NULL));

    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

	e = (tloe_endpoint_t *)malloc(sizeof(tloe_endpoint_t));

	init_tloe_endpoint(e, ether);

	if (pthread_create(&(e->tloe_endpoint_thread), NULL, tloe_endpoint, e) != 0) {
        error_exit("Failed to create tloe endpoint thread");
    }
    
	while(!(e->is_done)) {
		printf("Enter 's' to status, 'a' to send, 'q' to quit:\n");
		printf("> ");
		fgets(input_count, sizeof(input_count), stdin);

		if (sscanf(input_count, " %c %d", &input, &iter) < 1) {
			printf("Invalid input! Try again.\n");
			continue;
		}

		if (input == 's') {
			printf("-----------------------------------------------------\n");
			printf(" next_tx_seq: %d, next_rx_seq: %d, ack_cnt: %d, dup: %d, oos: %d, delay: %d, drop: %d\n", 
				e->next_tx_seq, e->next_rx_seq, e->ack_cnt, 
				e->dup_cnt, e->oos_cnt, e->delay_cnt, e->drop_cnt);
			printf("-----------------------------------------------------\n");
		} else if (input == 'a') {
			for (int i = 0; i < iter; i++) {
				TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
				if (!new_tloe) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				new_tloe->mask = 1;  // Set mask (1 = normal packet)

				while(is_queue_full(e->message_buffer)) 
					usleep(1000);

				if (enqueue(e->message_buffer, new_tloe)) {
					if (i % 100 == 0)
						fprintf(stderr, "Packet %d added to message_buffer\n", i);
				} else {
					//printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tloe);
					break;  // Stop if buffer is full
				}
			}
		} else if (input == 'i') {
			// Test timeout
			test_timeout = iter;
		} else if (input == 'q') {
			e->is_done = 1;
			printf("Exiting...\n");
			break;
		}
	}

	close_tloe_endpoint(e);


   return 0;
}

