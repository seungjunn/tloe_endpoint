#include "timeout.h"

long get_current_time() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

int is_timeout_tx(time_t send_time) {
	return difftime(time(NULL), send_time) >= TIMEOUT_TX;
}

void init_timeout_rx(TimeoutRX *rx) {
	rx->last_ack_time = get_current_time();
    rx->last_ack_seq = -1;
    rx->ack_pending = 0;
}

int is_send_delayed_ack(TimeoutRX *rx) {
	long curr_ms = get_current_time();
	long elapsed_ms = curr_ms - rx->last_ack_time;

	if (rx->ack_pending && elapsed_ms >= TIMEOUT_RX) {
		return 1;
	}
	return 0;
}

