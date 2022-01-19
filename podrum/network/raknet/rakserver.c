/*
                   Podrum R3 Copyright MFDGaming & PodrumTeam
                 This file is licensed under the GPLv2 license.
              To use this file you must own a copy of the license.
                       If you do not you can get it from:
            http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#include "./rakserver.h"
#include "./rakhandler.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

double get_raknet_timestamp(raknet_server_t *server)
{
	return (time(NULL) * 1000) - server->epoch;
}

char has_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	int i;
	for (i = 0; i < server->connections_count; ++i) {
		if ((server->connections[i].address.port == address.port) && (strcmp(server->connections[i].address.address, address.address) == 0)) {
			return 1;
		}
	}
	return 0;
}

void add_raknet_connection(misc_address_t address, uint16_t mtu_size, uint64_t guid, raknet_server_t *server)
{
	if (has_raknet_connection(address, server) == 0) {
		connection_t connection;
		connection.ack_queue = (uint32_t *) malloc(0);
		connection.ack_queue_size = 0;
		connection.address = address;
		connection.compound_id = 0;
		connection.frame_holder = (misc_frame_t *) malloc(0);
		connection.frame_holder_size = 0;
		connection.guid = guid;
		connection.last_ping_time = time(NULL);
		connection.last_receive_time = time(NULL);
		connection.ms = 0;
		connection.mtu_size = mtu_size;
		connection.nack_queue = (uint32_t *) malloc(0);
		connection.nack_queue_size = 0;
		connection.queue.frames = (misc_frame_t *) malloc(0);
		connection.queue.frames_count = 0;
		connection.queue.sequence_number = 0;
		connection.receiver_reliable_frame_index = 0;
		connection.receiver_sequence_number = 0;
		connection.recovery_queue = (packet_frame_set_t *) malloc(0);
		connection.recovery_queue_size = 0;
		memset(connection.sender_order_channels, 0, sizeof(connection.sender_order_channels));
		connection.sender_reliable_frame_index = 0;
		memset(connection.sender_sequence_channels, 0, sizeof(connection.sender_order_channels));
		connection.sender_sequence_number = 0;
		++server->connections_count;
		server->connections = (connection_t *) realloc(server->connections, server->connections_count * sizeof(connection_t));
		server->connections[server->connections_count - 1] = connection;
	}
}

void remove_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	if (has_raknet_connection(address, server) == 1) {
		int i;
		connection_t *connections = (connection_t *) malloc((server->connections_count - 1) * sizeof(connection_t));
		int connections_count = 0;
		for (i = 0; i < server->connections_count; ++i) {
			if ((server->connections[i].address.port != address.port) || (strcmp(server->connections[i].address.address, address.address) != 0)) {
				connections[connections_count] = server->connections[i];
				++connections_count;
			} else {
				free(server->connections[i].ack_queue);
				free(server->connections[i].nack_queue);
				int ii;
				for (ii = 0; ii < server->connections[i].frame_holder_size; ++ii) {
					free(server->connections[i].frame_holder[ii].stream.buffer);
				}
				free(server->connections[i].frame_holder);
				free(server->connections[i].queue.frames);
				for (ii = 0; ii < server->connections[i].recovery_queue_size; ++ii) {
					int iii;
					for (iii = 0; iii < server->connections[i].recovery_queue[ii].frames_count; ++iii) {
						free(server->connections[i].recovery_queue[ii].frames[iii].stream.buffer);
					}
					free(server->connections[i].recovery_queue[ii].frames);
				}
				free(server->connections[i].recovery_queue);
			}
		}
		free(server->connections);
		server->connections = connections;
		--server->connections_count;
	}
}

connection_t *get_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	int i;
	for (i = 0; i < server->connections_count; ++i) {
		if ((server->connections[i].address.port == address.port) && (strcmp(server->connections[i].address.address, address.address) == 0)) {
			return (&(server->connections[i]));
		}
	}
	return NULL;
}

char is_in_raknet_recovery_queue(uint32_t sequence_number, connection_t *connection)
{
	int i;
	for (i = 0; i < connection->recovery_queue_size; ++i) {
		if (connection->recovery_queue[i].sequence_number == sequence_number) {
			return 1;
		}
	}
	return 0;
}

void append_raknet_recovery_queue(packet_frame_set_t frame_set, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(frame_set.sequence_number, connection) == 0) {
		++connection->recovery_queue_size;
		connection->recovery_queue = (packet_frame_set_t *) realloc(connection->recovery_queue, connection->recovery_queue_size * sizeof(packet_frame_set_t));
		connection->recovery_queue[connection->recovery_queue_size - 1] = frame_set;
	}
}

void deduct_raknet_recovery_queue(uint32_t sequence_number, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(sequence_number, connection) == 1) {
		int i;
		packet_frame_set_t *recovery_queue = (packet_frame_set_t *) malloc((connection->recovery_queue_size - 1) * sizeof(packet_frame_set_t));
		int recovery_queue_size = 0;
		for (i = 0; i < connection->recovery_queue_size; ++i) {
			if (connection->recovery_queue[i].sequence_number != sequence_number) {
				recovery_queue[recovery_queue_size] = connection->recovery_queue[i];
				++recovery_queue_size;
			} else {
				int ii;
				for (ii = 0; ii < connection->recovery_queue[i].frames_count; ++ii) {
					free(connection->recovery_queue[i].frames[ii].stream.buffer);
				}
				free(connection->recovery_queue[i].frames);
			}
		}
		free(connection->recovery_queue);
		connection->recovery_queue = recovery_queue;
		--connection->recovery_queue_size;
	}
}

packet_frame_set_t pop_raknet_recovery_queue(uint32_t sequence_number, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(sequence_number, connection) == 1) {
		int i;
		packet_frame_set_t *recovery_queue = (packet_frame_set_t *) malloc((connection->recovery_queue_size - 1) * sizeof(packet_frame_set_t));
		int recovery_queue_size = 0;
		packet_frame_set_t output_frame_set;
		for (i = 0; i < connection->recovery_queue_size; ++i) {
			if (connection->recovery_queue->sequence_number != sequence_number) {
				recovery_queue[recovery_queue_size] = connection->recovery_queue[i];
				++recovery_queue_size;
			} else {
				output_frame_set = connection->recovery_queue[i];
			}
		}
		free(connection->recovery_queue);
		connection->recovery_queue = recovery_queue;
		--connection->recovery_queue_size;
		return output_frame_set;
	}
	packet_frame_set_t output_frame_set;
	output_frame_set.frames = NULL;
	output_frame_set.frames_count = 0;
	output_frame_set.sequence_number = 0;
	return output_frame_set;
}

char is_in_raknet_ack_queue(uint32_t sequence_number, connection_t *connection)
{
	int i;
	for (i = 0; i < connection->ack_queue_size; ++i) {
		if (connection->ack_queue[i] == sequence_number) {
			return 1;
		}
	}
	return 0;
}

void append_raknet_ack_queue(uint32_t sequence_number, connection_t *connection)
{
	if (is_in_raknet_ack_queue(sequence_number, connection) == 0) {
		++connection->ack_queue_size;
		connection->ack_queue = (uint32_t *) realloc(connection->ack_queue, connection->ack_queue_size * sizeof(uint32_t));
		connection->ack_queue[connection->ack_queue_size - 1] = sequence_number;
	}
}

char is_in_raknet_nack_queue(uint32_t sequence_number, connection_t *connection)
{
	int i;
	for (i = 0; i < connection->nack_queue_size; ++i) {
		if (connection->nack_queue[i] == sequence_number) {
			return 1;
		}
	}
	return 0;
}

void deduct_raknet_nack_queue(uint32_t sequence_number, connection_t *connection)
{
	if (is_in_raknet_nack_queue(sequence_number, connection) == 1) {
		int i;
		uint32_t *nack_queue = (uint32_t *) malloc((connection->nack_queue_size - 1) * sizeof(uint32_t));
		int nack_queue_size = 0;
		for (i = 0; i < connection->nack_queue_size; ++i) {
			if (connection->nack_queue[i] != sequence_number) {
				nack_queue[nack_queue_size] = connection->nack_queue[i];
				++nack_queue_size;
			}
		}
		free(connection->nack_queue);
		connection->nack_queue = nack_queue;
		--connection->nack_queue_size;
	}
}

void append_raknet_nack_queue(uint32_t sequence_number, connection_t *connection)
{
	if (is_in_raknet_nack_queue(sequence_number, connection) == 0) {
		++connection->nack_queue_size;
		connection->nack_queue = (uint32_t *) realloc(connection->nack_queue, connection->nack_queue_size * sizeof(uint32_t));
		connection->nack_queue[connection->nack_queue_size - 1] = sequence_number;
	}
}

void send_raknet_ack_queue(connection_t *connection, raknet_server_t *server)
{
	if (connection->ack_queue_size > 0) {
		packet_acknowledge_t acknowledge;
		acknowledge.sequence_numbers = connection->ack_queue;
		acknowledge.sequence_numbers_count = connection->ack_queue_size;
		socket_data_t output_socket_data;
		output_socket_data.stream.buffer = (char *) malloc(0);
		output_socket_data.stream.offset = 0;
		output_socket_data.stream.size = 0;
		put_packet_acknowledge(acknowledge, 0, ((&(output_socket_data.stream))));
		output_socket_data.address = connection->address;
		send_data(server->sock, output_socket_data);
		free(output_socket_data.stream.buffer);
		memset(&output_socket_data, 0, sizeof(socket_data_t));
		connection->ack_queue = (uint32_t *) realloc(connection->ack_queue, 0);
		connection->ack_queue_size = 0;
	}
}

void send_raknet_nack_queue(connection_t *connection, raknet_server_t *server)
{
	if (connection->nack_queue_size > 0) {
		packet_acknowledge_t acknowledge;
		acknowledge.sequence_numbers = connection->nack_queue;
		acknowledge.sequence_numbers_count = connection->nack_queue_size;
		socket_data_t output_socket_data;
		output_socket_data.stream.buffer = (char *) malloc(0);
		output_socket_data.stream.offset = 0;
		output_socket_data.stream.size = 0;
		put_packet_acknowledge(acknowledge, 1, ((&(output_socket_data.stream))));
		output_socket_data.address = connection->address;
		send_data(server->sock, output_socket_data);
		free(output_socket_data.stream.buffer);
		memset(&output_socket_data, 0, sizeof(socket_data_t));
		connection->nack_queue = (uint32_t *) realloc(connection->nack_queue, 0);
		connection->nack_queue_size = 0;
	}
}

void send_raknet_queue(connection_t *connection, raknet_server_t *server)
{
	if (connection->queue.frames_count > 0) {
		connection->queue.sequence_number = connection->sender_sequence_number;
		++connection->sender_sequence_number;
		append_raknet_recovery_queue(connection->queue, connection);
		socket_data_t output_socket_data;
		output_socket_data.stream.buffer = (char *) malloc(0);
		output_socket_data.stream.offset = 0;
		output_socket_data.stream.size = 0;
		put_packet_frame_set(connection->queue, ((&(output_socket_data.stream))));
		output_socket_data.address = connection->address;
		send_data(server->sock, output_socket_data);
		free(output_socket_data.stream.buffer);
		memset(&output_socket_data, 0, sizeof(socket_data_t));
		connection->queue.frames = (misc_frame_t *) malloc(0); /* mark */
		connection->queue.frames_count = 0;
	}
}

