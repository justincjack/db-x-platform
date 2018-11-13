//
//  deedle_command.c
//  deedlecore
//
//  Created by Justin Jack on 9/17/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#include "deedle_command.h"
#ifdef __APPLE__
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

void voicemail_change(int extension, int messages_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent) {
    vm_request.vm_new = vm_new;
    vm_request.vm_new_urgent = vm_new_urgent;
    vm_request.vm_old = vm_old;
    vm_request.vm_waiting = messages_waiting;
    vm_request.vm_old_urgent = vm_old_urgent;
    vm_request.collect_me = 1;
}


SOCKET deedle_command_connect( unsigned long ip, unsigned short port ) {
    char ipaddress[50] = {0};
    if (db_inet_ntop(AF_INET, &ip, ipaddress, 50)) {
        return tcpconnect(ipaddress, port, 0);
    }
    printf("deedle_command.c > deedle_command_connect(): db_inet_ntop() failed.\n");
    return 0;
}

DEEDLE_COMMAND *init_deedle_command( void ) {
    memset(&gvm_request, 0, sizeof(GROUP_VOICEMAIL_REQUEST));
    memset(&vm_request, 0, sizeof(GROUP_VOICEMAIL_REQUEST));
    return (DEEDLE_COMMAND *)calloc(sizeof(DEEDLE_COMMAND), 1);
}

int start_deedle_command_thread( DEEDLE_COMMAND *config ) {
    int i = 0;
    if (!config) return 0;
    /* Check configuration options here */

    /* Get the IP Address to which we need to connect */
    if (config->configuration.deedlex_ip == 0) {
        for (i = 0; i < config->configuration.deedle_server_count; i++) {
            config->configuration.deedlex_ip =
            db_get_host_ip_address(config->configuration.deedle_server_ip_list[i]);
            if (config->configuration.deedlex_ip != 0) {
                break;
            }
        }
        if (config->configuration.deedlex_ip == 0) {
            printf("No IP address configured for DEEDLEX\n");
            return 0;
        }
    }
    
    if (config->configuration.deedlex_port == 0) {
        printf("Please specify the PORT on which DEEDLEX is listenting.\n");
        return 0;
    }
    
    
    config->deedle_thread_running = 1;
    if (db_create_thread(&deedle_command_thread, (void *)config, &config->command_thread) ) {
        return 1;
    }
    /* Thread creation failed */
    config->deedle_thread_running = 0;
    return 0;
}

void dc_socket_failure( DEEDLE_COMMAND *config ) {
    printf("Deedle Command socket failure...\n");
    config->socket_error_condition = 1;
    config->socket_retry = 0;
}

int stop_deedle_command_thread( DEEDLE_COMMAND *config ) {
    if (config->deedle_thread_running == 0) {
        return 1;
    }
    config->deedle_thread_running = 0;
    if (config->command_thread) {
        db_thread_join(&config->command_thread);
    }
    printf("Stopped deedle command thread!\n");
    return 1;
}

void send_to_deedlex( DEEDLE_COMMAND *config, char *buffer_to_send, size_t length ) {
    int result = 0;
    if (!config) return;
    if (length == 0) return;
    if (config->socket_error_condition == 1) {
        printf("* Cancelling \"send_to_deedlex\" due to command socket being in an error state.\n");
        if (!config->deedle_thread_running) {
            printf("\tstart_deedle_command_thread()\n");
            start_deedle_command_thread(config);
        }
        return;
    }
    if (config->socket) {
        result = (int)send(config->socket, buffer_to_send, (int)length, 0);
        if (result <= 0) {
            printf("send_to_deedlex() failed..\n");
            dc_socket_failure(config);
        } else {
//            printf("Sent \"%.*s\" (%d bytes)\n\n", (int)length, buffer_to_send, result);
            config->socket_error_condition = 0;
        }
    }
}

