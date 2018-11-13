//
//  deedle_command.h
//  deedlecore
//
//  Created by Justin Jack on 9/17/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#ifndef deedle_command_h
#define deedle_command_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include "/Users/justinjack/Documents/Deedle Cross Platform Library/db_x_platform.h"
#include "/Users/justinjack/Documents/signal/signal.h"
#else
#define _WINDOWS_LEAN_AND_MEAN
#include <signal.h>
#include <db_x_platform.h>
#endif

typedef struct _gvm_request {
    int retrieving_group_voicemail;
    int collect_me;
    int done_getting_vm;
    int vm_waiting;
    int vm_new;
    int vm_new_urgent;
    int vm_old;
    int vm_old_urgent;
    SIGNAL *main_signal;
    char username[100];
    char password[100];
    char voicemail_server[100];
    int extension;
} GROUP_VOICEMAIL_REQUEST;

    typedef struct deedle_command {
        DB_THREAD command_thread;           // Handle or ID of our command thread;
        int deedle_thread_running;          // Flag to signal whether or not the command thread should run.
        SOCKET socket;                      // Connected socket to DEEDLEX module
        int socket_error_condition;
        time_t socket_retry;
        int extension;
        char *iplist;
        SIGNAL *signal;
        RTPENGINE *rtp;
        int advice_needed = 0;
        int shutdown_deedlecore_flag;
        char product_code[100];
        char build_version[200];
        char sip_server[100];
        struct {
            char *deedle_server_ip_list[50];// List of possible DeedleServer IPs or hostnames
            int deedle_server_count;        // Count of "deedle_server_ip_list"
            char signal_server_list[200][50];       // List of signalling server IPs or hostnames
            int signal_server_count;        // Count of "signal_server_list"
            char username[100];             // The name of the person logged in
            char password[100];             // The password from the configuration file
            int extension;                  // The numeric extension for this instance of Deedle
            unsigned long deedlex_ip;       // IP Address of DEEDLEX
            unsigned short deedlex_port;    // Port of DEEDLEX
        } configuration;
        int active_SIP_server;              // The index of the SIP server to which we're connected.
                                            // This should initialize as -1 and return to that if we
                                            // are not connected to a SIP server.
    } DEEDLE_COMMAND;
    
    SOCKET deedle_command_connect( unsigned long ip, unsigned short port );
    
    DEEDLE_COMMAND *init_deedle_command( void );
    
    int start_deedle_command_thread( DEEDLE_COMMAND *config );
    int stop_deedle_command_thread( DEEDLE_COMMAND *config );

    /* Make the string to send to DEEDLEX describing the phone's state */

    char *make_status_string(DEEDLE_COMMAND *config);

	//void *deedle_command_thread(void *param);

#ifdef __APPLE__
	void *deedle_command_thread(void *param);
#else
	DWORD deedle_command_thread(void *param);
#endif

    static GROUP_VOICEMAIL_REQUEST gvm_request, vm_request;


    void dc_socket_failure( DEEDLE_COMMAND *config );
    
    void send_to_deedlex( DEEDLE_COMMAND *config, char *buffer_to_send, size_t length );
    
    /* API Functions */
    void deedle_hangup_all( DEEDLE_COMMAND *config );   

    size_t make_call_list(SIGNAL *signal, char **calllist);

    void check_group_voicemail( SIGNAL *signal, char *voicemail_server, int extension, char *username, char *password );
    void *check_group_voicemail_thread(void *param);
    void voicemail_change(int extension, int messages_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent);
    void add_signal_server( char *signal_server, DEEDLE_COMMAND *config);
    char *get_signal_server( DEEDLE_COMMAND *config );
    char *get_next_server( DEEDLE_COMMAND *config );




#endif /* deedle_command_h */