void append_raknet_frame(misc_frame_t frame, int opts, connection_t *connection, raknet_server_t *server)
{
	if (opts != 0) {
		packet_frame_set_t frame_set;
		frame_set.frames = (misc_frame_t *) malloc(sizeof(misc_frame_t));
		frame_set.frames_count = 1;
		frame_set.frames[0] = frame;
		frame_set.sequence_number = connection->sender_sequence_number;
		++connection->sender_sequence_number;
		append_raknet_recovery_queue(frame_set, connection);
		socket_data_t output_socket_data;
		output_socket_data.stream.buffer = (char *) malloc(0);
		output_socket_data.stream.offset = 0;
		output_socket_data.stream.size = 0;
		put_packet_frame_set(frame_set, ((&(output_socket_data.stream))));
		output_socket_data.address = connection->address;
		send_data(server->sock, output_socket_data);
		free(output_socket_data.stream.buffer);
		memset(&output_socket_data, 0, sizeof(socket_data_t));
	} else {
		int size = get_frame_size(frame);
		int i;
		for (i = 0; i < connection->queue.frames_count; ++i) {
			size += get_frame_size(connection->queue.frames[i]);
		}
		if (size >= connection->mtu_size) {
			send_raknet_queue(connection, server);
		}
		++connection->queue.frames_count;
		connection->queue.frames = (misc_frame_t *) realloc(connection->queue.frames, connection->queue.frames_count * sizeof(misc_frame_t));
		connection->queue.frames[connection->queue.frames_count - 1] = frame;
	}
}