size_t make_call_list(SIGNAL *signal, char **calllist) {
    char *cl_string = 0;
    CALLINFO *call_list = 0;
    size_t call_count = 0, i = 0;
    const char call_obj[] = "{\"callid\":\"%s\", \"incoming\":%d ,\"intercom\":%d ,\"duration\":%zu, \"caller_name\":\"%s\", \"status\":\"%s\", \"talking_to\":\"%s\", \"did\":\"%s\", \"call_missed\":%d, \"uniqueid\":\"%s\" }";
    char call[sizeof(call_obj) + 500];
    if (!signal) return 0;
    *calllist = 0;
    call_count = signal->get_call_list(&call_list);
    if (call_count > 0) {
        cl_string = (char *)calloc( ( (strlen(call_obj)+1000) * call_count), 1);
        if (cl_string) {
            cl_string[0] = '[';
            for (i = 0; i < call_count; i++) {
                if (call_list[i].is_intercom && call_list[i].is_intercom) {
                    char did[100];
                    sprintf(did, "%d", signal->user_info.extension);
                    sprintf(call, call_obj,
                            call_list[i].callid,
                            call_list[i].is_incoming,
                            call_list[i].is_intercom,
                            time(0) - call_list[i].call_start_time,
                            call_list[i].talking_to_name,
                            signal->status_to_text(call_list[i].status),
                            call_list[i].talking_to_number,
                            did,
                            call_list[i].call_missed,
                            call_list[i].uniqueid);
                } else {
                    sprintf(call, call_obj,
                            call_list[i].callid,
                            call_list[i].is_incoming,
                            call_list[i].is_intercom,
                            time(0) - call_list[i].call_start_time,
                            call_list[i].talking_to_name,
                            signal->status_to_text(call_list[i].status),
                            call_list[i].talking_to_number,
                            call_list[i].did,
                            call_list[i].call_missed,
                            call_list[i].uniqueid);
                }
                if (i > 0) strcat(cl_string, ",");
                strcat(cl_string, call);
            }
            strcat(cl_string, "]");
        }
    } else {
        cl_string = (char *)malloc(3);
        if (cl_string) {
            cl_string[0] = '[';
            cl_string[1] = ']';
            cl_string[2] = '\0';
            *calllist = cl_string;
            return 2;
        }
    }
    if (cl_string != 0) {
        *calllist = cl_string;
        return strlen(cl_string);
    }
    
    return 0;
}

char *make_status_string(DEEDLE_COMMAND *config) {
    static char *connect_string = 0;
    static size_t connect_string_size = 0;
    char *cl_string = 0;
    size_t size_needed = 0, cl_length;
   
    // Base size: 400
    if (config->signal) {
        cl_length = make_call_list(config->signal, &cl_string);
        size_needed = 1200 + strlen(config->iplist) + cl_length;
        if (config->rtp) {
            size_needed+=strlen(config->rtp->json_audio_device_list());
        } else {
            size_needed+=10;
        }
    } else {
        cl_length = 2;
        cl_string = (char *)calloc(3, 1);
        cl_string[0] = '[';
        cl_string[1] = ']';
        cl_string[2] = 0;
        size_needed = 1204 + cl_length;
    }
    
    
    if (size_needed > connect_string_size) {
        if (connect_string) {
            free(connect_string);
            connect_string = 0;
        }
        connect_string = (char *)calloc(size_needed+100, 1);
        if (!connect_string) {
            if (cl_string) {
                free(cl_string);
            }
            return (char*)"";
        }
        connect_string_size = size_needed+100;
    }
    
    
    sprintf(connect_string, "deedlecore:\
type=logon&\
extension=%d&\
sip_server=%s&\
sip_registered=%d&\
error_status=%s&\
version=%s&\
iplist=[]&\
call_list=%s&\
input_device=%d&\
output_device=%d&\
ring_device=%d&\
dnd=%d&\
device_list=%s&\
ringing=%d&\
ringer_on=%d&\
ringer_volume=%d&\
caller_id=%s&\
vm_waiting=no&\
vm_old=0&\
vm_new=0&\
vm_old_urgent=0&\
vm_new_urgent=0&\
gvm_waiting=no&\
gvm_old=0&\
gvm_new=0&\
gvm_old_urgent=0&\
gvm_new_urgent=0",
            config->extension,
            config->configuration.signal_server_list[config->active_SIP_server], // here
            (( config->signal )?config->signal->isregistered():0),
            ((config->signal)?config->signal->string_error_condition:""),
            config->build_version,
            cl_string,
            ((config->rtp)?config->rtp->active_input_device():-1),
            ((config->rtp)?config->rtp->active_output_device():-1),
            ((config->rtp)?config->rtp->active_ring_device():-1),
            ((config->signal)?config->signal->do_not_disturb:0),
            ((config->rtp)?config->rtp->json_audio_device_list():"[]"),
            ((config->rtp)?config->rtp->is_ringing():0),
            ((config->rtp)?config->rtp->is_ringer_on():0),
            ((config->rtp)?config->rtp->get_ring_volume():0),
            ((config->signal)?config->signal->user_info.callerid:""));
    if (cl_string) {
        free(cl_string);
    }
    return connect_string;
}

void deedle_hangup_all( DEEDLE_COMMAND *config ) {
    CALLINFO *callinfo = 0, *calllist = 0;
    size_t call_count = 0, i = 0;
    if (!config->signal) return;
    
    call_count = config->signal->get_call_list(&callinfo);
    if (call_count > 0) {
        for (i = 0; i < call_count; i++) {
            calllist = &callinfo[i];
            config->signal->hangup(calllist->callid);
        }
    }

}

