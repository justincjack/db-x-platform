//
//  main.cpp
//  Deedle Core
//
//  Created by Justin Jack on 8/17/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//  Rev: 2


//#define _CHECK_RTP 1
#define DC_BUILD "1.1.42"
#define DEEDLEX_PORT 8555

int deedlecore_running = 1;


#include <stdio.h>
#ifdef __APPLE__
#include "/Users/justinjack/Documents/signal/signal.h"
#else
#define _WINDOWS_LEAN_AND_MEAN
#include "c:\Users\justinjack\source\repos\signal\signal\signal.h"
#endif
#include "deedle_command.h"

DEEDLE_COMMAND *config = 0;
RTPENGINE *rtp = 0;
SIGNAL_SETTINGS ss;

void notify_register(SIGNAL *signal, int r) {
    if (r > 0) {
        config->signal = signal;
        config->advice_needed = 1;
    } else if (r == 0) {
        config->signal = 0;
    } else if (r < 0) {
        config->signal = 0;
    }
}

void ring_alert(int ringer) {
    const char ringing[] = "deedlecore:type=ringing\r\n";
    const char notringing[] = "deedlecore:type=not_ringing\r\n";
    if (ringer) {
        send_to_deedlex(config, (char *)ringing, sizeof(ringing));
//        printf("main.cpp > ring_alert(): Ringing...\n");
    } else {
        send_to_deedlex(config, (char *)notringing, sizeof(notringing));
//        printf("main.cpp > ring_alert(): Stop Ringing...\n");
    }
}



void call_change(SIGNAL *signal, CALLINFO *calllist) {
    char call_change_string[3000];
    
    if (calllist->is_incoming) {
        if (calllist->is_intercom) {
            sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=1&did=%d&status=%s&caller_name=%s&call_missed=%d&uniqueid=%s\r\n",
                    calllist->callid,
                    calllist->talking_to_number,
                    signal->user_info.extension,
                    signal->status_to_text(calllist->status),
                    calllist->talking_to_name,
                    calllist->call_missed,
                    calllist->uniqueid
                    );
        } else {
            sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=0&did=%s&status=%s&caller_name=%s&call_missed=%d&uniqueid=%s\r\n",
                    calllist->callid,
                    calllist->talking_to_number,
                    calllist->did,
                    signal->status_to_text(calllist->status),
                    calllist->talking_to_name,
                    calllist->call_missed,
                    calllist->uniqueid
                    );
        }
    } else {
        if (calllist->is_intercom) {
            sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=1&did=%s&status=%s&caller_name=%s&call_missed=%d&uniqueid=%s\r\n",
                    calllist->callid,
                    calllist->talking_to_number,
                    ((strlen(calllist->did) > 0)?calllist->did:signal->user_info.callerid),
                    signal->status_to_text(calllist->status),
                    calllist->talking_to_name,
                    calllist->call_missed,
                    calllist->uniqueid
                    );
        } else {
            sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=0&did=%s&status=%s&caller_name=%s&call_missed=%d&uniqueid=%s\r\n",
                    calllist->callid,
                    calllist->talking_to_number,
                    ((strlen(calllist->did) > 0)?calllist->did:signal->user_info.callerid),
                    signal->status_to_text(calllist->status),
                    calllist->talking_to_name,
                    calllist->call_missed,
                    calllist->uniqueid
                    );
        }
    }
    if (strlen(call_change_string) > 0) {
        char *call_list_send = 0;
        char *cl_string = 0;
        size_t cl_length  = 0;
        send_to_deedlex(config, call_change_string, strlen(call_change_string));
        
        cl_string = 0;
        cl_length = make_call_list(signal, &cl_string);
        if (cl_string) {
            call_list_send = (char *)calloc(100 + cl_length, 1);
            if (call_list_send) {
                sprintf(call_list_send, "deedlecore:type=call_list&call_list=%s\r\n", cl_string);
                send_to_deedlex(config, call_list_send, strlen(call_list_send));
                free(call_list_send);
            }
            free(cl_string);
        }
    } else {
        printf("main.cpp > call_change(): No \"call_change_string\" to send...\n");
    }
    if (calllist->status == RINGING_IN) {
        if (calllist->is_intercom) {
            /*
             Auto Answer:
             
             1. Do not disturb is off
             
             2. If they are not on the phone.
             */
            if (signal->do_not_disturb == 0) {
                if (signal->calls_talking == 0) {
                    signal->answer(calllist->callid);
                }
            }
        }
    }
    free(calllist);
}

void todo(void) {
    printf("\nStuff still todo:\n");
    printf("-------------------------\n");
    printf("- Not much\n");
    printf("\n\n");
}

int audio_is_ready = 0;

void audio_ready(void *_rtpengine) {
//    RTPENGINE *rtp = (RTPENGINE *)_rtpengine;
//    if (rtp) {
//        printf("****************** Audio engine ready...\n");
//        printf("%s\n\n",rtp->json_audio_device_list());
//    }
    audio_is_ready = 1;
}

void wait(void) {
    printf("\nPress [ENTER] to continue...");
    while (getchar() != 0x0a) Sleep(1);
}

//int db_create_thread( DB_THREAD_PROC start_routine, DB_THREAD_PARAM param, DB_THREAD *thread) ;




DB_THREAD_PROC deedlex( void *param ) {
    
    return 0;
}