void add_to_raknet_queue(misc_frame_t frame, connection_t *connection, raknet_server_t *server)
{
	/* printf("<- 0x%X\n", frame.stream.buffer[0] & 0xff); */
	if (is_ordered(frame.reliability) == 1) {
		frame.ordered_frame_index = connection->sender_order_channels[frame.order_channel];
		++connection->sender_order_channels[frame.order_channel];
	} else if (is_sequenced(frame.reliability) == 1) {
		frame.ordered_frame_index = connection->sender_order_channels[frame.order_channel];
		frame.ordered_frame_index = connection->sender_sequence_channels[frame.order_channel];
		++connection->sender_sequence_channels[frame.order_channel];
	}
	int max_size = get_frame_size(frame) - 60; 
	if (max_size > connection->mtu_size) {
		int frame_count = (frame.stream.size / max_size) + 1;
		int pad_bytes = (frame_count * max_size) - frame.stream.size;
		int i;
		for (i = 0; i < frame_count; ++i) {
			misc_frame_t compound_entry;
			compound_entry.is_fragmented = 1;
			compound_entry.reliability = frame.reliability;
			compound_entry.compound_id = connection->compound_id;
			compound_entry.compound_size = frame_count;
			compound_entry.index = i;
			compound_entry.stream.offset = 0;
			if (i == (frame_count - 1)) {
				compound_entry.stream.size = max_size - pad_bytes;
			} else {
				compound_entry.stream.size = max_size;
			}
			compound_entry.stream.buffer = get_bytes(compound_entry.stream.size, ((&(frame.stream))));
			if (is_reliable(frame.reliability) == 1) {
				compound_entry.reliable_frame_index = connection->sender_reliable_frame_index;
				++connection->sender_reliable_frame_index;
			}
			if (is_sequenced_or_ordered(frame.reliability) == 1) {
				compound_entry.ordered_frame_index = frame.ordered_frame_index;
				compound_entry.order_channel = frame.order_channel;
			}
			if (is_sequenced(frame.reliability) == 1) {
				compound_entry.sequenced_frame_index = frame.sequenced_frame_index;
			}
			append_raknet_frame(compound_entry, 1, connection, server);
		}
		++connection->compound_id;
		free(frame.stream.buffer);
		memset(&frame, 0, sizeof(misc_frame_t));
	} else {
		if (is_reliable(frame.reliability) == 1) {
			frame.reliable_frame_index = connection->sender_reliable_frame_index;
			++connection->sender_reliable_frame_index;
		}
		append_raknet_frame(frame, 0, connection, server);
	}
}