void deedle_send_call_list( DEEDLE_COMMAND *config ) {
    CALLINFO *callinfo = 0, *calllist = 0;
    size_t call_count = 0, i = 0;
    char call_change_string[3000];
    if (!config->signal) return;
    call_count = config->signal->get_call_list(&callinfo);
    if (call_count > 0) {
        for (i = 0; i < call_count; i++) {
            calllist = &callinfo[i];
            if (calllist->is_incoming) {
                if (calllist->is_intercom) {
                    sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=1&did=%d&status=%s&caller_name=%s\r\n",
                            calllist->callid,
                            calllist->talking_to_number,
                            config->signal->user_info.extension,
                            config->signal->status_to_text(calllist->status),
                            calllist->talking_to_name
                            );
                } else {
                    sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=0&did=%s&status=%s&caller_name=%s\r\n",
                            calllist->callid,
                            calllist->talking_to_number,
                            calllist->did,
                            config->signal->status_to_text(calllist->status),
                            calllist->talking_to_name
                            );
                }
            } else {
                if (calllist->is_intercom) {
                    sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=1&did=%s&status=%s&caller_name=%s\r\n",
                            calllist->callid,
                            calllist->talking_to_number,
                            ((strlen(calllist->did) > 0)?calllist->did:config->signal->user_info.callerid),
                            config->signal->status_to_text(calllist->status),
                            calllist->talking_to_name
                            );
                } else {
                    sprintf(call_change_string, "deedlecore:type=call_change&callid=%s&talking_to=%s&intercom=0&did=%s&status=%s&caller_name=%s\r\n",
                            calllist->callid,
                            calllist->talking_to_number,
                            ((strlen(calllist->did) > 0)?calllist->did:config->signal->user_info.callerid),
                            config->signal->status_to_text(calllist->status),
                            calllist->talking_to_name
                            );
                }
            }
            send_to_deedlex(config, call_change_string, strlen(call_change_string));
        }
        free(callinfo);
    } else {
        printf("* No calls on which we can report status.\n\n");
    }
}

