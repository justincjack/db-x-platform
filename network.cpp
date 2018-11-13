//
//  network.c
//  deedlecore project - librtp.a
//
//  Created by Justin Jack on 8/13/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#include "network.h"
#include "db_x_platform.h"


#ifdef __APPLE__
pthread_t start_network( struct _audio_buffer **audio ){
#else
HANDLE start_network(struct _audio_buffer **audio) {
time_t timeout = 0;
#endif
    struct _audio_buffer *ab_test = *audio;
    if (!ab_test) {
        return 0;
    }
    if (network_thread_id != 0) {
        return 0;
    }
    network_run_status = 1;
	ab_test->udp_socket = 0;
#ifdef __APPLE__
    if (!pthread_create(&network_thread_id, 0, &network_thread, (void *)audio)) {
        pthread_create(&network_send_thread_id, 0, &network_send_thread, (void *)audio);
        return network_thread_id;
    }
#else
	network_thread_id = CreateThread(0, 0, &network_thread, (void *)audio, 0, &actual_network_thread_id);
	network_send_thread_id = 0;
	if (!network_thread_id) {
		goto ts_fail;
	}
	timeout = time(0) + 2;
	while (timeout > time(0)) {
		if (ab_test->udp_socket > 0) {
//            printf("network.cpp > start_network(): UDP socket is good.  Starting network_send_thread()\n\n");
			network_send_thread_id = CreateThread(0, 0, &network_send_thread, (void *)audio, 0, &actual_network_send_thread_id);
			if (network_send_thread_id) {
				return network_thread_id;
			}
			break;
		}
	}
#endif
    
ts_fail:
#ifdef _WIN32
	printf("network.cpp > start_network(): * Failed to start network!\n");
	actual_network_send_thread_id = 0;
	actual_network_thread_id = 0;
#endif
    network_run_status = 0;
    network_thread_id = 0;
    return 0;
}

int network_running(void) {
    return ((network_thread_id != 0) ? 1 : 0);
}

int stop_network(void) {
    if(network_thread_id == 0) {
        return 0;
    }

#ifdef _WIN32
	if (GetCurrentThreadId() == actual_network_thread_id) return 0;
#else
    if (pthread_self() == network_thread_id) return 0;
#endif
    network_run_status = 0;
#ifdef __APPLE__
    pthread_join(network_thread_id, 0);
    pthread_join(network_send_thread_id, 0);
#else
	WaitForSingleObject(network_thread, 5000);
	WaitForSingleObject(network_send_thread_id, 5000);
#endif
    network_send_thread_id = 0;
    network_thread_id = 0;
    return 1;
}

#ifdef __APPLE__
void *network_send_thread( void *parameter ) {
#else
DWORD WINAPI network_send_thread(void *parameter) {
#endif
    int i = 0, j = 0;
    struct _incoming_connection *connection_buffer = 0;
    struct _audio_buffer **main_audio_variable_address = (struct _audio_buffer **)parameter;
    struct _audio_buffer *audio = 0;
#ifdef __APPLE__
    socklen_t socklen = 0;
#else
	int socklen = 0;
#endif
    //static unsigned long ctr = 0;
    size_t ob_stream_count = 0;
    char rtp[172];
    char *ulaw = (char *)&((char*)&rtp)[12];
    unsigned long *rtp1 = 0, *rtp2 = 0, *rtp3 = 0;
    rtp1 = (unsigned long *)&rtp[0];
    rtp2 = (unsigned long *)&rtp[4];
    rtp3 = (unsigned long *)&rtp[8];
	int bytes_sent = 0;
    //char ipdest[100];
    
//    time_t timeout = time(0) + 3;
    
    audio = *main_audio_variable_address;
    
//    printf("network.cpp > network_send_thread(): Entering main loop\n\n");

    while (network_run_status) {
        if (audio->clock_sync == 0) {
            Sleep(10);
            continue;
        }
        db_sem_wait(audio->clock_sync);
        
        //db_lock_mutex(&audio->buffer_mutex);
        ob_stream_count = 0;
        for (i = 0; i < UDP_MAX_CONNECTIONS; i++) {
            connection_buffer = &audio->connection_list[i];
            if ( (time(0) - connection_buffer->inuse) <= RTP_AUDIO_TIMEOUT ) {
                ob_stream_count++;
                /* BUG FIX: This was using "writeblock_out", I switched it to readblock_out */
                while (connection_buffer->outbound_samples[connection_buffer->readblock_out].buffer_status == SAMPLE_BUFFER_READY) {
                    socklen = sizeof(struct sockaddr);

                    /* Set RTP header info */
                    *rtp1 = (htons(connection_buffer->media_index) << 16) | 128;
                    *rtp1 |= htons(0 /* Codec ULAW */);
                    *rtp2 = htonl(connection_buffer->media_time_stamp);
                    *rtp3 = htonl(connection_buffer->audio_stream_id);
                    
                    /* Convert to uLAW */
                    for (j = 0; j < 160; j++) {
                        ulaw[j] = linear2ulaw(connection_buffer->outbound_samples[connection_buffer->readblock_out].pcm[j]);
                    }
                    /*
                    db_inet_ntop(AF_INET, &connection_buffer->client_address.sin_addr, ipdest, 100);
                    printf("Sending RTP to %s:%u\n", ipdest, ntohs(connection_buffer->client_address.sin_port));
                    */
#ifdef __APPLE__
                    bytes_sent = (int)sendto(audio->udp_socket, &rtp, 172, 0, (struct sockaddr *)&connection_buffer->client_address, socklen);
#else
                    bytes_sent = sendto(audio->udp_socket, (const char *)&rtp, 172, 0, (struct sockaddr *)&connection_buffer->client_address, socklen);
                    if (bytes_sent == SOCKET_ERROR) {
                        OutputDebugString("sendto(): ");
                        switch (WSAGetLastError()) {
                            case WSANOTINITIALISED:
                                OutputDebugString("WSANOTINITIALISED\n");
                                break;
                            case WSAENETDOWN:
                                OutputDebugString("WSAENETDOWN\n");
                                break;
                            case WSAEACCES:
                                OutputDebugString("WSAEACCES\n");
                                break;
                            case WSAEINVAL:
                                OutputDebugString("WSAEINVAL\n");
                                break;
                            case WSAEINTR:
                                OutputDebugString("WSAEINTR\n");
                                break;
                            case WSAEINPROGRESS:
                                OutputDebugString("WSAEINPROGRESS\n");
                                break;
                            case WSAEFAULT:
                                OutputDebugString("WSAEFAULT\n");
                                break;
                            case WSAENETRESET:
                                OutputDebugString("WSAENETRESET\n");
                                break;
                            case WSAENOBUFS:
                                OutputDebugString("WSAENOBUFS\n");
                                break;
                            case WSAENOTCONN:
                                OutputDebugString("WSAENOTCONN\n");
                                break;
                            case WSAENOTSOCK:
                                OutputDebugString("WSAENOTSOCK\n");
                                break;
                            case WSAEOPNOTSUPP:
                                OutputDebugString("WSAEOPNOTSUPP\n");
                                break;
                            case WSAESHUTDOWN:
                                OutputDebugString("WSAESHUTDOWN\n");
                                break;
                            case WSAEWOULDBLOCK:
                                OutputDebugString("WSAEWOULDBLOCK\n");
                                break;
                            case WSAEMSGSIZE:
                                OutputDebugString("WSAEMSGSIZE\n");
                                break;
                            case WSAEHOSTUNREACH:
                                OutputDebugString("WSAEHOSTUNREACH\n");
                                break;
                            case WSAECONNABORTED:
                                OutputDebugString("WSAECONNABORTED\n");
                                break;
                            case WSAECONNRESET:
                                OutputDebugString("WSAECONNRESET\n");
                                break;
                            case WSAEADDRNOTAVAIL:
                                OutputDebugString("WSAEADDRNOTAVAIL\n");
                                break;
                            case WSAEAFNOSUPPORT:
                                OutputDebugString("WSAEAFNOSUPPORT\n");
                                break;
                            case WSAEDESTADDRREQ:
                                OutputDebugString("WSAEDESTADDRREQ\n");
                                break;
                            case WSAENETUNREACH:
                                OutputDebugString("WSAENETUNREACH\n");
                                break;
                            case WSAETIMEDOUT:
                                OutputDebugString("WSAETIMEDOUT\n");
                                break;
                            default:
                                OutputDebugString("UNKNOWN ERROR\n");
                                break;
                        }
                    }
#endif

                    if (connection_buffer->started_at == 0) connection_buffer->started_at = time(0);
                    
                    /* Increment and/or reset counters */
                    connection_buffer->media_time_stamp+=160;
                    if ( connection_buffer->media_time_stamp > 429496600 ) {
                        connection_buffer->media_time_stamp = 20000;
                    }
                    
                    /* Increment and/or reset packet Sequence Number */
                    if (++connection_buffer->media_index > 0xFFFF) connection_buffer->media_index = 1;
                    memset(connection_buffer->outbound_samples[connection_buffer->readblock_out].pcm, 0, 320);
                    connection_buffer->outbound_samples[connection_buffer->readblock_out].buffer_status = SAMPLE_BUFFER_EMPTY;
                    if (++connection_buffer->readblock_out == AUDIO_BUFFER_SIZE ) connection_buffer->readblock_out = 0;
                }
            }
        }
        audio->outbound_stream_count = ob_stream_count;
        //db_unlock_mutex(&audio->buffer_mutex);
    }
//    printf("network.cpp > network_send_thread(): Exiting thread...\n\n");
    return 0;
}

    /* network thread, receive data receive audio */
#ifdef __APPLE__
void *network_thread( void *parameter ) {
#else
DWORD WINAPI network_thread(void *parameter) {
#endif
    /* Create our socket and listen for audio streams */
    struct sockaddr_in client;
#ifdef __APPLE__
    ssize_t bytesrecd = 0;
    socklen_t socklen = 0;
#else
	size_t bytesrecd = 0;
	int socklen = 0;
#endif
    size_t i = 0;
    struct _audio_buffer **main_audio_variable_address = (struct _audio_buffer **)parameter;
    struct _audio_buffer *audio = 0;
    fd_set fdread;
    struct timeval tv;
    struct _incoming_connection *connection_check = 0;
    struct _incoming_connection *connection_buffer = 0;
    int connection_found = 0, select_result = 0;
    long long avg_amplitude;
    //struct _rtp_packet rtp;
    short pcmbuffer[160];
    AUDIO_STREAM_ID ssrc = 0;
    char rtp[172];
    char *ulaw = (char *)&((char*)&rtp)[12];
    AUDIO_STREAM_ID *pssrc = (AUDIO_STREAM_ID *)&rtp[8];
    unsigned long t_stamp;
    unsigned long *ptimestamp = (unsigned long *)&rtp[4];
    unsigned char *version = (unsigned char *)&rtp;
    unsigned char ver = 0;
    
    unsigned short *pseq = (unsigned short *)(&rtp[2]);
    unsigned short seq = 0;

//    printf("network.cpp > network_thread()\n\n");

    audio = *main_audio_variable_address;
    audio->udp_socket = openudpsocket(INTERCOM_PORT, 0);
    
    if (audio->udp_socket == 0) {
        network_run_status = 0;
        network_thread_id = 0;
		printf("network.cpp > network_thread(): Failed to open UDP socket.\n");
	} 

    while (network_run_status) {
        if ( audio->udp_socket <= 0) {
            Sleep(500);
            printf("** Attempting to re-open UDP socket for receiving RTP...\n");
            audio->udp_socket = openudpsocket(INTERCOM_PORT, 0);
            if ( audio->udp_socket <= 0) {
                continue;
            } else {
                printf("\tSuccess.\n");
            }
        }

        
        if ( /*The variable \"audio\" in main has been set to ZERO */ *main_audio_variable_address == 0) {
            printf("- Shutting down network because \"audio\" in main() is equal to 0 indicating the \"audio\" object is no longer valid\n\n");
            network_run_status = 0;
        } else {
            
            // Loop and read all inbound data
            FD_ZERO(&fdread);
            FD_SET(audio->udp_socket, &fdread);
            tv.tv_sec = 0;
            tv.tv_usec = 5000;
            select_result = select((int)(audio->udp_socket+1), &fdread, 0, 0, &tv) ;
            if ( select_result > 0) {
                /* Loop and read all inbound datat */
                socklen =  sizeof(struct sockaddr);
#ifdef __APPLE__
                bytesrecd = recvfrom(audio->udp_socket, (void *)&rtp, 172, 0, (struct sockaddr *)&client, &socklen);
#else
				bytesrecd = recvfrom(audio->udp_socket, rtp, 172, 0, (struct sockaddr *)&client, &socklen);
#endif
                if ( /*
                      There is no outstream configured, don't process this audio because it
                      will just back up the buffer
                      */
                    !audio->outstream) continue;
                
                /* Convert back to PCM */
                if (bytesrecd == 172 /* 12 Byte RTP Header + 160 bytes data */ ) {
                    ver = ((*version >> 6) & 0x03);
                    seq = ntohs(*pseq);
                    t_stamp = ntohl(*ptimestamp);
                    ssrc = ntohl(*pssrc);
                    for (i = 0; i < 160; i++) {
                        pcmbuffer[i] = ulaw2linear( ulaw[i] );
                    }
                    connection_buffer = 0;
                    connection_found = 0;
                    connection_check = 0;
                    /* Mutex protect **************************************************************/
                    //db_lock_mutex(&audio->buffer_mutex);
                    
                    
                    for (i = 0; i < UDP_MAX_CONNECTIONS; i++) {
                        connection_check = &audio->connection_list[i];
                        if (  /* This is an active connection, fewer than 5 seconds have ellapsed */  (time(0) - connection_check->inuse) <= RTP_AUDIO_TIMEOUT ) {
                            if ( /* The Sender's IP and port, matches SDP on a connection */
                                connection_check->client_address.sin_addr.s_addr ==
                                client.sin_addr.s_addr && connection_check->client_address.sin_port ==
                                client.sin_port) {
                                connection_buffer = connection_check;
                                connection_found = 1;
                                break;
                            }
                        }
                    }
                    
                    
                    
                    
                    if (connection_buffer) {
                        int sequence_ok = 0;
                        if (connection_buffer->started_at == 0) connection_buffer->started_at = time(0);
                        if (seq > connection_buffer->last_sequence) {
                            sequence_ok = 1;
                        } else {
                            printf("Out-of-sequence audio frame...\n");
                        }
                        connection_buffer->last_sequence = seq;
                        connection_buffer->last_time_stamp = t_stamp;

                        if (sequence_ok == 1) {
                            connection_buffer->inuse = time(0);
                            if (connection_buffer->incoming_samples[connection_buffer->writeblock].buffer_status == SAMPLE_BUFFER_EMPTY) {
                                connection_buffer->incoming_samples[connection_buffer->writeblock].buffer_status = SAMPLE_BUFFER_FILLING;
                                avg_amplitude = 0;
                                for (i = 0; i < 160; i++) {
                                    connection_buffer->incoming_samples[connection_buffer->writeblock].pcm[i] = pcmbuffer[i];
                                    avg_amplitude+=ABS(pcmbuffer[i]);
                                }
                                connection_buffer->incoming_samples[connection_buffer->writeblock].average_amplitude = (short)(avg_amplitude/160);
                                connection_buffer->incoming_samples[connection_buffer->writeblock].buffer_status = SAMPLE_BUFFER_READY;
                                if (++connection_buffer->writeblock == AUDIO_BUFFER_SIZE) connection_buffer->writeblock = 0;
                            } else {
                                printf("Cannot fill buffer %d (port: %u ) ", (int)i, ntohs(connection_buffer->client_address.sin_port) );
                                switch (connection_buffer->incoming_samples[connection_buffer->writeblock].buffer_status) {
                                    case SAMPLE_BUFFER_FILLING:
                                        printf(" BUFFER FILLING\n");
                                        break;
                                    case SAMPLE_BUFFER_EMPTY:
                                        printf(" BUFFER EMPTY\n");
                                        break;
                                    case SAMPLE_BUFFER_READY:
                                        printf(" BUFFER READY (to play)\n");
                                        break;
                                    default:
                                        printf(" INVALID STATE\n");
                                        break;
                                }
                                //printf("\tCannot fill buffer, it is backed up! Flushing audio buffers!\n\n");
                                flush_all_audio_buffers(audio);
                            }
                        }
                        
                        
                        
                    }
                    /*********************************************************************************/
                    //db_unlock_mutex(&audio->buffer_mutex);
                } /*else {
                    unsigned char *prtcpver, rtcpver;
                    unsigned char *prtcppadding, rtcppadding;
                    unsigned char *prtcprc, rtcptp;
                    unsigned char *prtcppt, rtcppt;
                    unsigned short *prtcplength, rtcplength;
                    unsigned long *prtcpssrc, rtcpssrc;
                    
                    
                    printf("RTP Rec'd %lu bytes and we're looking for 172 bytes...\n", bytesrecd);
                }*/
            } else {
                if (select_result == -1) {
                    printf("** Attempting to re-open UDP socket for receiving RTP...\n");
                    audio->udp_socket = openudpsocket(INTERCOM_PORT, 0);
                    if ( audio->udp_socket > 0) {
                        printf("\tSuccess.\n");
                    }
                }
            }
        }
    }
//    printf("network.cpp > network_thread(): Exiting network_thread()\n\n");
    db_sem_post(audio->clock_sync);
    network_run_status = 0;
    closesocket(audio->udp_socket);
#ifdef __APPLE__
    return &net_thread_return_value;
#else
	return net_thread_return_value;
#endif
}




