void device_change(int input_device, int output_device, int ring_device, char *device_list) {
    char notify_device_change[1000];
    sprintf(notify_device_change, "deedlecore:type=device_change&input_device=%d&output_device=%d&ring_device=%d&device_list=%s\r\n",
            input_device,
            output_device,
            ring_device,
            device_list);
    send_to_deedlex(config, notify_device_change, strlen(notify_device_change));
    return;
}





int main(int argc, const char **argv) {
    SIGNAL *signal = 0;
    int extension = 0;
    char deedlex[] = "hostfordeedlex.com";

    
    config = init_deedle_command();
    config->configuration.deedle_server_ip_list[0] = deedlex;
    config->configuration.deedle_server_count++;
    config->configuration.deedlex_port = DEEDLEX_PORT;
    sprintf(config->build_version, "DeedleCore Build %s, (c) 2018 O'Brien Garage Doors.", DC_BUILD);
    
    fclose(stderr);
    
    rtp = new RTPENGINE(&audio_ready);
    config->rtp = rtp;
    
    /***** From Here (We are setting up a SIGNAL object (SIP endpoint / UAC ) ***************/
    memset(&ss, 0, sizeof(SIGNAL_SETTINGS));
    
    ss.extension = 0;
    ss.SIP_server_port = 5060;
    ss.do_not_disturb = 1;
    sprintf(ss.auth.password, "rU12nRPn");

//    sprintf(ss.userinfo.callerid, "8175726610");
//    sprintf(ss.userinfo.username, "Justin Jack");
    
    
    /* Callbacks */
    ss.callbacks.call_status_change_callback = &call_change;
    ss.callbacks.registered_callback = &notify_register;
    ss.callbacks.ringing_status_callback = &ring_alert;
    ss.callbacks.voicemail_callback = &voicemail_change;
    ss.rtp = rtp;
    
    
    /**** To Here   ****************************************************************************************************/
    
#ifdef _CHECK_RTP
    timeout = time(0) + 3;
    while (!audio_is_ready) {
        Sleep(50);
        if (time(0) > timeout) {
            printf("Timeout waiting on RTPENGINE to start up....aborting...\n\n");
            break;
        }
    }
    
    if (time(0) > timeout) goto quit;
#endif
    
    
    rtp->set_device_change_callback(&device_change);
    
//    signal = new SIGNAL(&ss);
    
    
    
    
    
    
    
    
    
    if (argc > 1) {
        int lenarg = 0;
        char *arg = (char *)argv[1];
        char *end_arg = 0;
        lenarg = (int)strlen(argv[1]);
        end_arg = &arg[lenarg];
        sprintf(config->product_code, "%.*s", ((lenarg < 100)?lenarg:99), argv[1]);
    } else {
        sprintf(config->product_code, "BAD-CODE");
    }

    
    
    
    add_signal_server((char *)"deedleX.com", config);
    add_signal_server((char *)"deedleY.com", config);
    
    strcpy(ss.SIP_server_IP, get_signal_server(config));
    sprintf(ss.auth.password, "All3y3z0nM3");
    
    config->signal = 0;
    config->extension = 0;
    //config->iplist = signal->json_ip_list;
    config->iplist = (char *)"[]\0";
    
    signal = 0;
    config->signal = signal;
    
    if (!start_deedle_command_thread(config)) {
        printf("Failed to start deedle command thread!!!!\n");
        return 0;
    }

    while (deedlecore_running) {
        // Make sure things are going as they're supposed to in here...
        if (config->shutdown_deedlecore_flag == 1) {
            deedlecore_running = 0;
        } else {
            if (extension != config->extension ) {
                config->signal = 0;
                if (signal) {
                    delete signal;
                }
                signal = 0;
                ss.extension = config->extension;
                ss.do_not_disturb = 0;
                signal = new SIGNAL(&ss);
                extension = config->extension;
                config->signal = signal;
                config->advice_needed = 1;
            }
        }
        Sleep(20);
    }
    
//    todo();
//    do {
//        if (config->deedle_thread_running == 1) {
//            printf("%d@%s: ", ss.extension, ss.SIP_server_IP);
//        } else {
//            printf(":");
//        }
//        do {
//            memset(command, 0, 100);
//            fgets(command, 100, stdin);
//            cmd_length = strlen(command);
//            cmd = JSTRING::trim(command, &cmd_length, &cmd_length);
//        } while (cmd[0] == 0);
//        for (i = (int)(strlen(cmd) - 1); i >= 0; i--) {
//            if (cmd[i] == '\n' || cmd[i] == '\r') {
//                cmd[i] = 0;
//            } else {
//                break;
//            }
//        }
//        process_command(&signal, cmd);
//
//        if (signal) {
//            if (signal->shut_down) {
//                printf("*** SIGNAL has failed to connect using extension \"%d\"!!!\n\tDestroying this instance of the SIGNAL class...\n", ss.extension);
//                delete signal;
//                signal = 0;
//                continue;
//            }
//        }
//
//    } while ((strncmp(cmd, "quit", 4) && strncmp(cmd, "exit", 4)) && cmd[0] != 'q');
//
    
    if (signal) {
        delete signal;
    }
    
    
quit:
    delete rtp;
    stop_deedle_command_thread(config);
#ifdef _WIN32
    printf("Press [ENTER] to quit...");
    while (getchar() != 0x0a) Sleep(1);
#endif
    printf("\n");
    return 0;
}