#ifdef __APPLE__
void *deedle_command_thread(void *param) {
#else
DWORD deedle_command_thread(void *param) {
#endif
    DEEDLE_COMMAND *config = (DEEDLE_COMMAND *)param;
    char *status_string = 0;
    fd_set fdsr;
    int readcount = 0, bytes = 0;
    struct timeval tmo = { 0, 20000 };
    char buffer[500];
    time_t last_contact_time = 0, call_list_timer = 0;
    int ka_sent = 0;
    time_t extension_get_timer = 0;
    
    printf("Entering deedle_command_thread.\n");

    config->socket = deedle_command_connect(config->configuration.deedlex_ip, config->configuration.deedlex_port);
    
    
    if (config->socket) {
        char advice[500];
        sprintf(advice, "deedlecore:type=advise&extension=%d\r\n", config->extension);
        send_to_deedlex(config, advice, strlen(advice));
        
        
        status_string = make_status_string(config);
        send_to_deedlex(config, status_string, strlen(status_string));
        last_contact_time = time(0);
    } else {
        config->socket_error_condition = 1;
    }
    
    if (config->rtp) {
        config->rtp->set_ringer_off();
    }
    
    
    // loop
    call_list_timer = time(0) + 7;
    
    
    
    
    
    
    
    while (config->deedle_thread_running == 1) {
        
        if (config->signal) {
            
            if (config->advice_needed == 1) {
                char advice[500];
                sprintf(advice, "deedlecore:type=advise&extension=%d\r\n", config->extension);
                send_to_deedlex(config, advice, strlen(advice));
                status_string = make_status_string(config);
                send_to_deedlex(config, status_string, strlen(status_string));
                config->advice_needed = 0;
            }
            
            if (vm_request.collect_me == 1) {
                char vm_message[3000];
                vm_request.collect_me = 0;
                sprintf(vm_message, "deedlecore:type=voicemail&vm_waiting=%d&vm_new=%d&vm_new_urgent=%d&vm_old=%d&vm_old_urgent=%d",
                        vm_request.vm_waiting,
                        vm_request.vm_new,
                        vm_request.vm_new_urgent,
                        vm_request.vm_old,
                        vm_request.vm_old_urgent);
                send_to_deedlex(config, vm_message, strlen(vm_message));
                check_group_voicemail(config->signal, (char *)"deedleserver2.com", 300, NULL, (char *)"Oicu812!");
            }
            
            if (gvm_request.collect_me == 1) {
                char vm_message[3000];
                sprintf(vm_message, "deedlecore:type=groupvoicemail&vm_waiting=%d&vm_new=%d&vm_new_urgent=%d&vm_old=%d&vm_old_urgent=%d",
                        gvm_request.vm_waiting,
                        gvm_request.vm_new,
                        gvm_request.vm_new_urgent,
                        gvm_request.vm_old,
                        gvm_request.vm_old_urgent);
                send_to_deedlex(config, vm_message, strlen(vm_message));
                gvm_request.collect_me = 0;
            }
        } else {
            char get_my_info[500];
            if ( config->shutdown_deedlecore_flag == 0) { // If we're NOT shutting down...
                if (time(0) - extension_get_timer > 5) {
                    if (config->extension == 0) {
                        sprintf(get_my_info, "deedlecore:type=getextension&code=%s\r\n", config->product_code);
                        send_to_deedlex(config, get_my_info, strlen(get_my_info));
                    }
                    extension_get_timer = time(0) + 5;
                }
            }
        }
        // Every 7 seconds send a call list if there ARE calls...
        if (time(0) > call_list_timer) {
            char *timed_call_list = 0, *tcl_bfr_to_send = 0;
            size_t tcl_size = 0;
            if (config->signal) {
                tcl_size = make_call_list(config->signal, &timed_call_list);
            }
            if (tcl_size > 0) {
                if (!JSTRING::matches(timed_call_list, (char *)"[]")) {
                    tcl_bfr_to_send = (char *)calloc( tcl_size + 40 , 1);
                    if (tcl_bfr_to_send) {
                        sprintf(tcl_bfr_to_send, "deedlecore:type=call_list&call_list=%s\r\n", timed_call_list);
                        send_to_deedlex(config, tcl_bfr_to_send, strlen(tcl_bfr_to_send));
                        free(tcl_bfr_to_send);
                    }
                }
                
            }
            if (timed_call_list) {
                free(timed_call_list);
            }
            call_list_timer = time(0) + 7;
        }
        
        
        /*
         
         Ideas:
         Send call list every N seconds when calls are active.
         CyberOnline can infer that there are no calls if it
         has gone N seconds without getting a call list update...
         
         
         
         
         
         */
        
        
        if (!config->socket_error_condition && config->socket) {
            FD_ZERO(&fdsr);
            FD_SET(config->socket, &fdsr);
            tmo.tv_sec = 0;
            tmo.tv_usec = 20000;
            readcount = select(((int)config->socket + 1), &fdsr, 0, 0, &tmo);
            if (readcount > 0) {
                memset(buffer, 0, 500);
                bytes = (int)recv(config->socket, buffer, 500, 0);
                if (bytes > 0) {
                    struct JSTRING::jsstring **cmdlist = 0;
                    int cmd_itr = 0;
                    char *command = 0;
                    size_t command_length = 0;
                    last_contact_time = time(0);
                    ka_sent = 0;
//                    printf("RX: %.*s\n\n", bytes, buffer);
                    
                    
                    if (!strncmp(buffer, "\r\n\r\n", 4)) {
                        send(config->socket, "\r\n", 2, 0);
                    } else {
                        cmdlist = JSTRING::split(buffer, '\n');
                        if (cmdlist) {
                            while (cmdlist[cmd_itr] != 0) {
                                command = JSTRING::trim(cmdlist[cmd_itr]->ptr, cmdlist[cmd_itr]->length, &command_length);
//                                printf("\tProcessing: %.*s\n", (int)cmdlist[cmd_itr]->length, cmdlist[cmd_itr]->ptr);
                                
                                if (!strncmp(command, "status", 6)) {
                                    if (config->signal) {
                                        config->signal->do_not_disturb = 0;
                                    }
                                    status_string = make_status_string(config);
                                    send_to_deedlex(config, status_string, strlen(status_string));
                                    deedle_send_call_list(config);
                                } else if (!strncmp(command, "version", 7)) {
                                    char version_req[400];
                                    sprintf(version_req, "%s\r\n", config->build_version);
                                    send_to_deedlex(config, version_req, strlen(version_req));
                                } else if (!strncmp(command, "!quit!", 6)) {
                                    config->shutdown_deedlecore_flag = 1;
                                } else if (!strncmp(command, "h:all", 5)) {
                                    printf("Hangup All\n");
                                    deedle_hangup_all(config);
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "ro", 2)) {
                                    printf("GOT RINGER ON COMMAND!\n\n");
                                    if (config->rtp) config->rtp->set_ringer_on();
                                    if (config->signal) {
                                        config->signal->do_not_disturb = 0;
                                    }

                                } else if (!strncmp(command, "rf", 2)) {
                                    printf("GOT RINGER OFF COMMAND!\n\n");
                                    if (config->rtp) config->rtp->set_ringer_off();
                                    
                                } else if (!strncmp(command, "checkvm", 7)) {
                                    check_group_voicemail(config->signal, (char *)"deedleserver2.com", 300, 0, (char *)"Oicu812!");
                                } else if (!strncmp(command, "dnd:", 4)) { // Set Do-Not-Disturb
                                    char *szdnd = 0, *szdndend=0;
                                    size_t lendnd = 0;
                                    szdnd = JSTRING::headervalue(command, '\n', &lendnd);
                                    szdnd = JSTRING::trim(szdnd, &lendnd, &lendnd);
                                    if (lendnd > 0 && config->signal) {
                                        szdndend = &szdnd[lendnd];
                                        switch (strtoul(szdnd, &szdndend, 10)) {
                                            case 0:
                                                config->signal->do_not_disturb = 0;
                                                break;
                                            case 1:
                                                config->signal->do_not_disturb = 1;
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "ext:", 4)) {// Get notified about what our extension is
                                    char ext[500] = {0};
                                    char *szext = 0;
                                    size_t lenextension = 0;
                                    int extension = 0;
                                    szext = JSTRING::headervalue(command, '\n', &lenextension);
                                    szext = JSTRING::trim(szext, &lenextension, &lenextension);
                                    if (lenextension > 0) {
                                        if (lenextension < 500) {
                                            sprintf(ext, "%.*s", (int)lenextension, szext);
                                            if (!strncmp(ext, "lock", 4)) {
                                                return 0;
                                            }
                                        }
                                    }
                                    if (strlen(ext) > 0) {
                                        szext = &ext[lenextension];
                                        extension = (int)strtoul(ext, &szext, 10);
//                                        printf("Our extension is: %d\n", extension);
                                        if (extension > 0 && extension < 0xffffffff) {
                                            if (config->extension == 0) {
                                                config->extension = extension;
                                            }
                                        }
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);

                                    
                                } else if (!strncmp(command, "digit:", 6)) {
                                    // Dial a digit
                                    char callid[500] = {0};
                                    char *szcallid = 0;
                                    size_t lencallid = 0;
                                    szcallid = JSTRING::headervalue(command, '\n', &lencallid);
                                    szcallid = JSTRING::trim(szcallid, &lencallid, &lencallid);
                                    if (lencallid > 0) {
                                        if (lencallid < 500) {
                                            sprintf(callid, "%.*s", (int)lencallid, szcallid);
                                        }
                                    }
                                    if (strlen(callid) > 0) {
                                        if (config->signal) config->signal->digit(callid[0]);
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "hold:", 5)) {
                                    char callid[500] = {0};
                                    char *szcallid = 0;
                                    size_t lencallid = 0;
                                    szcallid = JSTRING::headervalue(command, '\n', &lencallid);
                                    szcallid = JSTRING::trim(szcallid, &lencallid, &lencallid);

                                    if (lencallid > 0) {
                                        if (lencallid < 500) {
                                            sprintf(callid, "%.*s", (int)lencallid, szcallid);
                                        }
                                    }
                                    if (strlen(callid) > 0) {
                                        if (config->signal) {
                                            printf("Putting callid \"%s\" on hold.\n", callid);
                                            config->signal->hold(callid);
                                        } else {
                                            printf("Failed to put callid \"%s\" on hold.\n", callid);
                                        }
                                    } else {
                                        printf("Failed to put callid \"%s\" on hold.\n", callid);
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                    
                                } else if (!strncmp(command, "unhold:", 7)) {
                                    char callid[500] = {0};
                                    char *szcallid = 0;
                                    size_t lencallid = 0;
                                    szcallid = JSTRING::headervalue(command, '\n', &lencallid);
                                    szcallid = JSTRING::trim(szcallid, &lencallid, &lencallid);

                                    if (lencallid > 0) {
                                        if (lencallid < 500) {
                                            sprintf(callid, "%.*s", (int)lencallid, szcallid);
                                        }
                                    }
                                    if (strlen(callid) > 0) {
                                        if (config->signal) config->signal->unhold(callid);
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                    
                                } else if (!strncmp(command, "t:", 2)) { // t:<call-id>:xfer-destination
                                    int xfer_itr = 1;
                                    struct JSTRING::jsstring **cmd_split = 0;
                                    char xfer_callid[500] = {0};
                                    char xfer_destination[500] = {0};
                                    cmd_split = JSTRING::split(command, ':');
                                    if (cmd_split) {
                                        while (cmd_split[xfer_itr] != 0) {
                                            if (cmd_split[xfer_itr]->length > 0 && cmd_split[xfer_itr]->length < 500) {
                                                switch (xfer_itr) {
                                                    case 1:
                                                        sprintf(xfer_callid, "%.*s", (int)cmd_split[xfer_itr]->length, cmd_split[xfer_itr]->ptr);
                                                        break;
                                                    case 2:
                                                        sprintf(xfer_destination, "%.*s", (int)cmd_split[xfer_itr]->length, cmd_split[xfer_itr]->ptr);
                                                        break;
                                                    default:
                                                        break;
                                                }
                                            }
                                            xfer_itr++;
                                        }
                                        JSTRING::freesplit(cmd_split);
                                    }
                                    if (strlen(xfer_callid) > 0 && strlen(xfer_destination) > 0) {
                                        if (config->signal) config->signal->transfer(xfer_callid, xfer_destination);
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "a:", 2)) {
                                    char *szcallid = 0;
                                    size_t lencallid = 0;
                                    szcallid = JSTRING::headervalue(command, '\n', &lencallid);
                                    szcallid = JSTRING::trim(szcallid, &lencallid, &lencallid);

                                    if (lencallid > 0) {
                                        char callid[300];
                                        if (lencallid < 300) {
                                            sprintf(callid, "%.*s", (int)lencallid, szcallid);
                                            printf("*** deedle_command.cpp > deedle_command_thread(): Answering Call ID: %s\n", callid);
                                            if (config->signal) config->signal->answer(callid);
                                        }
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "device_refresh", 14)) {
                                    char notify_device_change[1000];
                                    char *dev_list = 0;
                                    
                                    if (config->rtp) {
                                        dev_list = config->rtp->json_audio_device_list();
                                        size_t len_dev_length = strlen(config->rtp->json_audio_device_list());
                                        sprintf(notify_device_change, "deedlecore:type=device_change&input_device=%d&output_device=%d&ring_device=%d&device_list=%.*s\r\n",
                                                config->rtp->get_active_input_device(),
                                                config->rtp->get_active_output_device(),
                                                config->rtp->get_active_ring_device(),
                                                ((len_dev_length < 700)?(int)len_dev_length:700),
                                                dev_list);
                                        send_to_deedlex(config, notify_device_change, strlen(notify_device_change));
                                    } else {
                                        send_to_deedlex(config, (char *)"Phone is not connected.\r\n", strlen("Phone is not connected.\r\n"));
                                    }
                                    
                                } else if (!strncmp(command, "o:", 2)) {
                                    JSTRING *js = new JSTRING(command);
                                    char *rdline = 0, *dn_start = 0, *dn_end = 0;
                                    size_t rdit = 0, rdlinelen = 0, hv_len;
                                    int device_number = 0;
                                    printf("Device Change Request: %s\n", command);
                                    
                                    for (rdit = 0; rdit <= js->linecount; rdit++) {
                                        rdline = js->line((int)rdit, &rdlinelen);
                                        if (rdlinelen > 0) {
                                            rdline = JSTRING::trim(rdline, &rdlinelen, &rdlinelen);
                                            if (rdlinelen > 0) {
                                                
                                                dn_start = JSTRING::headervalue(rdline, '\n', &hv_len);
                                                if (hv_len > 0) {
                                                    dn_start = JSTRING::trim(dn_start, &hv_len, &hv_len);
                                                    if (hv_len > 0) {
                                                        dn_end = &dn_start[hv_len];
                                                        device_number = (int)strtol(dn_start, &dn_end, 10);
                                                    }
                                                }
                                                switch (rdline[0]) {
                                                    case 'o':
                                                        printf("*** Setting OUTPUT device to: %d\n", device_number);
                                                        if (config->rtp) {
                                                            config->rtp->set_output_device(device_number);
                                                        }
                                                        break;
                                                    case 'i':
                                                        printf("*** Setting INPUT device to: %d\n", device_number);
                                                        if (config->rtp)
                                                            config->rtp->set_input_device(device_number);
                                                        break;
                                                    case 'r':
                                                        printf("*** Setting RING device to: %d\n", device_number);
                                                        if (config->rtp)
                                                            config->rtp->set_ring_device(device_number);
                                                        break;
                                                    default:
                                                        break;
                                                }
                                            }
                                        }
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "h:", 2)) {
                                    DIALOG *d = 0;
                                    char *szcallid = 0;
                                    size_t lencallid = 0;
                                    szcallid = JSTRING::headervalue(command, '\n', &lencallid);
                                    szcallid = JSTRING::trim(szcallid, &lencallid, &lencallid);

                                    if (lencallid > 0 && config->signal) {
                                        char callid[300];
                                        if (lencallid < 300) {
                                            sprintf(callid, "%.*s", (int)lencallid, szcallid);
                                            printf("*** deedle_command.cpp > deedle_command_thread(): Hanging up Call ID: %s\n", callid);
                                            d = config->signal->get_dialog(callid);
                                            if (d) {
                                                if (!strncmp(d->callinfo.talking_to_number, "*98", 3)) {
                                                    /* Right here, we need to check to see if this was a call to *98.  If it WAS, we need
                                                     to check group voice mail again! */
                                                    check_group_voicemail(config->signal, (char *)"deedleserver2.com", 300, 0, (char *)"Oicu812!");
                                                }
                                            }
                                            config->signal->hangup(callid);
                                        }
                                    }
                                    
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else if (!strncmp(command, "srv:", 4)) {
                                    char *szvolume = 0, *endvolume = 0;
                                    size_t lenvolume = 0;
                                    int new_volume = 0;
                                    szvolume = JSTRING::headervalue(command, '\n', &lenvolume);
                                    szvolume = JSTRING::trim(szvolume, &lenvolume, &lenvolume);

                                    if (lenvolume > 0 && config->rtp) {
                                        JSTRING::trim(szvolume, &lenvolume, &lenvolume);
                                        if (lenvolume) {
                                            endvolume = &szvolume[lenvolume];
                                            new_volume = (int)strtoul(szvolume, &endvolume, 10);
                                            if (new_volume >= 0 && new_volume <= 100) {
                                                config->rtp->set_ring_volume(new_volume);
                                            }
                                        }
                                    }
                                    send_to_deedlex(config, (char *)"ok\r\n", 4);
                                } else { // It's inbound data we haven't handled yet
                                    //                            printf("DEEDLEX says: \"%.*s\"\n\n", bytes, command);
                                    if (command[0] == 'c' && command[1] == ':') {
                                        if (bytes-2 > 0) {
                                            char *numbertocall = 0;
                                            char number_to_call[200];
                                            size_t ntd_length = 0;
                                            numbertocall = JSTRING::trim(&command[2], &ntd_length);
                                            if (ntd_length > 0 && config->signal) {
                                                sprintf(number_to_call, "%.*s", ((ntd_length > 99)?99:(int)ntd_length), numbertocall);
                                                printf("Calling: \"%s\" ( %d bytes)\n", number_to_call, (int)ntd_length);
                                                config->signal->call(number_to_call);
                                            }
                                        }
                                    }
                                }
                                cmd_itr++;
                            }
                            JSTRING::freesplit(cmdlist);
                        }
                    }
                    
                    
                    
//                    if (!strncmp(buffer, "\r\n\r\n", 4)) {
//                        send(config->socket, "\r\n", 2, 0);
//                    } else {
//                    }
                    
                    // Done parsing commands here!!
                        
                    
                    
                    
                    
                    
                    
                    
                    //send(config->socket, quitmsg, strlen(quitmsg), 0);
                } else if (bytes == -1) {
                    printf("Need to check for error conditions!\n");
                    dc_socket_failure(config);
                }
            }
        }
        if (config->socket_error_condition > 0) {
            Sleep(20);
            if (config->rtp) {
                if (config->rtp->is_ringer_on()) {
                    config->rtp->set_ringer_off();
                }
            }
            if (time(0) > config->socket_retry) {
                printf("Attempting to reconnect to DEEDLEX\n");
                SOCKET temp_socket = 0;
                if (config->socket) {
                    closesocket(config->socket);
                    config->socket = 0;
                }
                temp_socket = deedle_command_connect(config->configuration.deedlex_ip, config->configuration.deedlex_port);
                if (temp_socket > 0) {
                    char advice[500];
                    config->socket = temp_socket;
                    config->socket_error_condition = 0;
                    config->socket_retry = 0;
                    sprintf(advice, "deedlecore:type=advise&extension=%d\r\n", config->extension);
                    send_to_deedlex(config, advice, strlen(advice));
                    status_string = make_status_string(config);
                    last_contact_time = time(0);
                    send_to_deedlex(config, status_string, strlen(status_string));
                    
                    
                    
                } else {
                    config->socket_retry = time(0) + 3;
                }
            }
        } else {
            if (time(0) - last_contact_time > 20) {
                printf("DEEDLEX communication broken.  Attempting reconnection.\n");
                config->socket_error_condition = 1;
                config->socket_retry = 0;
            } else if (time(0) - last_contact_time > 17 && !ka_sent) {
                bytes = (int)send(config->socket, "deedlecore:type=ka\r\n", 20, 0);
                if (bytes <= 0) {
                    config->socket_error_condition = 1;
                    config->socket_retry = 0;
                } else {
//                    printf("Keep-alive sent\n");
                    ka_sent = 1;
                }
            }

        }
    }
    if (config->socket) {
        send(config->socket, "deedlecore:action=close\r\n", 25, 0);
    }
    if (gvm_request.retrieving_group_voicemail) {
        time_t quit_waiting = time(0) + 3;
//        printf("deedle_command_thread() waiting for GROUP VOICEMAIL thread to end...\n");
        while (time(0) < quit_waiting) {
            if (!gvm_request.retrieving_group_voicemail) {
//                printf("\ncheck_group_voicemail_thread() exited.\n");
                break;
            }
            sleep(10);
        }
        
    }
//    printf("Shutting down deedle_command_thread()\n");
    closesocket(config->socket);
    return 0;
}

    void group_vm_notify(int extension, int voicemail_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent) {
        gvm_request.vm_waiting = voicemail_waiting;
        gvm_request.vm_new = vm_new;
        gvm_request.vm_new_urgent = vm_new_urgent;
        gvm_request.vm_old = vm_old;
        gvm_request.vm_old_urgent = vm_old_urgent;
        gvm_request.done_getting_vm = 1;
        gvm_request.collect_me = 1;
    }

    void *check_group_voicemail_thread( void *param) {
        SIGNAL_SETTINGS gvm_ss;
        static SIGNAL *gvm_signal = 0;
        time_t timeout = 0;
        memset(&gvm_ss, 0, sizeof(SIGNAL_SETTINGS));
        if (strlen(gvm_request.voicemail_server) == 0) {
            printf("* No voicemail server HOSTNAME or IP Address specified.\n\n");
            gvm_request.retrieving_group_voicemail = 0;
            return 0;
        }
        if ( gvm_request.extension <= 0) {
            printf("* Voicemail extension must be higher than ZERO.\n\n");
            gvm_request.retrieving_group_voicemail = 0;
            return 0;
        }
        if ( strlen(gvm_request.password) == 0 ) {
            printf("* Username and password required for voicemail server authentication.\n\n");
            gvm_request.retrieving_group_voicemail = 0;
            return 0;
        }
        if (!gvm_request.main_signal) {
            printf("* We are not REGISTERed on any SIP servers and cannot check Group Voicemail!\n\n");
            gvm_request.retrieving_group_voicemail = 0;
            return 0;
        }
        
        gvm_ss.do_not_disturb = 1;
        gvm_ss.rtp = 0;
        gvm_ss.auth.username[0] = 0;
        sprintf(gvm_ss.auth.password, "%s", gvm_request.password);
        sprintf(gvm_ss.auth.username, "%s", gvm_request.username); // Not needed for deedle
        sprintf(gvm_ss.SIP_server_IP, "%s", gvm_request.voicemail_server);
        gvm_ss.SIP_server_port = 5060;
        gvm_ss.extension = gvm_request.extension;
        gvm_ss.callbacks.call_status_change_callback = 0;
        gvm_ss.callbacks.registered_callback = 0;
        gvm_ss.callbacks.ringing_status_callback = 0;
        gvm_ss.callbacks.voicemail_callback = &group_vm_notify;
        
        gvm_signal = new SIGNAL(&gvm_ss);
//        printf("- GVM SIGNAL STARTED\n");
        if (gvm_signal) {
            timeout = time(0) + 5;
            
            while (time(0) < timeout) {
//                printf("check_group_voicemail_thread(): gvm_request.done_getting_vm = %d\n\n", gvm_request.done_getting_vm);
                // We need to BREAK OUT OF THIS, if we've received our voicemail!
                if (gvm_request.done_getting_vm == 1) {
//                    printf("-- Done getting GROUP voicemail!\n\n");
                    break;
                }
                Sleep(1000);
            }
            delete gvm_signal;
//            printf("Unregistering and Leaving check_group_voicemail_thread()\n");
        } else {
            printf("* check_group_voicemail(): Failed to instanciate a SIGNAL class for GROUP VOICEMAIL.\n");
        }
        gvm_request.retrieving_group_voicemail = 0;
        return 0;
    }

    void check_group_voicemail( SIGNAL *signal, char *voicemail_server, int extension, char *username, char *password ) {
        DB_THREAD check_gvm_thread = 0;
        int dbct = 0;
        if (gvm_request.retrieving_group_voicemail == 1) {
            return;
        }
        gvm_request.done_getting_vm = 0;
        if (!voicemail_server) return;
        if (!username) {
            gvm_request.username[0] = 0;
        } else {
            sprintf(gvm_request.username, "%s", username);
        }
        if (!password) return;
        if (extension <= 0) return;
        gvm_request.main_signal = signal;
        gvm_request.extension = extension;
        sprintf(gvm_request.voicemail_server, "%s", voicemail_server);
        sprintf(gvm_request.password, "%s", password);
        dbct = db_create_thread(&check_group_voicemail_thread, 0, &check_gvm_thread);
        if (check_gvm_thread) {
            gvm_request.retrieving_group_voicemail = 1;
        }
        return;
    }



    void add_signal_server( char *signal_server, DEEDLE_COMMAND *config) {
        if (config->configuration.signal_server_count + 1 == 50) return;
        sprintf(config->configuration.signal_server_list[config->configuration.signal_server_count], "%s", signal_server);
        config->configuration.signal_server_count++;
    }

    char *get_signal_server( DEEDLE_COMMAND *config ) {
        return config->configuration.signal_server_list[config->active_SIP_server];
    }
    
    char *get_next_server( DEEDLE_COMMAND *config ) {
        char *s = 0;
        if ((config->active_SIP_server + 1) < 50) {
            s = config->configuration.signal_server_list[config->active_SIP_server];
            if (strlen(s) > 0) {
                config->active_SIP_server++;
            }
        } else {
            s = config->configuration.signal_server_list[0];
            if (strlen(s) > 0) {
                config->active_SIP_server = 0;
            }
        }
        return config->configuration.signal_server_list[config->active_SIP_server];
    }