char is_in_raknet_frame_holder(uint16_t compound_id, uint32_t index, connection_t *connection)
{
	int i;
	for (i = 0; i < connection->frame_holder_size; ++i) {
		if (connection->frame_holder[i].compound_id == compound_id && connection->frame_holder[i].index == index) {
			return 1;
		}
	}
	return 0;
}

void append_raknet_frame_holder(misc_frame_t frame, connection_t *connection)
{
	if (is_in_raknet_frame_holder(frame.compound_id, frame.index, connection) == 0) {
		++connection->frame_holder_size;
		connection->frame_holder = (misc_frame_t *) realloc(connection->frame_holder, connection->frame_holder_size * sizeof(misc_frame_t));
		connection->frame_holder[connection->frame_holder_size - 1] = frame;
	}
}

int get_raknet_compound_size(uint16_t compound_id, connection_t *connection)
{
	int size = 0;
	int i;
	for (i = 0; i < connection->frame_holder_size; ++i) {
		if (connection->frame_holder[i].compound_id == compound_id) {
			++size;
		}
	}
	return size;
}

misc_frame_t pop_raknet_compound_entry(uint16_t compound_id, uint32_t index, connection_t *connection)
{
	if (is_in_raknet_frame_holder(compound_id, index, connection) == 1) {
		misc_frame_t *frame_holder = (misc_frame_t *) malloc((connection->frame_holder_size - 1) * sizeof(misc_frame_t));
		int frame_holder_size = 0;
		misc_frame_t output_frame;
		int i;
		for (i = 0; i < connection->frame_holder_size; ++i) {
			if (connection->frame_holder[i].compound_id != compound_id || connection->frame_holder[i].index != index) {
				frame_holder[frame_holder_size] = connection->frame_holder[i];
				++frame_holder_size;
			} else {
				output_frame = connection->frame_holder[i];
			}
		}
		free(connection->frame_holder);
		connection->frame_holder = frame_holder;
		--connection->frame_holder_size;
		return output_frame;
	}
	misc_frame_t frame;
	frame.is_fragmented = 0;
	frame.reliability = 0;
	frame.stream.buffer = NULL;
	frame.stream.size = 0;
	frame.stream.offset = 0;
	return frame;
}

