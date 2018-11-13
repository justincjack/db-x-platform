//
//  network.h
//  Portaudio intercom
//
//  Created by Justin Jack on 8/13/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#ifndef network_h
#define network_h

#include <stdio.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#else
#include <WinSock2.h>
#endif
#include <string.h>
#ifdef __APPLE__
#include <unistd.h>
#endif
#include <fcntl.h>
#include "audio.h"
#ifdef __APPLE__
#include <arpa/inet.h>
#include <sys/semaphore.h>
#include <sys/select.h>
#include <sys/time.h>
#endif
#define INTERCOM_PORT 8282

//#ifndef RTP_AUDIO_TIMEOUT
//#define RTP_AUDIO_TIMEOUT 10
//#endif

#ifdef __APPLE__
static pthread_t network_thread_id = 0;
static pthread_t network_send_thread_id = 0;
#else
static HANDLE network_thread_id = 0;
static HANDLE network_send_thread_id = 0;
static DWORD actual_network_thread_id = 0;
static DWORD actual_network_send_thread_id = 0;
#endif

static int network_run_status = 0;
static int net_thread_return_value = 1;

#ifdef __cplusplus
extern "C" {
#endif
    /* Functions go here */
    int network_running(void);
    int stop_network(void);
#ifdef __APPLE__
    pthread_t start_network( struct _audio_buffer **audio );
	void *network_thread(void *);
	void *network_send_thread(void *);
#else
	HANDLE start_network(struct _audio_buffer **audio);
	DWORD WINAPI network_thread(void *);
	DWORD WINAPI network_send_thread(void *);
#endif
    
    
    
#ifdef __cplusplus
}
#endif







#endif /* network_h */