void disconnect_raknet_client(connection_t *connection, raknet_server_t *server)
{
	server->on_disconnect_notification_executor(connection->address);
	misc_frame_t frame;
	frame.is_fragmented = 0;
	frame.reliability = 0;
	frame.stream.buffer = (char *) malloc(1);
	frame.stream.size = 1;
	frame.stream.offset = 0;
	frame.stream.buffer[0] = ID_DISCONNECT_NOTIFICATION;
	append_raknet_frame(frame, 1, connection, server);
	remove_raknet_connection(connection->address, server);
	/* exit(0); */
}

void handle_raknet_packet(raknet_server_t *server)
{
	socket_data_t socket_data = receive_data(server->sock);
	if (socket_data.stream.size > 0) {
		/* Just a debug thing
			int i;
			for (i = 0; i < socket_data.stream.size; ++i) {
				printf("0x%X ", socket_data.stream.buffer[i] & 0xff);
			}
			printf("\n");
		*/
		if ((socket_data.stream.buffer[0] & 0xff) == ID_UNCONNECTED_PING)
		{
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_unconneted_ping((&(socket_data.stream)), server);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_1) {
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_open_connection_request_1((&(socket_data.stream)), server);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_2) {
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_open_connection_request_2((&(socket_data.stream)), server, socket_data.address);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_ACK) {
			connection_t *connection = get_raknet_connection(socket_data.address, server);
			if (connection != NULL) {
				handle_ack((&(socket_data.stream)), server, connection);
			}
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_NACK) {
			connection_t *connection = get_raknet_connection(socket_data.address, server);
			if (connection != NULL) {
				handle_nack((&(socket_data.stream)), server, connection);
			}
		} else if ((socket_data.stream.buffer[0] & ID_FRAME_SET) != 0) {
			connection_t *connection = get_raknet_connection(socket_data.address, server);
			if (connection != NULL) {
				handle_frame_set((&(socket_data.stream)), server, connection);
			}
		}
	}
	free(socket_data.stream.buffer);
	memset(&socket_data, 0, sizeof(socket_data_t));
	int i;
	for (i = 0; i < server->connections_count; ++i) {
		send_raknet_ack_queue((&(server->connections[i])), server);
		send_raknet_nack_queue((&(server->connections[i])), server);
		send_raknet_queue((&(server->connections[i])), server);
	}
}
