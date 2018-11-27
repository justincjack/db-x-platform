// signal.h (c)2018 Justin Jack
// deedlecore project - libsignal.a / libsignal.lib
/*
This file handles the underlying connection to the signalling server.

It will implement TCP transport for reliability.

It will expose functions for sending and receiving signalling traffic.

It is responsible for assuring that the connection is alive and stable.  It
will monitor the response time for SIP OPTIONS, and monitor TCP keep-alives.

It will also handle registrations to the signalling server so we can get that task off
of the application.

 v1.2
 
*/
#include "jstring.h"

#include "signal.h"
#ifdef __APPLE__
#include <pthread.h>
#endif
//#include "memcheck.h"

const char *SIGNAL::sip_request_list[] = { "INVITE",  "ACK", "CANCEL", "OPTIONS", "BYE", "REFER", "SUBSCRIBE", "NOTIFY", "INFO", "PUBLISH", "MESSAGE", "REGISTER" };
const char *SIP::sip_request_list[] = { "INVITE",  "ACK", "CANCEL", "OPTIONS", "BYE", "REFER", "SUBSCRIBE", "NOTIFY", "INFO", "PUBLISH", "MESSAGE", "REGISTER" };
const char *SIGNAL::empty = "";

const char SIGNAL::OPTIONS[] = "\
OPTIONS sip:Registrar@%s;transport=tcp SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s;rport\r\n\
Max-Forwards: 70\r\n\
From: <sip:%d@%s;transport=tcp>;tag=%s\r\n\
To: \"Registrar\" <sip:Registrar@%s>\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: 102 OPTIONS\r\n\
User-Agent: deedlebox/%s\r\n\
Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n\
Accept: application/sdp, application/simple-message-summary, message/sipfrag;version=2.0, text/plain\r\n\
Supported: replaces, timer\r\n\
Allow-Events: presence, message-summary, refer\r\n\
Content-Length: 0\r\n\r\n";


/*
const char SIGNAL::REGISTER[]...

Old Contact line: Contact: <sip:%d@%s;transport=tcp;ob>;expires=%d\r\n\
*/

const char SIGNAL::REGISTER[] = "\
REGISTER sip:%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;rport;branch=%s\r\n\
Max-Forwards: 70\r\n\
From: \"%s\" <sip:%d@%s>;tag=%s\r\n\
To: <sip:%d@%s>\r\n\
Call-ID: %s\r\n\
CSeq: %d REGISTER\r\n\
User-Agent: deedlebox/%s\r\n\
Contact: <sip:%d@%s;transport=tcp>;expires=%d\r\n\
Expires: %d\r\n\
Allow: PRACK,INVITE,ACK,BYE,CANCEL,UPDATE,SUBSCRIBE,NOTIFY,REFER,MESSAGE,OPTIONS\r\n\
Content-Length: 0\r\n\r\n";


/*
const char SIGNAL::AUTHREGISTER[]

Contact: <sip:%d@%s;transport=tcp;ob>;expires=%d\r\n\
*/

const char SIGNAL::AUTHREGISTER[] = "\
REGISTER sip:%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;rport;branch=%s\r\n\
Max-Forwards: 70\r\n\
From: \"%s\" <sip:%d@%s>;tag=%s\r\n\
To: <sip:%d@%s>\r\n\
Call-ID: %s\r\n\
CSeq: %d REGISTER\r\n\
User-Agent: deedlebox/%s\r\n\
Contact: <sip:%d@%s;transport=tcp>;expires=%d\r\n\
Expires: %d\r\n\
Allow: PRACK,INVITE,ACK,BYE,CANCEL,UPDATE,SUBSCRIBE,NOTIFY,REFER,MESSAGE,OPTIONS\r\n\
Authorization: Digest %s\r\n\
Content-Length: 0\r\n\r\n";

/*
unsigned long long timestamp(void) {
#ifdef _WIN32
	return (unsigned long long)GetTickCount();
#else
	unsigned long long retval = 0;
	struct timeval ts;
	gettimeofday(&ts, NULL);
	retval = (((unsigned long long)ts.tv_sec * (unsigned long long)1000000) + (unsigned long long)ts.tv_usec) / 1000;
	return retval;
#endif
}
*/



/*
This function DID get the IP address of the computer on which it's running, but that caused problems for Macs
with Wine.  It's not all that necessary with TCP, so I did away with it and put in a dummy IP address for
inital presentation...
*/
char *SIGNAL::setlocalipaddress() {
    int i = 0;
	if (this->validateipaddress(this->user_info.ipaddress)) return this->user_info.ipaddress;
    for (i = 0; i < 10; i++) {
        if (this->iplist[i] > 0) {
            if (db_inet_ntop(AF_INET, &this->iplist[i], this->user_info.ipaddress, 50)) {
                break;
            }
        }
    }
    if (i > 9 ) {
        sprintf(this->user_info.ipaddress, "192.168.1.1");
    }
	return this->user_info.ipaddress;
}

int SIGNAL::IPandport_deedle(char *buff) {
	if (!this->validateipaddress(this->user_info.ipaddress)) {
		/*
		OutputDebugString("\n\n***************************************\n");
		OutputDebugString("*  Calling this->setlocalipaddress()  *\n");
		OutputDebugString("***************************************\n\n");
		*/
		this->setlocalipaddress();
	}
	if (this->user_info.rport == 0) this->user_info.rport = 5060;
	sprintf(buff, "%s:%u", this->user_info.ipaddress, this->user_info.rport);
	return 1;
}

int SIGNAL::IPandport_server(char *buff) {
	if (!this->validateipaddress(this->voip_server_ip)) return 0;
	if (this->signal_port == 0) this->signal_port = 5060;
	sprintf(buff, "%s:%u", this->voip_server_ip, this->signal_port);
	return 1;
}


void SIGNAL::newbranch(char *buff) {
	static int ctr = 0;
	sprintf(buff, "z9hG4bKdb%x%x%x%x%x",
		rand(),
		rand(),
		rand(),
		rand(), ctr++);
}

void SIGNAL::newcallid(char *buff) {
	static int ctr = 0;
	sprintf(buff, "%x%x%x%x%x%x%x",
		rand(),
		rand(),
		rand(),
		rand(),
		rand(),
		rand(), ctr++);
}

void SIGNAL::newtag(char *buff) {
	this->newcallid(buff);
}




void SIGNAL::options(void) {
	char opts[3000];
	char my_address[50], server_address[50];
	char newtag[255], newbranch[255], callid[255];
	this->newcallid(callid);
	this->newtag(newtag);
	this->newbranch(newbranch);

	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_address)) return;
	sprintf(opts, SIGNAL::OPTIONS,
		server_address,
		my_address,
		newbranch,
		this->user_info.extension,
		my_address,
		newtag,
		server_address,
		this->user_info.extension,
		my_address,
		callid, DEEDLEBOX_VERSION);
	showsip(opts);
	db_send(this->psocket->s, opts, strlen(opts), 0);
	return;
}

int SIGNAL::setlocalip(char *ip) {
	if (this->validateipaddress(ip)) {
		memset(this->user_info.ipaddress, 0, 50);
		strcpy(this->user_info.ipaddress, ip);
		return 1;
	}
	return 0;
}

int SIGNAL::setlocalip(char *ip, size_t len) {
	if (this->validateipaddress(ip)) {
		memset(this->user_info.ipaddress, 0, 50);
		memcpy(this->user_info.ipaddress, ip, len);
		return 1;
	}
	return 0;
}

int SIGNAL::isregistered(void) {
    if (!this->registration) return 0;
	if (time(0) < this->current_registration_expires) return 1;
	return 0;
}

void SIGNAL::ready(void) {
	this->lock();
	this->application_ready = 1;
	this->unlock();
}

void SIGNAL::unready(void) {
	this->lock();
	this->application_ready = 0;
	this->unlock();
}

void SIGNAL::set_rtp(RTPENGINE *rtp_to_use) {
	this->rtp = rtp_to_use;
}

void SIGNAL::initialize() {
	int ip4count = 0, json_ip_size = 0, i = 0, j = 0;
	char temp_ip_buffer[100];

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    
    this->shut_down = 0;
    this->calls_talking = 0;
	/* Default, set do not disturb off */

	this->do_not_disturb = 0;

    /*
     Set our initial call counter to zero.  This is only used
     when we're shutting down to make sure all calls are disposed
     of before exiting...
     */
    this->total_call_count = 0;
    
	/* Set up dialog stuff */
	db_create_mutex(&this->dialog_mutex);
	memset(this->dialog_list, 0, sizeof(DIALOG) * MAX_DIALOG_COUNT);
	this->call_status_change_callback = 0;
	this->ringing_status_callback = 0;
	this->voicemail_callback = 0;
	this->registered_callback = 0;
	/* Get local IP Address list.  Mainly for exposing to the code controlling the SIGNAL class. */
	this->json_ip_list = 0;
	ip4count = get_ip4_list(this->iplist);
	if (ip4count > 0) {
		json_ip_size = ip4count * 100;
		//this->json_ip_list = (char *)MEMORY::calloc_(json_ip_size, 1, (char *)"signalling.cpp > initialize(): this->json_ip_list");
		this->json_ip_list = (char *)calloc(json_ip_size, 1);
		this->json_ip_list[0] = '[';
		for (i = 0; i < ip4count; i++) {
			if (db_inet_ntop(AF_INET, &this->iplist[i], temp_ip_buffer, 100)) {
				if (j++ > 0) strcat(this->json_ip_list, ",");
				strcat(this->json_ip_list, "\"");
				strcat(this->json_ip_list, temp_ip_buffer);
				strcat(this->json_ip_list, "\"");
			}
		}
		strcat(this->json_ip_list, "]");
	}
	if (!this->json_ip_list) {
		printf("* No IP Addresses found on this local system!\n");
	}

	this->rtp = 0;

	/* Set up variables to keep up with registration failures */
	this->registration_info.auth_sent = 0;
	this->registration_info.failed_attempts = 0;
	this->registration_info.last_registration_attempt = 0;
	/* Initialize other variables */
	this->connected = 0;
	this->application_ready = 0;
	this->cseq_registration = 100;
	this->current_registration_expires = 0;
	this->registration = 0;
	this->shuttingdown = 0;
	this->last_message = 0;
	this->signal_available_message_count = 0;
	this->last_options = 0;
	this->signal_last_options = 0;
	this->db_signal_thread = 0;
	this->signal_thread_running = 1;
	this->signal_last_successfull_send = 0;
	memset(&this->signal_serveraddr, 0, sizeof(this->signal_serveraddr));
	memset(&this->signal_registration, 0, sizeof(this->signal_registration));
	memset(this->_socket, 0, sizeof(struct __socket) * 10);
	this->psocket = 0;
	this->signal_thread_id = 0;
	this->voip_server_ip[0] = 0;
	this->send_queue = 0;
	this->send_queue_count = 0;
	this->message_stack_size = 0;
	this->user_info.rport = this->signal_port;
	this->user_info.ipconfirmed = 0;
	this->user_info.extension = 0;
	memset(this->user_info.password, 0, 100);
	memset(this->user_info.callerid, 0, 100);
	sprintf(this->user_info.callername, "Caller");
	this->user_info.ipaddress[0] = 0;
	this->user_info.name[0] = 0;
	db_create_mutex(&this->signal_mutex);
//    printf("Done with SIGNAL::Initialzize()\n");
	return;
}

int SIGNAL::setvoipserverip(char *ip) {
	if (!ip) return 0;
	if (strlen(ip) > 19) return 0;
	if (this->validateipaddress(ip)) {
		memset(this->voip_server_ip, 0, 20);
		strcpy(this->voip_server_ip, ip);
		return 1;
	}
	return 0;
}


SIGNAL::SIGNAL(int extension, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;
	if (strlen(ipadress) > 19) {
		strncpy(this->voip_server_ip, ipadress, 19);
	} else {
		sprintf(this->voip_server_ip, "%s", ipadress);
	}
	return;
}

SIGNAL::SIGNAL(int extension, char *pwd, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;
	if (strlen(ipadress) > 19) {
		strncpy(this->voip_server_ip, ipadress, 19);
	} else {
		sprintf(this->voip_server_ip, "%s", ipadress);
	}
	if (pwd) {
		strcpy(this->user_info.password, pwd);
	}
	return;
}



SIGNAL::SIGNAL(char *username, int extension, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;
	if (strlen(ipadress) > 19) {
		strncpy(this->voip_server_ip, ipadress, 19);
	} else {
		sprintf(this->voip_server_ip, "%s", ipadress);
	}
	if (strlen(username) > 99) {
		strncpy(this->user_info.name, username, 99);
	} else {
		sprintf(this->user_info.name, "%s", username);
	}
	return;
}


SIGNAL::SIGNAL(char *username, int extension, char *pwd, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;

	if (strlen(ipadress) > 19) {
		strncpy(this->voip_server_ip, ipadress, 19);
	} else {
		sprintf(this->voip_server_ip, "%s", ipadress);
	}
	if (strlen(username) > 99) {
		strncpy(this->user_info.name, username, 99);
	} else {
		sprintf(this->user_info.name, "%s", username);
	}
	if (pwd) {
		strcpy(this->user_info.password, pwd);
	}
	return;
}



SIGNAL::SIGNAL(const char *username, int extension, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;
	if (strlen(username) > 99) {
		strncpy(this->user_info.name, username, 99);
	} else {
		sprintf(this->user_info.name, "%s", username);
	}
	return;
}

SIGNAL::SIGNAL(const char *username, int extension, char *pwd, char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	if (extension > 0) this->user_info.extension = extension;
	if (strlen(username) > 99) {
		strncpy(this->user_info.name, username, 99);
	} else {
		sprintf(this->user_info.name, "%s", username);
	}

	if (pwd) {
		strcpy(this->user_info.password, pwd);
	}
	return;
}


// new constructor signal candidate new signal
SIGNAL::SIGNAL(SIGNAL_SETTINGS *s) {
	int server_ip_okay = 0, extension_okay = 0, password_okay = 0;
	this->initialize();
	this->do_not_disturb = s->do_not_disturb;
	if (s->auth.password[0] != 0) {
		strncpy(this->user_info.password, s->auth.password, 99);
		password_okay = 1;
	}
	if (s->auth.username[0] != 0) {
		strncpy(this->user_info.name, s->auth.username, 99);
	}
	if (s->callbacks.call_status_change_callback) {
		this->call_status_change_callback = s->callbacks.call_status_change_callback;
	}
	if (s->callbacks.registered_callback) {
		this->registered_callback =
			s->callbacks.registered_callback;
	}

	if (s->callbacks.ringing_status_callback) {
		this->ringing_status_callback = s->callbacks.ringing_status_callback;
	}

	if (s->callbacks.voicemail_callback) {
		this->voicemail_callback =
			s->callbacks.voicemail_callback;
	}
	if (s->extension) {
		this->user_info.extension =
			s->extension;
		extension_okay = 1;
	}

	if (s->rtp) {
		this->rtp =
			s->rtp;
	}

	if (s->SIP_server_IP[0] != 0) {
		if (this->validateipaddress(s->SIP_server_IP)) {
			strncpy(this->voip_server_ip, s->SIP_server_IP, 20);
			server_ip_okay = 1;
		} else {
			unsigned long ipaddr = db_get_host_ip_address(s->SIP_server_IP);
			if (ipaddr != 0) {
				if (db_inet_ntop(AF_INET, &ipaddr, this->voip_server_ip, 20)) {
//                    printf("Server's IP Address is: %s\n", this->voip_server_ip);
                    sprintf(s->SIP_server_IP, "%s", this->voip_server_ip);
					server_ip_okay = 1;
				}
			}
		}
	}

	if ( s->SIP_server_port > 0) {
		this->signal_port = s->SIP_server_port;
	} else {
		this->signal_port = 5060;
	}

	if (s->userinfo.callerid[0] != 0) {
		strncpy(this->user_info.callerid, s->userinfo.callerid, 99);
	}

	if (s->userinfo.username[0] != 0) {
		strncpy(this->user_info.callername, s->userinfo.username, 99);
	}

	if (extension_okay && server_ip_okay && password_okay) {
//        printf("Settings OKAY, launching signal_thread()\n\n");
#ifdef __APPLE__
		pthread_create(&this->db_signal_thread, 0, &signal_thread, (void *)this);
#elif _WIN32
		db_create_thread(&signal_thread, this, &this->db_signal_thread);
#endif
	} else {
		if (!extension_okay) {
			printf("* Invalid Telephone Extension: Please make sure you've specified a valid extension in SIGNAL_SETTINGS::extension.\n\n");
		}
		if (!password_okay) {
			printf("* Password not set: Please make sure you've entered a valid password in SIGNAL_SETTINGS::auth::password.\n\n");
		}
		if (!server_ip_okay) {
			printf("* IP Address for \"%s\" could not be found/verified.\n\n", s->SIP_server_IP);
		}
	}

		
	return;
}

SIGNAL::SIGNAL(char *ipadress, unsigned short port) {
	this->initialize();
	if (this->validateipaddress(ipadress)) strcpy(this->voip_server_ip, ipadress);
	if (port > 0) this->signal_port = port;
	return;
}

SIGNAL::SIGNAL(unsigned short port) {
	this->initialize();
	this->signal_port = port;
	return;
};

SIGNAL::SIGNAL() {
	this->initialize();
	this->signal_port = 5060;
	return;
};

SIGNAL::~SIGNAL() {
    /* Destructor */
    time_t timeout = time(0) + 3;
    int i = 0;
    DIALOG *d = 0;
    char calls_to_hang_up[500][MAX_DIALOG_COUNT];
    int icalls_to_hang_up = 0;
    
    this->shut_down = 1;
    /* Set DO NOT DISTURB so we don't get too far with any new calls */
    this->do_not_disturb = 1;
    
    
    
    /* Un register so we don't GET any more calls */
	if (this->isregistered()) {
		this->sipshutdown();
		while (timeout > time(0)) {
			if (!this->isregistered()) {
				break;
			}
		}
	}

    
    /* Hang up or reject all calls we are currently on */
    db_lock_mutex(&this->dialog_mutex);
    for (i = 0; i < MAX_DIALOG_COUNT; i++) {
        d = &this->dialog_list[i];
        if (!d->inuse) continue;
        switch (d->callinfo.status) {
            case RINGING_IN:
            case RINGING_OUT:
            case ANSWERING:
            case TALKING:
            case ONHOLD:
            case TRANSFERRING:
            case CONFIRMING:
                sprintf(calls_to_hang_up[icalls_to_hang_up++], "%s", d->dialog_info.callid);
                break;
            case CANCELLING_CALL:
            case SUBSCRIPTION:
            case UNKNOWN:
            case HUNGUP:
            case HANGING_UP:
            default:
                break;
        }
    }
    
    db_unlock_mutex(&this->dialog_mutex);
    
    for (i = 0; i < icalls_to_hang_up; i++) {
        this->hangup(calls_to_hang_up[i]);
    }

    timeout = time(0) + 2;
    
    while (this->total_call_count > 0) {
        Sleep(10);
        if (time(0) > timeout) {
            printf("* Timeout waiting for all queued calls to be terminated!\n");
            break;
        }
    }
    
    
	if (this->json_ip_list) {
		free(this->json_ip_list);
		//MEMORY::free_(this->json_ip_list);
	}
	this->json_ip_list = 0;
	WSACleanup();
	db_destroy_mutex(&this->dialog_mutex);
	db_destroy_mutex(&this->signal_mutex);
	this->signal_thread_running = 0;
	db_thread_join(&db_signal_thread);
	return;
};

void SIGNAL::reconnect(void) {
	return;
}

int SIGNAL::nextmessage(char *buff, int len) {
	int bytesreturned = 0, i = 1;
	struct signal_message *msg = 0;
	if (!this->connected) return 0;
	this->lock();
	msg = &this->message_stack[0];
	if (this->message_stack_size == 0) {
		this->unlock();
		return 0;
	}
	if (len < msg->messagesize) SIGNAL_FAIL(SIGNAL_BUFF_TOO_SMALL);
	bytesreturned = msg->messagesize;
	memmove(buff, msg->message, msg->messagesize);
	buff[bytesreturned] = 0;
	//MEMORY::free_(msg->message);
	free(msg->message);
	msg->message = 0;
	for (; i < this->message_stack_size; i++) {
		memmove(&this->message_stack[i - 1], &this->message_stack[i], sizeof(struct signal_message));
	}
	this->message_stack_size--;
	this->unlock();
	return bytesreturned;
}

/* Changes in_addr to IPv4 text */
char *SIGNAL::ip2string(struct in_addr *addr) {
	static char setup = 0;
	size_t arraycount = 0, currentindex = 0;
	static struct _ipaddress {
		char address[50];
	} *addresslist;
	char *activepointer = 0;
	if (!setup) {
		/* Allocate 50 spots of 50 characters */
		arraycount = 50;
		//addresslist = (struct _ipaddress *)MEMORY::calloc_(arraycount, sizeof(struct _ipaddress), (char *)"signalling.cpp > SIGNAL::ip2string");
		addresslist = (struct _ipaddress *)calloc(arraycount, sizeof(struct _ipaddress));
		setup = 1;
	} else {
		if (++currentindex == 50) currentindex = 0;
	}
	activepointer = addresslist[currentindex].address;
	memset(activepointer, 0, 50);
	db_inet_ntop(AF_INET, addr, activepointer, 50);
	return activepointer;
}

/* Changes a text-based IP address to an in_addr structure */
unsigned long SIGNAL::string2ip(char *ipaddress) {
	struct in_addr output;
	db_inet_pton(AF_INET, ipaddress, &output);
	return *((unsigned long *)&output);
}

unsigned long SIGNAL::string2ip(const char *ipaddress) {
	return SIGNAL::string2ip((char *)ipaddress);
}


int SIGNAL::nextmessage(char *buff, int len, sockaddr_in* sa) {
	/* Copy server info into *sa here!!! */
	memmove(sa, &this->signal_serveraddr, sizeof(struct sockaddr_in));
	return this->nextmessage(buff, len);
}

void SIGNAL::_close(void) {
	int i = 0;
	for (; i < 10; i++) {
		if (this->_socket[i].s == 0) continue;
		//#ifdef _WIN32
		shutdown(this->_socket[i].s, 2);
		closesocket(this->_socket[i].s);
		//#else
		//		closesocket(this->_socket[i].s);
		//#endif
	}
	this->psocket = 0;
//    OutputDebugString("SIGNAL::close(): socket closed\n");
}

/*
Basically, just checks for three dots in the string, I don't want
to make a big deal of this.
*/

int SIGNAL::validateipaddress(char *ip) {
	int i = 0, pcount = 0;
	unsigned int addrchar = 0;
	JSTRING::jsstring **split = 0;
	char *endptr = 0;

	if (JSTRING::haschar(ip, ':')) {
		/* There is a port */
		split = JSTRING::split(ip, '.', ':');
	} else {
		/* No port */
		split = JSTRING::split(ip, '.', '\0');
	}

	if (!split) return 0;
	pcount = 0;
	while (split[i] != 0) {
		if (split[i]->length > 0) {
			endptr = &split[i]->ptr[split[i]->length];
			addrchar = (unsigned int)strtoull(split[i]->ptr, &endptr, 10);
			if (i == 0) {
				if (addrchar > 0 && addrchar <= 255) {
					pcount++;
				}
			} else {
				if (addrchar <= 255) {
					pcount++;
				}
			}
		}
		i++;
	}

	JSTRING::freesplit(split);
	return ((pcount == 4) ? 1 : 0);
}
int SIGNAL::voipconnect(void) {
	int i = 0;
	struct SIGNAL::__socket *poldsocket = 0;
	SIP *oldregistration = 0;
	char debug[200];
	memset(&this->signal_serveraddr, 0, sizeof(struct sockaddr));

	if (this->registration) {
		OutputDebugString("\n\nClearing out old registration...\n\n");
		oldregistration = this->registration;
		this->registration = 0;
		this->current_registration_expires = 0;
		//MEMORY::deleted(oldregistration);
		delete oldregistration;
	}

//    sprintf(debug, "Ext: %d\tAttempting to connect to: %s:%u\n", this->user_info.extension, this->voip_server_ip, this->signal_port);
//    OutputDebugString(debug);
	//sprintf(debug, "Connected is: %d\n", signal->connected);

	if (!this->validateipaddress(this->voip_server_ip)) {
		OutputDebugString("signalling.cpp > SIGNAL::voipconnect(): Failing with invalid IP address.\n");
		SIGNAL_FAIL(SIGNAL_CONNECT_INVALID_IP);
	}

	if (!this->signal_port) {
		OutputDebugString("signalling.cpp > SIGNAL::voipconnect(): Failing with invalid port.\n");
		SIGNAL_FAIL(SIGNAL_CONNECT_NO_PORT);
	}

	/* Unused */
	//this->signal_serveraddr;
	/**********************************/

	if (this->psocket == 0) {
//        OutputDebugString("signalling.cpp > voipconnect(): Starting new connection on socket 0.\n");
		this->psocket = &this->_socket[0];
	} else {
		poldsocket = this->psocket;
		for (i = 0; i < 10; i++) {
			if (&this->_socket[i] == psocket) {
				if (i == 9) {
//                    OutputDebugString("signalling.cpp > voipconnect(): Starting new connection with socket 0 and scheduling destruction of socket 9.\n");
					this->psocket = &this->_socket[0];
				} else {
//                    sprintf(debug, "signalling.cpp > voipconnect(): Starting new connection with socket %i and scheduling destruction of socket %d.\n", (i + 1), i);
					OutputDebugString(debug);
					this->psocket = &this->_socket[i + 1];
				}
				break;
			}
		}
		// Check if the new psocket is NOT empty, if it is NOT EMPTY, destroy it.
		if (this->psocket->s != 0) {
			OutputDebugString("\tsignalling.cpp > voipconnect(): For some reason, the new socket was not destroyed.  We're going ahead and destroying it to make room for the new connection...\n");
			shutdown(this->psocket->s, 2);
			closesocket(this->psocket->s);
			memset(this->psocket, 0, sizeof(struct SIGNAL::__socket));
		}

		// Set poldsocket for destruction two minutes out
		poldsocket->destroy_at_time = time(0) + 120;
	}



	this->psocket->s = tcpconnect(this->voip_server_ip, this->signal_port, 1);
	if (!this->psocket->s) {
		printf("SIGNAL::voipconnect(): tcpconnect() failed...\n");
        sprintf(this->string_error_condition, "FAILED to connect to %s:%u", this->voip_server_ip, this->signal_port);
		memset(this->psocket, 0, sizeof(struct SIGNAL::__socket));
		this->psocket = 0;
		this->connected = 0;
		SIGNAL_FAIL(-1);
	}
    this->connected = 1;
    this->string_error_condition[0] = 0;

	this->options();
	this->psocket->last_response = time(0); // Give 10 seconds before signal_thread() loop marks this connection as dead.
	return this->connected;
}
void SIGNAL::sipregister(struct SIGNAL::__socket *psocketstruct, int expires) {
	char opts[1000];
	char my_address[50], server_address[50];
	char newtag[255], newbranch[255], callid[255];
	SIP *oldregistration = 0;
	if (!psocketstruct) return;

	if (!this->registration) {
		/* This is a new registration */
		this->newcallid(callid);
		this->newtag(newtag);
	} else {
		oldregistration = this->registration;
		sprintf(callid, "%.*s", (int)this->registration->message.callid_header->value.length,
			this->registration->message.callid_header->value.string);
		sprintf(newtag, "%s", this->registration->message.from_header->sip_headerinfo.uri->tag());
	}
	this->newbranch(newbranch);

	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_address)) return;
	sprintf(opts, SIGNAL::REGISTER,
		server_address, // REGISTER
		my_address, newbranch, // Via
		((strlen(this->user_info.name) > 0) ? this->user_info.name : ""), this->user_info.extension, server_address, newtag, // From:
		this->user_info.extension, server_address, // To:
		callid, // Call-ID
		++this->cseq_registration, // CSeq
		DEEDLEBOX_VERSION, // User-Agent
		this->user_info.extension, my_address, expires, // Contact: (with ";expires=" parameter)
		expires); // Expires:
	this->registration = new SIP(opts, strlen(opts));
	//MEMORY::add_pointer_to_track(this->registration, (char *)"signalling.cpp > sipregister(): Allocated new REGISTER-ation");
	psocketstruct->last_connection_check = time(0);
	showsip(opts);
	db_send(psocketstruct->s, opts, strlen(opts), 0);
	if (oldregistration) {
		//MEMORY::deleted(oldregistration);
		delete oldregistration;
	}
	return;
}


void SIGNAL::sipregister(struct SIGNAL::__socket *psocketstruct) {
	SIGNAL::sipregister(psocketstruct, REGISTRATION_EXPIRATION);
}
void SIGNAL::sipunregister(struct SIGNAL::__socket *psocketstruct) {
	if (this->registration) SIGNAL::sipregister(psocketstruct, 0);
}

void SIGNAL::sipunregister(void) {
	if (this->registration)	SIGNAL::sipregister(this->psocket, 0);
}

void SIGNAL::sipshutdown(void) {
	this->shuttingdown = 1;
	this->sipunregister();
}


void SIGNAL::set_caller_id(const char *callerid) {
	if (!callerid) return;
	sprintf(this->user_info.callerid, "%.*s", ((strlen(callerid) < 100) ? (int)strlen(callerid) : 99), callerid);
}

void SIGNAL::set_caller_name(const char *callername) {
	if (!callername) return;
	sprintf(this->user_info.callername, "%.*s", ((strlen(callername) < 100) ? (int)strlen(callername) : 99), callername);
}


char *SIGNAL::password(void) {
	return this->user_info.password;
}
void SIGNAL::password(char *pwd) {
	memset(this->user_info.password, 0, 100);
	if (pwd) {
		strcpy(this->user_info.password, pwd);
	}
	return;
}


void SIGNAL::auth(DIALOG *dialog, SIP *message) {
	char thispacket[5000];
	char authheader[1000];
	char ha1[33], ha2[33], response[33], first_value[500], second_value[500], third_value[500];
	char my_address[50], server_address[50];
	char auth_extension[100];
	JSTRING *js = 0;
	size_t i = 0, linelength = 0;
	char *line = 0;
	memset(thispacket, 0, 5000);

	if (message->message.from_header->sip_headerinfo.uri->_extension.length > 0) {
		sprintf(auth_extension, "%.*s",
			(int)message->message.from_header->sip_headerinfo.uri->_extension.length,
			message->message.from_header->sip_headerinfo.uri->_extension.string);
	} else {
		sprintf(auth_extension, "%d", this->user_info.extension);
	}

	sprintf(first_value, "%s:%.*s:%s",
		((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
		(int)message->message.authinfo.realm.length,
		message->message.authinfo.realm.string,
		this->password());
	md5(first_value, ha1);
	sprintf(second_value, "%.*s:sip:%s",
		(int)message->message.request.length,
		message->message.request.string,
		message->message.to_header->sip_headerinfo.uri->ipaddress());
	md5(second_value, ha2);
	sprintf(third_value, "%s:%.*s:%s",
		ha1,
		(int)message->message.authinfo.nonce.length,
		message->message.authinfo.nonce.string,
		ha2);
	md5(third_value, response);
	if (message->message.authinfo.opaque.length > 0) {
		sprintf(authheader, "Authorization: Digest username=\"%s\",realm=\"%.*s\",nonce=\"%.*s\",opaque=\"%.*s\",uri=\"sip:%s\",response=\"%s\",algorithm=MD5\r\n",
			((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
			(int)message->message.authinfo.realm.length,
			message->message.authinfo.realm.string,
			(int)message->message.authinfo.nonce.length,
			message->message.authinfo.nonce.string,
			(int)message->message.authinfo.opaque.length,
			message->message.authinfo.opaque.string,
			message->message.to_header->sip_headerinfo.uri->ipaddress(),
			response);

	} else {
		sprintf(authheader, "Authorization: Digest username=\"%s\",realm=\"%.*s\",nonce=\"%.*s\",uri=\"sip:%s\",response=\"%s\",algorithm=MD5\r\n",
			((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
			/*message->message.from_header->sip_headerinfo.uri->extension(),*/
			(int)message->message.authinfo.realm.length,
			message->message.authinfo.realm.string,
			(int)message->message.authinfo.nonce.length,
			message->message.authinfo.nonce.string,
			message->message.to_header->sip_headerinfo.uri->ipaddress(),
			response);
	}

    dialog->dialog_info.mine.cseq++;
	dialog->dialog_errors++;
	dialog->dialog_info.theirs.tag[0] = 0;
	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_address)) return;
	js = new JSTRING(dialog->primary->pmessage);
	//MEMORY::add_pointer_to_track(js, (char *)"signalling.cpp > SIGNAL::auth(): js = new JSTRING(dialog->primary->pmessage);");
	if (js) {
        char newcseqline[500];
		for (i = 1; i <= js->linecount; i++) {
			line = js->line((int)i, &linelength);
			line = JSTRING::trim(line, &linelength, &linelength);
			if (linelength > 0) {
				/* It's some other line/header, just add it. */
                if (JSTRING::matches(line, (char *)"CSeq:")) {
                    sprintf(newcseqline, "CSeq: %d %.*s\r\n", ++dialog->dialog_info.mine.cseq,
                            (int)message->message.request.length,
                            message->message.request.string);
                    strcat(thispacket, newcseqline);
                } else {
                    strncat(thispacket, line, linelength);
                    strcat(thispacket, "\r\n");
                }
			} else {
				/* Blank Line */
				strcat(thispacket, authheader);
				strcat(thispacket, "\r\n");
				if (dialog->primary->message.content.rawcontent) {
					strcat(thispacket, dialog->primary->message.content.rawcontent);
				}
				break;
			}

		}
		if (this->psocket) {
			showsip(thispacket);
			db_send(this->psocket->s, thispacket, strlen(thispacket), 0);
		}

		//        printf("<------ New message with AUTH ------------------------------------->\n");
		//        printf("%s", thispacket);
		//        printf("<------------------------------------------------------------------>\n\n");
		//MEMORY::deleted(js);
		delete js;
	}


	return;
}



void SIGNAL::auth(SIP *message, struct SIGNAL::__socket *psocketstruct) {
	/*
	The SIP *message to which we're responding should be a 401 Unauthorized message.  We're not going to
	queue this message.  We're going to send it back out the same pipe (socket) that
	*/
	char thispacket[5000];
	char authheader[1000];
	char ha1[33], ha2[33], response[33], first_value[500], second_value[500], third_value[500];
	char my_address[50], server_address[50];
	char newtag[255], newbranch[255], callid[255];
	//char debug[200];

	if (!this->registration) return;

	memset(thispacket, 0, 3000);
	//HA1
	// "username(extension)":"asterisk":"password"
	//                        (realm)
	//OutputDebugString("Calculating MD5 Digest...\n");
	sprintf(first_value, "%s:%.*s:%s",
		((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
		(int)message->message.authinfo.realm.length,
		message->message.authinfo.realm.string,
		this->password());
	//sprintf(debug, "\tHA1: %s\n", first_value);
	//OutputDebugString(debug);
	md5(first_value, ha1);

	// HA2
	//  HA1:"sip":requestline's IP Address
	sprintf(second_value, "%.*s:sip:%s",
		(int)message->message.request.length,
		message->message.request.string,
		message->message.to_header->sip_headerinfo.uri->ipaddress());
	//sprintf(debug, "\tHA2: %s\n", second_value);
	//OutputDebugString(debug);

	md5(second_value, ha2);

	// HA3
	// First Hash:nonce:Second Hash
	sprintf(third_value, "%s:%.*s:%s",
		ha1,
		(int)message->message.authinfo.nonce.length,
		message->message.authinfo.nonce.string,
		ha2);
	//sprintf(debug, "\tHA3: %s\n", third_value);
	//OutputDebugString(debug);

	md5(third_value, response);


	if (message->message.authinfo.opaque.length > 0) {
		sprintf(authheader, "username=\"%s\",realm=\"%.*s\",nonce=\"%.*s\",opaque=\"%.*s\",uri=\"sip:%s\",response=\"%s\",algorithm=MD5",
			((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
			(int)message->message.authinfo.realm.length,
			message->message.authinfo.realm.string,
			(int)message->message.authinfo.nonce.length,
			message->message.authinfo.nonce.string,
			(int)message->message.authinfo.opaque.length,
			message->message.authinfo.opaque.string,
			message->message.to_header->sip_headerinfo.uri->ipaddress(),
			response);

	} else {
		sprintf(authheader, "username=\"%s\",realm=\"%.*s\",nonce=\"%.*s\",uri=\"sip:%s\",response=\"%s\",algorithm=MD5",
			((strlen(this->user_info.name) > 0) ? this->user_info.name : message->message.from_header->sip_headerinfo.uri->extension()) /*message->message.from_header->sip_headerinfo.uri->extension()*/,
			/*message->message.from_header->sip_headerinfo.uri->extension(),*/
			(int)message->message.authinfo.realm.length,
			message->message.authinfo.realm.string,
			(int)message->message.authinfo.nonce.length,
			message->message.authinfo.nonce.string,
			message->message.to_header->sip_headerinfo.uri->ipaddress(),
			response);
	}
	this->newtag(newtag);
	this->newbranch(newbranch);
	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_address)) return;
	sprintf(callid, "%.*s", (int)message->message.callid_header->value.length, message->message.callid_header->value.string);
	sprintf(thispacket, SIGNAL::AUTHREGISTER,
		server_address /* %s */, // REGISTER
		my_address /* %s */, newbranch /* %s */, // Via
		((strlen(this->user_info.name)>0) ? this->user_info.name : "") /* %s */, this->user_info.extension /* %d */, server_address /* %s */, newtag /* %s */, // From:
		this->user_info.extension /* %d */, server_address /* %s */, // To:
		callid, // Call-ID
		++this->cseq_registration, // CSeq
		DEEDLEBOX_VERSION,
		this->user_info.extension, my_address, this->registration->message.expires, // Contact: (with ";expires=" parameter)
		this->registration->message.expires, // Expires
		authheader); // authorization header
	psocketstruct->last_connection_check = time(0);
	showsip(thispacket);
	db_send(psocketstruct->s, thispacket, strlen(thispacket), 0);
	//    printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	//    printf("%s\n\n", thispacket);
	//    printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");
	return;
}



int SIGNAL::queue(char *buffer, int len) {
	int result = 0;
	int i = 0;
	struct SIGNAL::_send_queue *oldqueue = 0, *newqueue = 0;
	if (len == 0) SIGNAL_FAIL(SIGNAL_NODATA);
	if (len > 5000) {
		SIGNAL_FAIL(SIGNAL_PACKET_TOO_LARGE);
	}
	this->lock();
//    OutputDebugString("ENTER SIGNAL::queue()\n");
//    sprintf(debug, "\nAdding to SEND queue. Current queue size: %d\n", this->send_queue_count);
//    OutputDebugString("<----------------------------------------->\n");
//    OutputDebugString(debug);
//    OutputDebugString(buffer);
//    OutputDebugString("<----------------------------------------->\n");
//    for (i = 0; i < this->send_queue_count; i++) {
//        sprintf(debug, "Message %d:\n", i);
//        OutputDebugString(debug);
//        OutputDebugString(this->send_queue[i].send_buffer);
//        OutputDebugString("<----------------------------------------->\n");
//    }
//    OutputDebugString("**********************************************\n\n\n");
	//queuesize = this->send_queue_count * sizeof(struct SIGNAL::_send_queue);
	//newqueuesize = ((this->send_queue_count + 1) * sizeof(struct SIGNAL::_send_queue));

	if (this->send_queue_count == 0) {
		/* This is the FIRST packet going into the send queue */
		//newqueue = (struct SIGNAL::_send_queue *)MEMORY::calloc_(sizeof(struct SIGNAL::_send_queue), 1, (char *)"signalling.cpp > queue(): send_queue_count == 0");
		newqueue = (struct SIGNAL::_send_queue *)calloc(sizeof(struct SIGNAL::_send_queue), 1);
	} else {
		//newqueue = (struct SIGNAL::_send_queue *)MEMORY::calloc_(sizeof(struct SIGNAL::_send_queue), this->send_queue_count + 1, (char *)"signalling.cpp > queue(): send_queue_count != 0");
		newqueue = (struct SIGNAL::_send_queue *)calloc(sizeof(struct SIGNAL::_send_queue), this->send_queue_count + 1);
		oldqueue = this->send_queue;
	}
	if (newqueue) {
		/* Copy pending queue data to larger array  */
		for (i = 0; i < this->send_queue_count; i++) {
			newqueue[i].last_send_attempt = this->send_queue[i].last_send_attempt;
			newqueue[i].len = this->send_queue[i].len;
			newqueue[i].max_retry = this->send_queue[i].max_retry;
			newqueue[i].send_buffer = this->send_queue[i].send_buffer;
		}

		/* Fill in our new data at the end of the queue */
		newqueue[this->send_queue_count].last_send_attempt = 0;
		newqueue[this->send_queue_count].len = len;
		/* Allocate buffer size */
		/*
		newqueue[this->send_queue_count].send_buffer =
			(char *)MEMORY::calloc_((len + 1), 1, (char *)"signalling.cpp > queue(): newqueue[this->send_queue_count].send_buffer");
		*/

			newqueue[this->send_queue_count].send_buffer =
			(char *)calloc((len + 1), 1);


		/* Copy message to be xmitted */
		memmove(newqueue[this->send_queue_count].send_buffer, buffer, len);

		/* Set the new queue pointer as the queue */
		this->send_queue = (struct SIGNAL::_send_queue *)newqueue;
		result = (++this->send_queue_count);
		if (oldqueue) free(oldqueue);// MEMORY::free_(oldqueue);
	} else {
		this->last_message = SIGNAL_OUT_OF_MEMORY;
	}
	this->unlock();
//    OutputDebugString("EXIT SIGNAL::queue()\n");
	return result;
}



void SIGNAL::request_terminated(DIALOG *dialog) {
	char okmessage[3000];
	SIP *message = 0;
	char contact[300];
	if (!dialog) return;
	message = dialog->primary;

	//if (message->message.contact_header == 0) {
	sprintf(contact, "\"%s\" <sip:%d@%s:%u>",
		((strlen(this->user_info.name) > 0) ? this->user_info.name : ""),
		this->user_info.extension,
		this->user_info.ipaddress,
		this->user_info.rport);
	/*} else {
	sprintf(contact, "%.*s",
	((message->message.contact_header->value.length<300)?(int)message->message.contact_header->value.length:299),
	message->message.contact_header->value.string);
	}*/


	sprintf(okmessage, "\
SIP/2.0 487 Request Terminated\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
From: %.*s\r\n\
To: %.*s;tag=%s\r\n\
Call-ID: %.*s\r\n\
CSeq: %.*s\r\n\
Contact: %s\r\n\
Content-Length: 0\r\n\r\n",
(int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
(int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
this->signal_port,
this->voip_server_ip,
(int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
message->message.via_header->sip_headerinfo.sip_via.branch.string,
(int)message->message.from_header->value.length, // From
message->message.from_header->value.string, // From
(int)message->message.to_header->value.length, // To
message->message.to_header->value.string, // To
dialog->dialog_info.mine.tag, // To (tag)
(int)message->message.callid_header->value.length, // Call-ID
message->message.callid_header->value.string, // Call-ID
(int)message->message.csq_header->value.length, // CSeq
message->message.csq_header->value.string, // CSeq
contact);
	/*
	printf("<--------------- SENDING -------------------------------------------------->\n");
	printf("%s\n", okmessage);
	printf("<-------------------------------------------------------------------------->\n\n");
	*/
	/*
	sprintf(debug, "SIP/2.0 200 OK => %d %.*s\n\n", message->message.csq_header->sip_headerinfo.sip_cseq.sequence,
	(int)message->message.csq_header->sip_headerinfo.sip_cseq.method.length,
	message->message.csq_header->sip_headerinfo.sip_cseq.method.string);
	OutputDebugString(debug);
	*/
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}
}


void SIGNAL::ok(SIP *message) {
	char okmessage[3000], newtag[500];
	this->newtag(newtag);
    DIALOG *d = this->get_dialog(message->message.callid_header->value.string, message->message.callid_header->value.length);
    if (d) {
        sprintf(newtag, "%.*s", (int)message->message.to_header->sip_headerinfo.uri->_tag.length,  message->message.to_header->sip_headerinfo.uri->_tag.string);
    }
    
//    if (JSTRING::matches(message->message.request.string, "MESSAGE")) {
//        int *r = 0;
//        printf("Breaking in MESSSAGE %d!\n", *r);
//    }
	sprintf(okmessage, "\
SIP/2.0 200 Ok\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
From: <sip:%.*s@%.*s>;tag=%.*s\r\n\
To: <sip:%.*s@%.*s>;tag=%s\r\n\
Call-ID: %.*s\r\n\
CSeq: %.*s\r\n\
Content-Length: 0\r\n\r\n",
    (int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
    message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
    (int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
    message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
    message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
    this->signal_port,
    this->voip_server_ip,
    (int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
    message->message.via_header->sip_headerinfo.sip_via.branch.string,
    (int)message->message.from_header->sip_headerinfo.uri->_extension.length, // From
    message->message.from_header->sip_headerinfo.uri->_extension.string, // From
    (int)message->message.from_header->sip_headerinfo.uri->_ipaddress.length, // From
    message->message.from_header->sip_headerinfo.uri->_ipaddress.string, // From
    (int)message->message.from_header->sip_headerinfo.uri->_tag.length, // From
    message->message.from_header->sip_headerinfo.uri->_tag.string, // From
    (int)message->message.to_header->sip_headerinfo.uri->_extension.length, // To
    message->message.to_header->sip_headerinfo.uri->_extension.string, // To
    (int)message->message.to_header->sip_headerinfo.uri->_ipaddress.length, // To
    message->message.to_header->sip_headerinfo.uri->_ipaddress.string, // To
    newtag, // To (tag)
    (int)message->message.callid_header->value.length, // Call-ID
    message->message.callid_header->value.string, // Call-ID
    (int)message->message.csq_header->value.length, // CSeq
    message->message.csq_header->value.string /* CSeq */);
													 /*
													 printf("<--------------- SENDING -------------------------------------------------->\n");
													 printf("%s\n", okmessage);
													 printf("<-------------------------------------------------------------------------->\n\n");
													 */
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}
}


void SIGNAL::ack(DIALOG *dialog) {
	char okmessage[3000], newtag[500], my_address[100], server_ip[100];
	this->newtag(newtag);
	if (!dialog) return;
	if (!this->isregistered()) return;
	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;

	sprintf(okmessage, "\
ACK sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
From: <sip:%d@%s>;tag=%s\r\n\
Max-Forwards: 70\r\n\
Call-ID: %s\r\n\
CSeq: %d ACK\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Content-Length: 0\r\n\r\n",
dialog->dialog_info.theirs.extension, server_ip,
my_address, dialog->dialog_info.branch,
dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress, dialog->dialog_info.theirs.tag,
this->user_info.extension, dialog->dialog_info.mine.ipaddress, dialog->dialog_info.mine.tag,
dialog->dialog_info.callid,
dialog->dialog_info.mine.cseq,
this->user_info.extension, my_address);

	//sprintf(debug, "SIP/2.0 ACK => %d INVITE\n\n", dialog->dialog_info.theirs.cseq);
	//OutputDebugString(okmessage);
	//this->queue(okmessage, (int)strlen(okmessage));
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}

	return;
}


void SIGNAL::ringing(DIALOG *dialog) {
	char okmessage[3000];
	SIP *message = 0;
	if (!dialog) return;
	message = dialog->primary;
	sprintf(okmessage, "\
SIP/2.0 180 Ringing\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
From: %.*s\r\n\
To: %.*s;tag=%s\r\n\
Call-ID: %.*s\r\n\
CSeq: %.*s\r\n\
Contact: %.*s\r\n\
Content-Length: 0\r\n\r\n",
(int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
(int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
this->signal_port,
this->voip_server_ip,
(int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
message->message.via_header->sip_headerinfo.sip_via.branch.string,
(int)message->message.from_header->value.length, // From
message->message.from_header->value.string, // From
(int)message->message.to_header->value.length, // To
message->message.to_header->value.string, // To
dialog->dialog_info.mine.tag, // To (tag)
(int)message->message.callid_header->value.length, // Call-ID
message->message.callid_header->value.string, // Call-ID
(int)message->message.csq_header->value.length, // CSeq
message->message.csq_header->value.string, // CSeq
(int)message->message.contact_header->value.length, // Contact
message->message.contact_header->value.string /* Contact*/);
	/*
	printf("<--------------- SENDING -------------------------------------------------->\n");
	printf("%s\n", okmessage);
	printf("<-------------------------------------------------------------------------->\n\n");
	*/
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}


	return;
}

void SIGNAL::transfer(char *callid, char *xferto) {
	char refermessage[3000], my_address[100], server_ip[100];
	DIALOG *dialog = this->get_dialog(callid);
	if (!dialog) return;
	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;
	if (!xferto) return;
	if (strlen(xferto) == 0 || strlen(xferto) > 50) return;
	sprintf(refermessage, "\
REFER sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
Contact: <sip:%s@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: %d REFER\r\n\
Max-Forwards: 70\r\n\
Event: refer\r\n\
Supported: replaces, 100rel, timer, norefersub\r\n\
Accept: message/sipfrag;version=2.0\r\n\
Allow-Events: presence, message-summary, refer\r\n\
Refer-To: sip:%s@%s\r\n\
Referred-By: <sip:%s@%s;transport=tcp>\r\n\
User-Agent: Deedle Core\r\n\
Content-Length: 0\r\n\r\n",
dialog->dialog_info.theirs.extension, server_ip,                                    /* Request Line */
my_address, dialog->dialog_info.branch,                                             /* Via */
dialog->dialog_info.theirs.extension, server_ip, dialog->dialog_info.theirs.tag,    /* To */
dialog->dialog_info.mine.extension, my_address, dialog->dialog_info.mine.tag,       /* From */
dialog->dialog_info.mine.extension, my_address,                                     /* Contact */
dialog->dialog_info.callid,                                                         /* Call-ID */
++dialog->dialog_info.mine.cseq,                                                    /* CSeq */
xferto, server_ip,                                                                  /* Refer-To */
dialog->dialog_info.mine.extension, my_address                                      /* Referred-By */
);
	printf("%s\n", refermessage);
	if (this->psocket) {
		showsip(refermessage);
		db_send(this->psocket->s, refermessage, strlen(refermessage), 0);
	}

	return;
}

void SIGNAL::busy(SIP *message) {
	char okmessage[3000], newtag[500];
	this->newtag(newtag);
	sprintf(okmessage, "\
SIP/2.0 486 Busy Here\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
From: %.*s\r\n\
To: %.*s;tag=%s\r\n\
Call-ID: %.*s\r\n\
CSeq: %.*s\r\n\
Contact: %.*s\r\n\
Content-Length: 0\r\n\r\n",
(int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
(int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
this->signal_port,
this->voip_server_ip,
(int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
message->message.via_header->sip_headerinfo.sip_via.branch.string,
(int)message->message.from_header->value.length, // From
message->message.from_header->value.string, // From
(int)message->message.to_header->value.length, // To
message->message.to_header->value.string, // To
newtag, // To (tag)
(int)message->message.callid_header->value.length, // Call-ID
message->message.callid_header->value.string, // Call-ID
(int)message->message.csq_header->value.length, // CSeq
message->message.csq_header->value.string, // CSeq
(int)message->message.contact_header->value.length, // Contact
message->message.contact_header->value.string /* Contact*/);
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}
}


void SIGNAL::busy(DIALOG *dialog) {
	char okmessage[3000];
	SIP *message = 0;
	if (!dialog) return;
	message = dialog->primary;
	sprintf(okmessage, "\
SIP/2.0 486 Busy Here\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
From: %.*s\r\n\
To: %.*s;tag=%s\r\n\
Call-ID: %.*s\r\n\
CSeq: %.*s\r\n\
Contact: %.*s\r\n\
Content-Length: 0\r\n\r\n",
(int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
(int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
this->signal_port,
this->voip_server_ip,
(int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
message->message.via_header->sip_headerinfo.sip_via.branch.string,
(int)message->message.from_header->value.length, // From
message->message.from_header->value.string, // From
(int)message->message.to_header->value.length, // To
message->message.to_header->value.string, // To
dialog->dialog_info.mine.tag, // To (tag)
(int)message->message.callid_header->value.length, // Call-ID
message->message.callid_header->value.string, // Call-ID
(int)message->message.csq_header->value.length, // CSeq
message->message.csq_header->value.string, // CSeq
(int)message->message.contact_header->value.length, // Contact
message->message.contact_header->value.string /* Contact*/);
	if (this->psocket) {
		showsip(okmessage);
		db_send(this->psocket->s, okmessage, strlen(okmessage), 0);
	}
}


int SIGNAL::isSIP(char *ptr) {
	int rq = this->isSIPrequest(ptr);
	if (!rq) return this->isSIPresponse(ptr);
	return rq;
}


int SIP::issip(char *ptr) {
	int rq = SIP::isSIPrequest(ptr);
	if (!rq) return SIP::isSIPresponse(ptr);
	return rq;
}

int SIP::issip() {
	int rq = SIP::isSIPrequest(this->pmessage);
	if (!rq) return SIP::isSIPresponse(this->pmessage);
	return rq;
}


char *SIGNAL::trim(char *ptr, int len) {
	int i = 0;
	if (!ptr) return (char *)SIGNAL::empty;
	for (; i < len; i++) {
		if (ptr[i] != ' ' &&
			ptr[i] != '\t' &&
			ptr[i] != '\r' &&
			ptr[i] != '\n') {
			return &ptr[i];
		}
	}
	return &ptr[len];
}

int SIGNAL::linecount(char *ptr, int len) {
	int lc = 0, i = 0;
	char *lineending = 0;
	if (!ptr) return 0;
	lineending = ptr;
	for (; &lineending[i] < &ptr[len]; i++) {
		if (lineending[i] == '\n') {
			lc++;
		}
	}
	return lc;
}

int SIGNAL::linecount(char *ptr) {
	if (!ptr) return 0;
	return (int)this->linecount(ptr, (int)strlen(ptr));
}



char *SIGNAL::line(char *ptr, int count) {
	char *retline = 0;
	int j = 0;
	if (count == 0) return (char *)SIGNAL::empty;
	if (count > this->linecount(ptr)) return (char *)SIGNAL::empty;
	retline = ptr;
	for (j = 1; j < count; j++) {
		retline = strstr(retline, "\n");
		if (!retline) return (char *)SIGNAL::empty;
		retline += 1;
	}
	return retline;
}


int SIGNAL::matches(char *str1, char *str2) {
	size_t i = 0;
	//char c1 = 0, c2 = 0;
	if (!str1 || !str2) return 0;
	for (; i < ((strlen(str1) > strlen(str2)) ? strlen(str2) : strlen(str1)); i++) {
		//c1 = LOWER(str1[i]);
		//c2 = LOWER(str2[i]);
		if (LOWER(str1[i]) != LOWER(str2[i])) return 0;
	}
	return 1;
}

int SIGNAL::linelength(char *ptr) {
	char *lineend = 0;
	if (!ptr) return 0;
	lineend = strstr(ptr, "\n");
	if (lineend) return (int)(lineend - this->trim(ptr, (int)(lineend - ptr)));
	return 0;
}


char *SIGNAL::headervalue(char *headerptr) {
	int i = 0, itlen = 0;
	if (!headerptr) return (char *)SIGNAL::empty;
	itlen = (int)strlen(headerptr);
	for (; i < itlen; i++) {
		if (headerptr) {
			switch (headerptr[i]) {
				case '\n':
					return (char *)SIGNAL::empty;
				case ':':
					if (i + 1 <= itlen) {
						return this->trim(&headerptr[(i + 1)], itlen - i);
					}
					return (char *)SIGNAL::empty;
				default:
					break;
			}
		}
	}
	return (char *)SIGNAL::empty;
}


void SIP::setsip(char *sip) {
	if (this->pmessage[0] != 0) {
		printf("signalling.cpp > SIP::setsip(): this->pmessage is in use.  Calling this->clear()\n");
		this->clear();
	}
	if (!SIP::issip(sip)) {
		printf("signalling.cpp > SIP::setsip(): SIP::issip() returned FALSE for the following data:\n");
		printf("%s\n\n", sip);
		return;
	}
	this->sipsize = strlen(sip);

	memmove(this->pmessage, sip, ((this->sipsize > (MAX_SIP_MESSAGE_SIZE - 1)) ? MAX_SIP_MESSAGE_SIZE - 1 : this->sipsize));
	this->parseSIP();
}

void SIP::setsip(char *sip, size_t length) {
	if (this->pmessage[0] != 0) {
		printf("signalling.cpp > SIP::setsip(): this->pmessage is in use.  Calling this->clear()\n");
		this->clear();
	}
	if (length == 0) {
		printf("signalling.cpp > SIP::setsip(): Length passed was ZERO\n");
		return;
	}
	if (!SIP::issip(sip)) {
		printf("signalling.cpp > SIP::setsip(): SIP::issip() returned FALSE for the following data:\n");
		printf("%s\n\n", sip);
		return;
	}
	this->sipsize = length;
	memmove(this->pmessage, sip, ((this->sipsize > (MAX_SIP_MESSAGE_SIZE - 1)) ? MAX_SIP_MESSAGE_SIZE - 1 : this->sipsize));
	this->parseSIP();
}


int SIP::parseSIP() {
	size_t i = 0, linelength = 0,
		auth_iter = 0, stalelength = 0, j = 0,
		sdplinelength = 0;
	JSTRING *siptext = 0;
	JSTRING *sdplines = 0;
	JSTRING::jsstring **viasplit = 0, **sdpparams = 0;
	size_t viacounter = 0, sdpparamitr = 0;
	char *line = 0, *valstopptr = 0, *stale = 0, *sdpline = 0,
		*sdpstart = 0;
	struct _headerlist *thisheader = 0;
	VOIP_PROTOCOLS voip_proto = VOIP_SIP;
	char debug[500];

	if ( /* SIP message string is empty */ this->pmessage[0] == 0) return 0;
	/* Parse the SIP Message Here */
	this->stats.created = time(0);
	siptext = new JSTRING(this->pmessage);
	//MEMORY::add_pointer_to_track(siptext, (char *)"signalling.cpp > SIP::parseSIP(): siptext = new JSTRING(this->pmessage);");
	for (i = 1; i <= siptext->linecount; i++) {
		line = siptext->line((int)i, &linelength);
//        printf("Parsing Line: %.*s\n", (int)linelength, line);
		if (i == 1) {
			/*
			This is the first line. Is it a REQUEST or a RESPONSE?
			Set the response code if applicable.
			*/
		} else {
			if (voip_proto == VOIP_SIP) {
//                sprintf(debug, "Content-Type: %d\tParsing line: \"%.*s\" (%d chars)\n", this->message.content_type, (int)linelength, line, linelength);
//                OutputDebugString(debug);
				if (linelength > 0) {
					if ((thisheader = this->addheader(line))) {

						if (JSTRING::matches(line, (char *)"Via")) { // parse via line
																	 //OutputDebugString("PARSING SIP VIA LINE.\n");
							thisheader->sip_headerinfo.headertype = SIG_VIA;
							this->message.via_header = thisheader;
							this->message.via_header->sip_headerinfo.sip_via.rport.length = 0;
							this->message.via_header->sip_headerinfo.sip_via.rport.string = 0;


							//thisheader->sip_headerinfo.sip_via.

							viasplit = JSTRING::split(line, ' ', linelength);
							viacounter = 0;
							// Via: SIP/2.0/TCP 107.182.238.227:5060;rport;branch=z9hG4bKPjfc28d13b-e184-4de3-85c7-84f4a99614be;alias
							if (viasplit) {
								while (viasplit[viacounter]) {
									if (JSTRING::matches(viasplit[viacounter]->ptr, (char *)"SIP/")) {

										thisheader->sip_headerinfo.sip_via.version.string =
											viasplit[viacounter]->ptr;
										thisheader->sip_headerinfo.sip_via.version.length =
											viasplit[viacounter]->length;
										viacounter++; /* We know we're at the SIP version / protocol, we know the IP/PORT is next so we'll move into it. */
										if (viasplit[viacounter]) {
											char *port, *portstop;
											size_t portlength;
											thisheader->sip_headerinfo.sip_via.ipaddress.string =
												JSTRING::stringfield(viasplit[viacounter]->ptr, ';', 1, &thisheader->sip_headerinfo.sip_via.ipaddress.length);
											/* Now, the ipaddress field contains the IP address and the port */
											port = JSTRING::between(thisheader->sip_headerinfo.sip_via.ipaddress.string, ":;", &portlength);
											if (portlength) {
												thisheader->sip_headerinfo.sip_via.ipaddress.length =
													(port - thisheader->sip_headerinfo.sip_via.ipaddress.string) - 1;
												/* possible bug? */
												//if (thisheader->sip_headerinfo.sip_via.ipaddress.length < 0)
												//	thisheader->sip_headerinfo.sip_via.ipaddress.length = 0;
												portstop = &port[portlength];
												thisheader->sip_headerinfo.sip_via.usrport = (unsigned short)strtoull(port, &portstop, 10);
											}
										}
										break;
									}
									viacounter++;
								}


								JSTRING::freesplit(viasplit);
								viasplit = 0;
							}
							viasplit = JSTRING::split(line, ';', linelength);
							if (viasplit) {
								viacounter = 0;
								while (viasplit[viacounter]) {
									/*
									sprintf(debug, "\t%.*s\n", viasplit[viacounter]->length, viasplit[viacounter]->ptr);
									OutputDebugString(debug);
									*/
									/* Check known VIA parameters i.e. rport, received, branch */
									if (JSTRING::matches(viasplit[viacounter]->ptr, (char *)"rport")) {

										thisheader->sip_headerinfo.sip_via.rport.string =
											JSTRING::keyvalue(viasplit[viacounter]->ptr, &viasplit[viacounter]->length, &viasplit[viacounter]->length);
										thisheader->sip_headerinfo.sip_via.rport.length = viasplit[viacounter]->length;

										if (thisheader->sip_headerinfo.sip_via.rport.length > 0) {
											valstopptr = &thisheader->sip_headerinfo.sip_via.rport.string[thisheader->sip_headerinfo.sip_via.rport.length];
											thisheader->sip_headerinfo.sip_via.usrport = (unsigned short)
												strtoull(thisheader->sip_headerinfo.sip_via.rport.string,
													&valstopptr, 10);
										}
									} else if (JSTRING::matches(viasplit[viacounter]->ptr, (char *)"received")) {

										thisheader->sip_headerinfo.sip_via.received.string =
											JSTRING::keyvalue(viasplit[viacounter]->ptr, &viasplit[viacounter]->length, &viasplit[viacounter]->length);

										thisheader->sip_headerinfo.sip_via.received.length = viasplit[viacounter]->length;

									} else if (JSTRING::matches(viasplit[viacounter]->ptr, (char *)"branch")) {
										thisheader->sip_headerinfo.sip_via.branch.string =
											JSTRING::keyvalue(viasplit[viacounter]->ptr, &viasplit[viacounter]->length, &viasplit[viacounter]->length);
										thisheader->sip_headerinfo.sip_via.branch.length = viasplit[viacounter]->length;
									} else {
										sprintf(debug, "Unrecognized VIA parameter: %.*s\n", (int)viasplit[viacounter]->length, viasplit[viacounter]->ptr);
									}
									viacounter++;
								}
								//OutputDebugString("\nDone parsing VIA\n\n");
								JSTRING::freesplit(viasplit);
							}

						} else if (JSTRING::matches(line, (char *)"Content-Length")) {
							valstopptr = &thisheader->value.string[thisheader->value.length];
							this->message.contentlength = (int)strtoull(thisheader->value.string, &valstopptr, 10);
							thisheader->sip_headerinfo.headertype = SIG_CONTENTLENGTH;
						} else if (JSTRING::matches(line, (char *)"Content-Type")) {
							/* Set the next content type here */
							thisheader->sip_headerinfo.headertype = SIG_CONTENTTYPE;
							if (JSTRING::matches(thisheader->value.string, (char *)"application/sdp")) {
								this->message.content_type = CT_SDP;
							} else if (JSTRING::matches(thisheader->value.string, (char *)"message/sipfrag")) {
								this->message.content_type = CT_SIPFRAG;
							} else if (JSTRING::matches(thisheader->value.string, (char *)"text/plain")) {
								this->message.content_type = CT_TEXT;
							} else if (JSTRING::matches(thisheader->value.string, (char *)"text/html")) {
								this->message.content_type = CT_HTML;
							} else if (JSTRING::matches(thisheader->value.string, (char *)"application/simple-message-summary")) {
								this->message.content_type = CT_SIMPLE_MESSAGE_SUMMARY;
							} else {
								/* Other Content-Type */
								printf("* UNKNOWN CONTENT-TYPE: %.*s\n", (int)thisheader->value.length, thisheader->value.string);
								this->message.content_type = CT_UNKNOWN;
							}
						} else if (JSTRING::matches(line, (char *)"Call-ID")) {
							thisheader->sip_headerinfo.headertype = SIG_CALLID;
							this->message.callid_header = thisheader;
						} else if (JSTRING::matches(line, (char *)"From")) {
							thisheader->sip_headerinfo.headertype = SIG_FROM;
							this->message.from_header = thisheader;

							thisheader->sip_headerinfo.uri =
								new SIP_URI(thisheader->value.string, (size_t)thisheader->value.length);

						} else if (JSTRING::matches(line, (char *)"To")) {
							thisheader->sip_headerinfo.headertype = SIG_TO;
							this->message.to_header = thisheader;
							thisheader->sip_headerinfo.uri =
								new SIP_URI(thisheader->value.string, (size_t)thisheader->value.length);
						} else if (JSTRING::matches(line, (char *)"CSeq")) {
							this->message.csq_header = thisheader;
							thisheader->sip_headerinfo.headertype = SIG_CSEQ;
							size_t templength = 0;
							char *_topasstostrtoi64;

							/*
							Check this in debug, the string ("line") in question
							is most likely "CSeq: 102 OPTIONS" and we're
							retrieving............^^^^^^^^^^^
							||| |||||||
							sequence <-----/   \--------> method
							*/
							// Here, was are filling the following values
							//(SIPSTRING) thisheader->sip_headerinfo.sip_cseq.method
							//      (int) thisheader->sip_headerinfo.sip_cseq.sequence
							thisheader->sip_headerinfo.sip_cseq.method.string =
								JSTRING::stringfield(
									thisheader->value.string,
									' ',
									'\n',
									1,
									&thisheader->sip_headerinfo.sip_cseq.method.length);

							this->message.request.string =
								JSTRING::trim(
									JSTRING::stringfield(
										thisheader->value.string,
										' ',
										'\n',
										2,
										&templength),
									&templength, &templength);


							this->message.request.length = templength;
							_topasstostrtoi64 = &thisheader->value.string[templength];

							this->message.csq_header->sip_headerinfo.sip_cseq.method.string = this->message.request.string;
							this->message.csq_header->sip_headerinfo.sip_cseq.method.length = this->message.request.length;


							thisheader->sip_headerinfo.sip_cseq.sequence
								= (int)strtoull(thisheader->value.string, &_topasstostrtoi64, 10);

						} else if (JSTRING::matches(line, (char *)"WWW-Authenticate")) {
							this->message.authneeded = 1;
							this->message.authinfo.step = SENDAUTH;
							this->message.authinfo.stale = 0;
							JSTRING::jsstring **authlist = 0;
							thisheader->sip_headerinfo.headertype = SIG_WWWAUTHENTICATE;
							//this->message.authinfo
							if (JSTRING::matches((char *)"digest", thisheader->value.string)) {
								JSTRING::jsstring authparams;
								authparams.ptr =
									JSTRING::stringfield(thisheader->value.string, ' ',
										thisheader->value.length, 2, &authparams.length);
								authparams.length = &thisheader->value.string[thisheader->value.length]
									- authparams.ptr;
								authparams.ptr = JSTRING::trim(authparams.ptr, &authparams.length, &authparams.length);
								//sprintf(debug, "*** Auth params to parse: \"%.*s\" ***\n", authparams.length, authparams.ptr);
								//OutputDebugString(debug);
								authlist = JSTRING::split(authparams.ptr, ',', authparams.length);
								if (authlist) {
									auth_iter = 0;
									while (authlist[auth_iter] != 0) {
										//sprintf(debug, "\t%.*s\n", authlist[auth_iter]->length, authlist[auth_iter]->ptr);
										//OutputDebugString(debug);
										authlist[auth_iter]->ptr = JSTRING::trim(authlist[auth_iter]->ptr, &authlist[auth_iter]->length, &authlist[auth_iter]->length);
										if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"realm")) {
											this->message.authinfo.realm.string =
												JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
													authlist[auth_iter]->length,
													2,
													&this->message.authinfo.realm.length);
											this->message.authinfo.realm.string =
												JSTRING::unquote(
													this->message.authinfo.realm.string,
													&this->message.authinfo.realm.length,
													&this->message.authinfo.realm.length);
										} else if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"stale")) {
											stale = JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
												authlist[auth_iter]->length,
												2,
												&stalelength);
											stale = JSTRING::trim(stale, &stalelength, &stalelength);
											if (JSTRING::matches((char *)"true", stale) ||
												JSTRING::matches((char *)"1", stale)) {
												this->message.authinfo.stale = 1;
												this->message.authinfo.step = RESTART;
											}
										} else if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"nonce")) {
											this->message.authinfo.nonce.string =
												JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
													authlist[auth_iter]->length,
													2,
													&this->message.authinfo.nonce.length);
											this->message.authinfo.nonce.string =
												JSTRING::unquote(
													this->message.authinfo.nonce.string,
													&this->message.authinfo.nonce.length,
													&this->message.authinfo.nonce.length);
										} else if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"opaque")) {
											this->message.authinfo.opaque.string =
												JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
													authlist[auth_iter]->length,
													2,
													&this->message.authinfo.opaque.length);
											this->message.authinfo.opaque.string =
												JSTRING::unquote(
													this->message.authinfo.opaque.string,
													&this->message.authinfo.opaque.length,
													&this->message.authinfo.opaque.length);
										} else if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"algo")) {
											this->message.authinfo.algorithm.string =
												JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
													authlist[auth_iter]->length,
													2,
													&this->message.authinfo.algorithm.length);
											this->message.authinfo.algorithm.string =
												JSTRING::unquote(
													this->message.authinfo.algorithm.string,
													&this->message.authinfo.algorithm.length,
													&this->message.authinfo.algorithm.length);
										} else if (JSTRING::matches(authlist[auth_iter]->ptr, (char *)"qop")) {
											this->message.authinfo.qop.string =
												JSTRING::stringfield(authlist[auth_iter]->ptr, '=',
													authlist[auth_iter]->length,
													2,
													&this->message.authinfo.qop.length);
											this->message.authinfo.qop.string =
												JSTRING::unquote(
													this->message.authinfo.qop.string,
													&this->message.authinfo.qop.length,
													&this->message.authinfo.qop.length);
											//} else if (JSTRING::matches(authlist[auth_iter]->ptr, "realm")) {
										}
										auth_iter++;
									}

									/* Free the splt() array */
									JSTRING::freesplit(authlist);
									//OutputDebugString("\n\n");
								}
							} else {
								sprintf(debug, "Invalid WWW-Authenticate value: \"%.*s\"\n", (int)thisheader->value.length, thisheader->value.string);
							}

							//WWW-Authenticate: Digest  realm = "asterisk", nonce = "1528493294/f3031c07cbd8e40663da1ffb366a4e0a", opaque = "3141ac10328a28a3", algorithm = md5, qop = "auth"

						} else if (JSTRING::matches(line, (char *)"Contact")) {
							thisheader->sip_headerinfo.headertype = SIG_CONTACT;
							thisheader->sip_headerinfo.uri =
								new SIP_URI(thisheader->value.string, (size_t)thisheader->value.length);
							this->message.contact_header = thisheader;
						} else if (JSTRING::matches(line, (char *)"Allow")) {
							thisheader->sip_headerinfo.headertype = SIG_ALLOW;
						} else if (JSTRING::matches(line, (char *)"Accept")) {
							thisheader->sip_headerinfo.headertype = SIG_ACCEPT;
						} else if (JSTRING::matches(line, (char *)"Supported")) {
							thisheader->sip_headerinfo.headertype = SIG_SUPPORTED;
						} else if (JSTRING::matches(line, (char *)"Allow-Events")) {
							thisheader->sip_headerinfo.headertype = SIG_ALLOWEVENTS;
						} else if (JSTRING::matches(line, (char *)"Server")) {
							thisheader->sip_headerinfo.headertype = SIG_SERVER;
						} else if (JSTRING::matches(line, (char *)"expires")) {
							valstopptr = &thisheader->value.string[thisheader->value.length];
							this->message.expires = (int)strtoull(thisheader->value.string, &valstopptr, 10);
							thisheader->sip_headerinfo.headertype = SIG_EXPIRES;
						} else if (JSTRING::matches(line, (char *)"User-Agent")) {
							thisheader->sip_headerinfo.headertype = SIG_USERAGENT;
						} else if (JSTRING::matches(line, (char *)"Diversion")) {
							thisheader->sip_headerinfo.headertype = SIG_DIVERSION;
							thisheader->sip_headerinfo.uri =
								new SIP_URI(thisheader->value.string, (size_t)thisheader->value.length);
						} else if (JSTRING::matches(line, (char *)"P-Asserted-Identity")) {
							thisheader->sip_headerinfo.headertype = SIG_P_ASSERT_ID;
							thisheader->sip_headerinfo.uri =
								new SIP_URI(thisheader->value.string, (size_t)thisheader->value.length);
						} else {
							thisheader->sip_headerinfo.headertype = SIG_UNKNOWN;
						}
					}
				} else {
					/* this was a blank line, set the next protocol */
//                    printf("** BLANK LINE PARSING MESSAGE HEADERS, setting voip_proto = VOIP_BODY\n");
					voip_proto = VOIP_BODY;
                    if (this->message.content_type == CT_TEXT) {
                        if (this->message.contentlength > 0) {
                            if (line[0] == '\r') {
                                this->message.content.rawcontent = &line[2];
                            }
                        }
                    }
				}
			} else if (this->message.content_type == CT_SIPFRAG) {
				//                printf("SIPFRAG: line=%s\n", line);
				if (linelength > 0) {
					if (this->message.content.rawcontent == 0) {
						this->message.content.rawcontent = line;
					}
				}
            } else if (this->message.content_type == CT_TEXT) {
                if (linelength > 0) {
                    if (this->message.content.rawcontent == 0) {
                        this->message.content.rawcontent = line;
                    }
                }
			} else if (this->message.content_type == CT_SIMPLE_MESSAGE_SUMMARY) {
				if (linelength > 0) {
					if (this->message.content.rawcontent == 0) {
						this->message.content.rawcontent = line;
					}
				}
			} else if (this->message.content_type == CT_SDP) {
				// Just break for now.  We can parse the SDP later when we implement 
				// this as the entire SIP/RTP handler. parse sdp
				if (linelength > 0) {
					if (this->message.content.rawcontent == 0) {
						this->message.content.rawcontent = line;
						/*
						printf("<----- SDP to parse-------------------------->\n");
						printf("%s\n", this->message.content.rawcontent);
						printf("<-------------------------------------------->\n\n");
						*/
						sdplines = new JSTRING(line);
						//MEMORY::add_pointer_to_track(sdplines, (char *)"signalling.cpp > SIP::parseSIP(): sdplines = new JSTRING(line);");
						if (sdplines) {
							for (j = 1; j <= sdplines->linecount; j++) {
								sdpline = sdplines->line((int)j, &sdplinelength);
								if (sdplinelength > 2) {
									sdpstart = &sdpline[2];
									sdpparams = JSTRING::split(sdpstart, ' ', sdplinelength - 2);
									if (sdpline[1] == '=') {
										switch (sdpline[0]) {
											//case 'o': /* o=- 638910810 638910810 IN IP4 107.182.238.227 */
											/*    if (sdpparams) {
											sdpparamitr = 0;
											while (sdpparams[sdpparamitr] != 0) {
											if (sdpparamitr == 0) printf(" SDP Username: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											if (sdpparamitr == 1) printf("  Remote SSRC: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											if (sdpparamitr == 2) printf("  Session Num: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											if (sdpparamitr == 3) printf(" Xport Domain: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											if (sdpparamitr == 4) printf("Xport Version: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											if (sdpparamitr == 5) printf("  Source Addr: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
											sdpparamitr++;
											}
											}
											break;*/
											case 'c': /* c=IN IP4 107.182.238.227 */
												if (sdpparams) {
													sdpparamitr = 0;
													while (sdpparams[sdpparamitr] != 0) {
														//if (sdpparamitr == 0) printf("Media Dest Domain: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
														//if (sdpparamitr == 1) printf("   Dest Xport Ver: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
														if (sdpparamitr == 2) {
															sprintf(this->message.content.sdp.audio_address, "%.*s", ((sdpparams[sdpparamitr]->length > 99) ? 99 : (int)sdpparams[sdpparamitr]->length), sdpparams[sdpparamitr]->ptr);
															db_inet_pton(AF_INET, this->message.content.sdp.audio_address, &this->message.content.sdp.audio_destination.sin_addr.s_addr);
															//printf("  Media Dest Addr: %s\n", this->message.content.sdp.audio_address);
														}
														sdpparamitr++;
													}
												}
												break;
											case 'm': /* m=audio 17182 RTP/AVP 0 101 */
												if (sdpparams) {
													if (sdpparams[0] != 0) {
														if (!strncmp(sdpparams[0]->ptr, "audio", 5)) {
															sdpparamitr = 1;
															while (sdpparams[sdpparamitr] != 0) {
																if (sdpparamitr == 1) {
																	char *endptr = &sdpparams[sdpparamitr]->ptr[sdpparams[sdpparamitr]->length];
																	this->message.content.sdp.audio_port =
																		(unsigned short)strtoul(sdpparams[sdpparamitr]->ptr, &endptr, 10);
																	this->message.content.sdp.audio_destination.sin_port = htons(this->message.content.sdp.audio_port);
																	//printf("Audio Dest Port: %u\n", this->message.content.sdp.audio_port);
																} //else if (sdpparamitr > 2) {
																  //printf("\tSDP Payload Type Offered: %.*s\n", (int)sdpparams[sdpparamitr]->length, sdpparams[sdpparamitr]->ptr);
																  //}
																sdpparamitr++;
															}
														}
													}
												}
												break;
											case 'a': /* a=<header>:<value (JSTRING::split by ' ')> */
												if (sdplinelength >= 10) {
                                                    printf("SDP AUDIO DIRECTION: %.*s\n\n", 10, sdpstart);
													if (!strncmp(sdpstart, "sendrecv", 8)) {
														this->message.content.sdp.audio_direction = AD_SENDRECV;
													} else if (!strncmp(sdpstart, "recvonly", 8)) {
														this->message.content.sdp.audio_direction = AD_RECVONLY;
													} else if (!strncmp(sdpstart, "sendonly", 8)) {
														this->message.content.sdp.audio_direction = AD_SENDONLY;
													} else if (!strncmp(sdpstart, "inactive", 8)) {
														this->message.content.sdp.audio_direction = AD_INACTIVE;
													}
													if (sdplinelength > 19) {
														if (!strncmp(sdpstart, "rtpmap:0 PCMU/8000", 18)) {
															//printf("SDP uLaw offered!\n");
															this->message.content.sdp.ulaw_offered = 1;
														}
													}
												}
												/*
												a=rtpmap:0 PCMU/8000
												a=rtpmap:101 telephone-event/8000
												a=fmtp:101 0-16
												a=ptime:20
												a=maxptime:150
												a=sendrecv
												*/
												break;
											default:
												break;
										}
									}
									if (sdpparams) {
										JSTRING::freesplit(sdpparams);
										sdpparams = 0;
									}
								}
							}
							//MEMORY::deleted(sdplines);
							delete sdplines;
							sdplines = 0;
						}
						break;
					}
				}
			} /* Unknown protocol */ else if (this->message.content_type == CT_UNKNOWN) {
				// Just break and leave the unrecognized protocol at the end of the string.
				if (linelength > 0) {
					if (this->message.content.rawcontent == 0) {
						this->message.content.rawcontent = line;
					}
					printf("<----- Unknown Body Type -------------------->\n");
					printf("%s\n", this->message.content.rawcontent);
					printf("<-------------------------------------------->\n\n");
					break;
				}
            } else {
                printf("****** WTF??! ********\n");
                printf("CT = %d\n", this->message.content_type);
                printf("voip_proto = %d\n\n\n", voip_proto);
            }
		}
	} // End looping through string's lines.



	//MEMORY::deleted(siptext);
	delete siptext;
	this->isparsed = 1;
	return 1;
}


SIP::~_sip() {
	this->clear();
}

SIP::_sip(char *sip) {
	this->isparsed = 0;
	memset(&this->stats, 0, sizeof(SIP::_stats));
	memset(&this->message, 0, sizeof(SIP_message));
	memset(this->pmessage, 0, MAX_SIP_MESSAGE_SIZE);
	this->sipsize = 0;
	this->setsip(sip);
}

SIP::_sip(char *sip, size_t length) {
	this->isparsed = 0;
	memset(&this->stats, 0, sizeof(SIP::_stats));
	memset(&this->message, 0, sizeof(SIP_message));
	memset(this->pmessage, 0, MAX_SIP_MESSAGE_SIZE);
	this->sipsize = 0;
	this->setsip(sip, length);
}



void SIP::clear(void) {
	int i = 0;
	if (this->message.headercount > 0) {
		for (; i < this->message.headercount; i++) {

			if (this->message.headers[i]->sip_headerinfo.headertype == SIG_VIA) {
				if (this->message.headers[i]->sip_headerinfo.sip_via.via_params) {
					JSTRING::freesplit(this->message.headers[i]->sip_headerinfo.sip_via.via_params);
				}
			}

			/* Free up URI resource */
			if (this->message.headers[i]->sip_headerinfo.uri) {
				//MEMORY::deleted(this->message.headers[i]->sip_headerinfo.uri);
				delete this->message.headers[i]->sip_headerinfo.uri;
				this->message.headers[i]->sip_headerinfo.uri = 0;
			}
			/*
			printf("Freeing header: %.*s:%.*s\n",
			(int)this->message.headers[i]->header.length,
			this->message.headers[i]->header.string,
			(int)this->message.headers[i]->value.length,
			this->message.headers[i]->value.string
			);
			*/
			//MEMORY::free_(this->message.headers[i]);
			free(this->message.headers[i]);
		}
		//MEMORY::free_(this->message.headers);
		free(this->message.headers);
		this->message.headers = 0;
	}
	//printf("\n\n");
	memset(&this->message, 0, sizeof(struct SIP_message));
	this->isparsed = 0;
	memset(this->pmessage, 0, MAX_SIP_MESSAGE_SIZE);
	this->sipsize = 0;
}

struct _headerlist *SIP::addheader(char *line) {
	struct _headerlist **hllist, *header;

	//SIPSTRING *headerorvalue = 0;
	//int maxlen = 0;
	if (!line) return 0;
	/*maxlen = (int)strlen(line);*/
	if ( /* This is the first header */ this->message.headercount == 0) {
		/*
		hllist = (struct _headerlist **)
			MEMORY::calloc_(sizeof(struct _headerlist *), 1, (char *)"signalling.cpp > SIP::addheader(): Create header list \"hllist\"");
		*/

		hllist = (struct _headerlist **)calloc(sizeof(struct _headerlist *), 1);


		//header = hllist[0] = (struct _headerlist *)MEMORY::calloc_(sizeof(struct _headerlist), 1, line);
		header = hllist[0] = (struct _headerlist *)calloc(sizeof(struct _headerlist), 1);
	} else {
		//hllist = (struct _headerlist **) MEMORY::realloc_(this->message.headers, sizeof(struct _headerlist *)  * (this->message.headercount + 1));
		hllist = (struct _headerlist **)realloc(this->message.headers, sizeof(struct _headerlist *)  * (this->message.headercount + 1));
		if ( /* We failed to allocate memory */ !hllist) {
			OutputDebugString("##### ERROR #####\nrealloc() failed in SIP::addheader()\n\n");
			return 0;
		}
		//header = hllist[this->message.headercount] = (struct _headerlist *)MEMORY::calloc_(sizeof(struct _headerlist), 1, line /*(char *)"signalling.cpp > SIP::addheader()  Not the first header added."*/);
		header = hllist[this->message.headercount] = (struct _headerlist *)calloc(sizeof(struct _headerlist), 1);
		memset(header, 0, sizeof(struct _headerlist));
	}

	this->message.headers = hllist;
	header->header.string =
		JSTRING::headername(line, '\n',
			&header->header.length);
	header->value.string = JSTRING::headervalue(line, '\n',
		&header->value.length);
	this->message.headercount++;
	return header;
}

void SIP::show_message(void) {
	printf("<------------------ SIP Message -------------------------------->\n");
	printf("%s\n", this->pmessage);
	printf("<--------------------------------------------------------------->\n\n");
}

char *SIP::getheaderval(const char *header, size_t *value_length) {
	size_t i = 0, reqheaderlen = 0;
	struct _headerlist *htemp = 0;
	if (!value_length) return 0;
	*value_length = 0;
	if (!header) return 0;
	reqheaderlen = strlen(header);
	for (; i < (size_t)this->message.headercount; i++) {
		htemp = this->message.headers[i];
		if (JSTRING::matches(htemp->header.string, (char *)header)) {
			if (htemp->header.length == reqheaderlen) {
				*value_length = htemp->value.length;
				return htemp->value.string;
			}
		}
	}
	return 0;
}



struct _headerlist *SIP::getheader(const char *header) {
	size_t i = 0, reqheaderlen = 0;
	struct _headerlist *htemp = 0;
	if (!header) return 0;
	reqheaderlen = strlen(header);
	for (; i < (size_t)this->message.headercount; i++) {
		htemp = this->message.headers[i];
		if (JSTRING::matches(htemp->header.string, (char *)header)) {
			if (htemp->header.length == reqheaderlen) {
				return htemp;
			}
		}
	}
	return 0;
}




int SIGNAL::getwholeSIPmessage(char *ptr) {
	int content_length = -1/*, ptrlen = 0*/;
	size_t linelength = 0, cl_length = 0, i = 0, message_length = 0;
	char *line = ptr, *contentptr = 0, *valstopptr = 0;
	char *sipmessageend = 0;
	//    char debug[500];
	if (!this->isSIP(ptr)) return 0;
	JSTRING sip(ptr);
	//    OutputDebugString("********* SIGNAL::getwholeSIPmessage() Analyzing the following ******************\n");
	//    OutputDebugString(ptr);
	//    OutputDebugString("*********************************************************************************\n");
	for (i = 1; i <= sip.linecount; i++) {
		line = sip.line((int)i, &linelength);
		//        sprintf(debug, "Checking Line: %.*s\n", (int)linelength, line);
		//        OutputDebugString(debug);
		if (linelength == 0) {
			// This is the blank line separating headers from content
			if ( /* There was no "Content-Length" header.
				 This is an incomplete message.
				 */ content_length == -1) {
				OutputDebugString("\n\nSIGNAL::getwholeSIPmessage() returns: 0 because there was no \"Content-Length\" found.\n\n");
				return 0;
			} else /* We have received a "Content-Length" of this message */ {
				sipmessageend = &line[content_length + (2 /* Plus the CRLF in this line */)];
				message_length = (sipmessageend - ptr);
				if (strlen(ptr) < message_length) {
					/* The content isn't finished downloading */
					OutputDebugString("\n\nSIGNAL::getwholeSIPmessage() returns: 0 because the complete SIP message isn't downloaded.\n\n");
					return 0;
				} else {
					//                    sprintf(debug, "\n\nSIGNAL::getwholeSIPmessage() returns: %lu --- Message to process is as follows:\n\n", message_length);
					//                    OutputDebugString(debug);
					//                    printf("\"%.*s\"", (int)message_length, ptr);
					//                    OutputDebugString("\n*********************************************************************************\n");
					return (int)(message_length);
				}
			}
		} else if (JSTRING::matches(line, (char *)"content-length")) {
			contentptr = JSTRING::headervalue(line, '\n', &cl_length);
			valstopptr = &contentptr[cl_length];
			content_length = (int)strtoull(contentptr, &valstopptr, 10);
		}
	}
	return 0;
}


int SIGNAL::r_matches(char *haystack, char *needle) {
	int i = 0;
	char *phaystack = 0;
	size_t needlelen = 0;
	if (!haystack || !needle) return 0;
	phaystack = haystack;
	needlelen = strlen(needle);
	for (i = (int)(needlelen - 1); i >= 0; i--) {
		while (
			phaystack[0] == '\r' ||
			phaystack[0] == '\n' ||
			phaystack[0] == ' ' ||
			phaystack[0] == '\t') {
			phaystack--;
		}
		if (phaystack[0] != needle[i]) return 0;
		phaystack--;
	}
	return 1;
}


int SIP::isSIPrequest(char *ptr) {
	int retval = 0, j = 0;
	char *lineend = 0;
	if (!ptr) return 0;
	if (strlen(ptr) < 3) return 0;
	for (j = 0; j < sizeof(SIP::sip_request_list) / sizeof(char *); j++) {
		if (JSTRING::matches(ptr, (char *)SIP::sip_request_list[j])) {
			lineend = strstr(ptr, "\n");
			if (JSTRING::reverse_matches(lineend, (char *)"SIP/2.0")) {
				return 1;
			}
		}
	}
	return retval;
}

int SIP::isSIPrequest() {
	return SIP::isSIPrequest(this->pmessage);
}

int SIP::isSIPresponse(char *ptr) {
	int statuscode = 0;
	char *lineend = 0, *statuscodeend = 0, *statuscodeptr = 0;
	if (!ptr) return 0;
	lineend = strstr(ptr, "\n");
	if (!lineend) return 0; /* There's no \r\n */
	if (lineend - ptr > 400) return 0; /* The line-ending is > 400 bytes away, too long */
	if (strncmp(ptr, "SIP/2.0 ", 8)) return 0; /* The string doesn't start with SIP identifier */
	statuscodeptr = &ptr[8];
	if (ptr[11] != ' ') return 0; /* The status code is not three digits */
	statuscodeend = &ptr[11];
	statuscode = (int)strtoull(statuscodeptr, &statuscodeend, 10);
	return statuscode;
}


int SIP::isSIPresponse() {
	return SIP::isSIPresponse(this->pmessage);
}




size_t SIGNAL::make_hold_sdp(DIALOG *dialog, char *buffer) {
    if (!dialog || !buffer) return 0;
    if (dialog->dialog_info.mine.session_identifier == 0) {
        dialog->dialog_info.mine.session_identifier = (unsigned long)(timestamp() + (rand() % 1000000));
        dialog->dialog_info.mine.ssrc = dialog->dialog_info.mine.media_version = dialog->dialog_info.mine.session_identifier;
    } else {
        dialog->dialog_info.mine.media_version++;
    }
//    dialog->callinfo.audio_stream_id = dialog->dialog_info.mine.session_identifier;
//    dialog->dialog_info.mine.ssrc = dialog->callinfo.audio_stream_id;
    buffer[0] = 0;
    sprintf(buffer, "\
v=0\r\n\
o=- %lu %lu IN IP4 %s\r\n\
s=deedlecore\r\n\
c=IN IP4 %s\r\n\
t=0 0\r\n\
m=audio 8282 RTP/AVP 0 96\r\n\
a=X-nat:0\r\n\
a=direction:active\r\n\
a=rtpmap:0 PCMU/8000\r\n\
a=rtpmap:96 telephone-event/8000\r\n\
a=fmtp:96 0-16\r\n\
a=rtcp-mux\r\n\
a=inactive\r\n",
            dialog->dialog_info.mine.session_identifier, dialog->dialog_info.mine.media_version, this->user_info.ipaddress, /* o */
            this->user_info.ipaddress);
    return strlen(buffer);
}






size_t SIGNAL::generate_sdp(DIALOG *dialog, char *buffer, char *audio_state) {
	const char def_audio_state[] = "sendrecv";
	if (!dialog || !buffer) return 0;
	if (!audio_state) {
		audio_state = (char *)def_audio_state;
	}
//    dialog->callinfo.audio_stream_id = (unsigned long)(timestamp() + (rand() % 1000000));
//    dialog->dialog_info.mine.ssrc = dialog->callinfo.audio_stream_id;
//    dialog->dialog_info.mine.media_version++;
//    dialog->callinfo.audio_stream_id+=dialog->dialog_info.mine.media_version;
    if (dialog->dialog_info.mine.session_identifier == 0) {
        dialog->dialog_info.mine.session_identifier = (unsigned long)(timestamp() + (rand() % 1000000));
        dialog->dialog_info.mine.ssrc = dialog->dialog_info.mine.media_version = dialog->dialog_info.mine.session_identifier;
    } else {
        dialog->dialog_info.mine.media_version++;
    }

    
    
	buffer[0] = 0;
	sprintf(buffer, "\
v=0\r\n\
o=- %lu %lu IN IP4 %s\r\n\
s=deedlecore\r\n\
c=IN IP4 %s\r\n\
t=0 0\r\n\
m=audio 8282 RTP/AVP 0 96\r\n\
a=rtpmap:0 PCMU/8000\r\n\
a=rtpmap:96 telephone-event/8000\r\n\
a=fmtp:96 0-16\r\n\
a=ptime:20\r\n\
a=rtcp-mux\r\n\
a=%s\r\n",
/*dialog->callinfo.audio_stream_id*/dialog->dialog_info.mine.session_identifier,
            dialog->dialog_info.mine.media_version/*dialog->callinfo.audio_stream_id*/,
            this->user_info.ipaddress, /* o */
this->user_info.ipaddress, /* c */
audio_state);
	return strlen(buffer);
}


AUDIO_STREAM_ID SIGNAL::generate_sdp(char *buffer, char *audio_state) {
	AUDIO_STREAM_ID retval = (unsigned long)(timestamp() + (rand() % 1000000));
	const char def_audio_state[] = "sendrecv";
	if (!buffer) return 0;
	if (!audio_state) {
		audio_state = (char *)def_audio_state;
	}
	buffer[0] = 0;
	sprintf(buffer, "\
v=0\r\n\
o=- %lu %lu IN IP4 %s\r\n\
s=deedlecore\r\n\
c=IN IP4 %s\r\n\
t=0 0\r\n\
m=audio 8282 RTP/AVP 0 96\r\n\
a=rtpmap:0 PCMU/8000\r\n\
a=rtpmap:96 telephone-event/8000\r\n\
a=fmtp:96 0-16\r\n\
a=ptime:20\r\n\
a=rtcp-mux\r\n\
a=%s\r\n",
retval, retval, this->user_info.ipaddress, /* o */
this->user_info.ipaddress, /* c */
audio_state);
	return retval;
}




void SIGNAL::digit_convert(char *digit_string) {
	int i = 0;
	if (!digit_string) return;
	for (; i < (int)strlen(digit_string); i++) {
		if (LOWER(digit_string[i]) >= 'a' && LOWER(digit_string[i]) <= 'c') {
			digit_string[i] = '2';
		} else if (LOWER(digit_string[i]) >= 'd' && LOWER(digit_string[i]) <= 'f') {
			digit_string[i] = '3';
		} else if (LOWER(digit_string[i]) >= 'g' && LOWER(digit_string[i]) <= 'i') {
			digit_string[i] = '4';
		} else if (LOWER(digit_string[i]) >= 'j' && LOWER(digit_string[i]) <= 'l') {
			digit_string[i] = '5';
		} else if (LOWER(digit_string[i]) >= 'm' && LOWER(digit_string[i]) <= 'o') {
			digit_string[i] = '6';
		} else if (LOWER(digit_string[i]) >= 'p' && LOWER(digit_string[i]) <= 's') {
			digit_string[i] = '7';
		} else if (LOWER(digit_string[i]) >= 't' && LOWER(digit_string[i]) <= 'v') {
			digit_string[i] = '8';
		} else if (LOWER(digit_string[i]) >= 'w' && LOWER(digit_string[i]) <= 'z') {
			digit_string[i] = '9';
		}
	}
}

void SIGNAL::digit(char d) {
	int i = 0;
	char message[2048], content[200];
	char my_address[100];
	char server_ip[100];
	DIALOG *dialog = 0;

	if (d < '0' || d > '9') {
		if (LOWER(d) >= 'a' && LOWER(d) <= 'c') {
			d = '2';
		} else if (LOWER(d) >= 'd' && LOWER(d) <= 'f') {
			d = '3';
		} else if (LOWER(d) >= 'g' && LOWER(d) <= 'i') {
			d = '4';
		} else if (LOWER(d) >= 'j' && LOWER(d) <= 'l') {
			d = '5';
		} else if (LOWER(d) >= 'm' && LOWER(d) <= 'o') {
			d = '6';
		} else if (LOWER(d) >= 'p' && LOWER(d) <= 's') {
			d = '7';
		} else if (LOWER(d) >= 't' && LOWER(d) <= 'v') {
			d = '8';
		} else if (LOWER(d) >= 'w' && LOWER(d) <= 'z') {
			d = '9';
		} else if ( d != '*' && d != '#') {
			printf("** DTMF error: Invalid character to convert to a DTMF digit.\n");
			return;
		}
	}
	sprintf(content, "Signal=%c\r\nDuration=160\r\n", d);

	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;

	for (i = 0; i < MAX_DIALOG_COUNT; i++) {
		dialog = &this->dialog_list[i];
		if (!dialog->inuse || !dialog->isinvite) continue;
		if (dialog->callinfo.status != TALKING) continue;
		sprintf(message, "\
INFO sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: %d INFO\r\n\
User-Agent: Deedle Core\r\n\
Content-Type: application/dtmf-relay\r\n\
Content-Length: %d\r\n\r\n%s",
dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress,                                 /* Method Line */
my_address, dialog->dialog_info.branch,                                                                     /* Via */
dialog->dialog_info.mine.extension, dialog->dialog_info.mine.ipaddress, dialog->dialog_info.mine.tag,       /* From */
dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress, dialog->dialog_info.theirs.tag, /* To */
this->user_info.extension, my_address,                                                                      /* Contact */
dialog->dialog_info.callid,                                                                                 /* Call-ID */
++dialog->dialog_info.mine.cseq,                                                                            /* CSeq */
(int)strlen(content),                                                                                       /* Content-Length */
content);
		if (this->psocket) {
			showsip(message);
            this->queue(message, (int)strlen(message));
			//db_send(this->psocket->s, message, strlen(message), 0);
		}
	}

	return;
}


void SIGNAL::dial_digits(char *digits, size_t len) {
	int i = 0;
	char dtd[100];
	if (len == 0) return;
	memset(dtd, 0, 100);
	sprintf(dtd, "%.*s", ((len<99) ? (int)len : 99), digits);
	SIGNAL::digit_convert(dtd);
	for (i = 0; i < (int)strlen(dtd); i++) {
		this->digit(dtd[i]);
		Sleep(200);
	}
}

void SIGNAL::dial_digits(char *digits) {
	if (digits) this->dial_digits(digits, strlen(digits));
}


void SIGNAL::subscribe(unsigned long extension) {
	char message[2048], mytag[500], newbranch[500], newcallid[500];
	char my_address[100];
	char server_ip[100];
	SIP *sipmessage = 0;
	DIALOG *d = 0;
	this->newbranch(newbranch);
	this->newcallid(newcallid);
	this->newtag(mytag);

	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;
	sprintf(message, "\
SUBSCRIBE sip:%ld@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
To: <sip:%d@%s>\r\n\
From: <sip:%ld@%s>;tag=%s\r\n\
Call-ID: %s\r\n\
CSeq: 101 SUBSCRIBE\r\n\
User-Agent: Deedle Core\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Event: message-summary\r\n\
Accept: application/simple-message-summary\r\n\
Expires: 300\r\n\
Content-Length: 0\r\n\r\n",
extension, server_ip,                           /* Method Line */
my_address, newbranch,                          /* Via */
/*extension*/ this->user_info.extension, server_ip,                           /* To */
extension /*this->user_info.extension*/, my_address, mytag,   /* From */
newcallid,                                      /* Call-ID */
this->user_info.extension, my_address           /* Contact */);

	//    printf("<--------------- SENDING -------------------------------------------------->\n");
	//    printf("%s\n", message);
	//    printf("<-------------------------------------------------------------------------->\n\n");
	sipmessage = new SIP(message);
	//MEMORY::add_pointer_to_track(sipmessage, (char *)"signalling.cpp > SIGNAL::subscribe(): sipmessage = new SIP(message);");
	if (!sipmessage) {
		printf("*** Failed to parse SIP message ***\n");
		return;
	}

	printf("SIP Message: %.*s expires in %d seconds.\n",
		(int)sipmessage->message.request.length,
		sipmessage->message.request.string,
		sipmessage->message.expires);

	d = add_dialog(sipmessage);
    printf("My CSeq is: %d\n", d->dialog_info.mine.cseq);
	if (!d) {
		printf("** Failed to create dialog for ");
		printf("SIP Message: %.*s expires in %d seconds.\n",
			(int)sipmessage->message.request.length,
			sipmessage->message.request.string,
			sipmessage->message.expires);
		//MEMORY::deleted(sipmessage);
		delete sipmessage;
	}
	d->destroy_after = time(0) + 300;
	if (this->psocket) {
		showsip(message);
		db_send(this->psocket->s, message, strlen(message), 0);
	}
	return;
}

const char *SIGNAL::response_to_text( int sip_response ) {
    switch (sip_response) {
        case 100:
            return "Trying";
            break;
        case 180:
            return "Ringing";
        case 181:
            return "Call is being forwarded";
        case 182:
            return "Queued";
        case 183:
            return "Session Progress";
        case 199:
            return "Early Dialog Terminated";
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 204:
            return "No Notification";
        case 300:
            return "Multiple Choices";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Moved Temporarily";
        case 305:
            return "Use Proxy";
        case 380:
            return "Alternate Service";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 402:
            return "Payment Required";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 406:
            return "Not Acceptable";
        case 407:
            return "Proxy Authentications Required";
        case 408:
            return "Request Timeout";
        case 409:
            return "Conflict";
        case 410:
            return "Gone";
        case 411:
            return "Length Required";
        case 412:
            return "Conditional Request Failed";
        case 413:
            return "Request Entity Too Large";
        case 414:
            return "Request URI too long";
        case 415:
            return "Unsupported media type";
        case 416:
            return "Unsupported URI Scheme";
        case 417:
            return "Unknown Resource-Priority";
        case 420:
            return "Bad Extension";
        case 421:
            return "Extension Required";
        case 422:
            return "Session Interval Too Small";
        case 423:
            return "Interval Too Brief";
        case 424:
            return "Bad Locationa Information";
        case 428:
            return "Use Identity Header";
        case 429:
            return "Provide Referrer Identity";
        case 430:
            return "Flow Failed";
        case 433:
            return "Anonymity Disallowed";
        case 436:
            return "Bad Identity-Info";
        case 437:
            return "Unsupported Certificate";
        case 438:
            return "Invalid Identity Header";
        case 439:
            return "First Hop Lacks Outbound Support";
        case 440:
            return "Max-Breadth Exceeded";
        case 469:
            return "Bad Info Package";
        case 470:
            return "Consent Needed";
        case 480:
            return "Temporarily Unavailable";
        case 481:
            return "Call Leg/Transaction Does Not Exist";
        case 482:
            return "Loop Detected";
        case 483:
            return "Too Many Hops";
        case 484:
            return "Address Incomplete";
        case 485:
            return "Ambiguous";
        case 486:
            return "Busy Here";
        case 487:
            return "Request Terminated";
        case 488:
            return "Not Acceptable Here";
        case 489:
            return "Bad Event";
        case 491:
            return "Request Pending";
        case 493:
            return "Undecipherable";
        case 494:
            return "Security Agreement Required";
        case 500:
            return "Server Internal Error";
        case 501:
            return "Not Implemented";
        case 503:
            return "Service Unavailable";
        case 504:
            return "Server Time-out";
        case 505:
            return "Version Not Supported";
        case 513:
            return "Message Too Large";
        case 580:
            return "Precondition Failure";
        case 600:
            return "Busy Everywhere";
        case 603:
            return "Decline";
        case 604:
            return "Does Not Exist Anywhere";
        case 606:
            return "Not Acceptable";
        case 607:
            return "Unwanted";
        default:
            return "Response";
    }
}


void SIGNAL::response( SIP *message, int sip_response ) {
    DIALOG *dialog = 0;
    char sip_text[3000];
    char via[1000];
    char from[1000];
    char to[1000];
    char contact[1000];
    char callid[500];
    char cseq[100];
    char fromtag[500];
    char totag[500];
    char branch[500];
    char my_address[100];
    char server_ip[100];
    
    if (!message) return;
    dialog = get_dialog(message->message.callid_header->value.string, message->message.callid_header->value.length);
    
    if (!this->IPandport_deedle(my_address)) return;
    if (!this->IPandport_server(server_ip)) return;

    sprintf(sip_text, "SIP/2.0 %d ", sip_response);
    strcat(sip_text, this->response_to_text(sip_response));
    strcat(sip_text, "\r\n");
    
    if (message->message.via_header->sip_headerinfo.sip_via.branch.length > 0) {
        sprintf(branch, "%.*s", (int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
                message->message.via_header->sip_headerinfo.sip_via.branch.string);
    } else {
        if (dialog) {
            strcpy(branch, dialog->dialog_info.branch);
        }
    }
    contact[0] = 0;
    if (message->message.contact_header != 0) {
        if (message->message.contact_header->value.length > 0) {
            sprintf(contact, "Contact: %.*s\r\n", (int)message->message.contact_header->value.length, message->message.contact_header->value.string);
        }
    }
    if (dialog) {
        sprintf(from, "From: <sip:%s@%s>;tag=%s\r\n", dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress, dialog->dialog_info.theirs.tag);
        sprintf(to, "To: <sip:%s@%s>;tag=%s\r\n", dialog->dialog_info.mine.extension, dialog->dialog_info.mine.ipaddress, dialog->dialog_info.mine.tag);
        strcpy(callid, dialog->dialog_info.callid);
    } else {
        if ( /* There's a FROM tag */ message->message.from_header->sip_headerinfo.uri->_tag.length > 0) {
            sprintf(fromtag, "%.*s", (int)message->message.from_header->sip_headerinfo.uri->_tag.length,
                    message->message.from_header->sip_headerinfo.uri->_tag.string);
        } else {
            this->newtag(fromtag);
        }
        
        if ( /* There's a TO tag */ message->message.to_header->sip_headerinfo.uri->_tag.length > 0) {
            sprintf(totag, "%.*s", (int)message->message.to_header->sip_headerinfo.uri->_tag.length,
                    message->message.to_header->sip_headerinfo.uri->_tag.string);
        } else {
            this->newtag(totag);
        }
        sprintf(from, "From: <sip:%.*s@%.*s>;tag=%s\r\n",
                (int)message->message.from_header->sip_headerinfo.uri->_extension.length,
                message->message.from_header->sip_headerinfo.uri->_extension.string,
                (int)message->message.from_header->sip_headerinfo.uri->_ipaddress.length,
                message->message.from_header->sip_headerinfo.uri->_ipaddress.string,
                fromtag);
        sprintf(to, "To: <sip:%.*s@%.*s>;tag=%s\r\n",
                (int)message->message.to_header->sip_headerinfo.uri->_extension.length,
                message->message.to_header->sip_headerinfo.uri->_extension.string,
                (int)message->message.to_header->sip_headerinfo.uri->_ipaddress.length,
                message->message.to_header->sip_headerinfo.uri->_ipaddress.string,
                fromtag);
        sprintf(callid, "%.*s", (int)message->message.callid_header->value.length,
                message->message.callid_header->value.string);
    }
    sprintf(via, "Via: SIP/2.0/TCP %s;rport=%u;received=%s;branch=%s\r\n", my_address, this->signal_port, this->voip_server_ip, branch);
    strcat(sip_text, via);
    strcat(sip_text, from);
    strcat(sip_text, to);
    strcat(sip_text, "Call-ID: ");
    strcat(sip_text, callid);
    strcat(sip_text, "\r\n");
    sprintf(cseq, "CSeq: %d %.*s\r\n", message->message.csq_header->sip_headerinfo.sip_cseq.sequence,
            (int)message->message.request.length, message->message.request.string);
    strcat(sip_text, cseq);
    strcat(sip_text, contact);
    strcat(sip_text, "Content-Length: 0\r\n\r\n");
    if (this->psocket) {
        showsip(sip_text);
        db_send(this->psocket->s, sip_text, strlen(sip_text), 0);
    }
    return;
}

void SIGNAL::cancel(DIALOG *dialog) {
	char message[2048];
	char my_address[100];
	char server_ip[100];
	SIP *sipmessage = 0;
	if (!dialog) return;
	sipmessage = dialog->primary;

	/*
	A CANCEL is sent by whoever sent the INVITE and
	has all the same information as the INVITE


	CANCEL sip:317@172.56.7.118:43082;transport=tcp SIP/2.0
	Via: SIP/2.0/TCP 107.182.238.227:5060;rport;branch=z9hG4bKPjc2c59d25-551e-4f0c-9aa9-0e089f816d9d;alias
	From: "+19405777489" <sip:+19405777489@107.182.238.227>;tag=25105450-2172-4f81-a8eb-8fcf68dd427d
	To: <sip:317@172.56.7.118>
	Call-ID: 4b00d2ab-15c7-4bf8-bc37-7bfe376df677
	CSeq: 12602 CANCEL
	Max-Forwards: 70
	User-Agent: Asterisk PBX 15.4.0
	Content-Length:  0
	*/

	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;
	sprintf(message, "\
CANCEL sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
To: <sip:%s@%s>%s%s\r\n\
CSeq: %d CANCEL\r\n\
Call-ID: %s\r\n\
Content-Length: 0\r\n\r\n",
dialog->callinfo.talking_to_number, server_ip, /* CANCEL */
my_address, dialog->dialog_info.branch, /* Via */
dialog->dialog_info.mine.extension, my_address, dialog->dialog_info.mine.tag, /* From */
dialog->dialog_info.theirs.extension, server_ip, ((strlen(dialog->dialog_info.theirs.tag) > 0)?";tag=":""), ((strlen(dialog->dialog_info.theirs.tag) > 0)?dialog->dialog_info.theirs.tag:""),/* To */
++dialog->dialog_info.mine.cseq,
dialog->dialog_info.callid); /* Call-ID */

	if (this->psocket) {
		showsip(message);
		db_send(this->psocket->s, message, strlen(message), 0);
	}
	return;
}

void SIGNAL::bye(DIALOG *dialog) {
	char message[2048];
	char my_address[100];
	char server_ip[100];
	char newbranch[500];
	if (!dialog) return;
	this->newbranch(newbranch);


	/*
	BYE sip:+14258026626@173.192.209.79:5060 SIP/2.0
	Via: SIP/2.0/TCP 68.201.122.149:49250;branch=z9hG4bKdb6f83633c754cfd1d
	Max-Forwards: 70
	From: <sip:316@68.201.122.149:49250;transport=tcp>;tag=46f336a772ff18641f7c295c6
	To: "SEA-WVP" <sip:+14258026626@173.192.209.79>;tag=as11169d60
	Contact: <sip:316@68.201.122.149:49250;transport=tcp>
	Call-ID: 485f29904dc2167f34f82ad50381a443@173.192.209.79:5060
	CSeq: 104 BYE
	User-Agent: deedlebox/1.0
	Content-Length: 0
	*/


	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;
	sprintf(message, "\
BYE sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;branch=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: %d BYE\r\n\
User-Agent: Deedle Core\r\n\
Content-Length: 0\r\n\r\n",
dialog->dialog_info.theirs.extension, server_ip,
my_address, newbranch, /* Via */
dialog->dialog_info.mine.extension, dialog->dialog_info.mine.ipaddress, dialog->dialog_info.mine.tag, /* From */
dialog->dialog_info.theirs.extension, dialog->dialog_info.mine.ipaddress, dialog->dialog_info.theirs.tag, /* To */
this->user_info.extension, my_address, /* Contact */
dialog->dialog_info.callid,
++dialog->dialog_info.mine.cseq);
	if (this->psocket) {
		showsip(message);
		db_send(this->psocket->s, message, strlen(message), 0);
	}
	return;
}


void SIGNAL::hangup(char *szcallid) {
	DIALOG *dialog = 0;
	if (!szcallid) return;
	dialog = this->get_dialog(szcallid);
	if (!dialog) return;

	switch (dialog->callinfo.status) {
		case RINGING_IN:
			/* Send 486 Busy Here */
			//dialog->callinfo.status = CANCELLING_CALL;
			this->dialog_change_status(dialog, CANCELLING_CALL);
//            printf("Testing change!\n");
//            this->busy(dialog);
            this->response(dialog->primary, 486);
			break;
		case RINGING_OUT:
			/* Send CANCEL here */
            if (strlen(dialog->dialog_info.theirs.tag) == 0) {
                this->dialog_change_status(dialog, CANCELLING_CALL);
                this->cancel(dialog);
            } else {
                this->dialog_change_status(dialog, HANGING_UP);
                this->bye(dialog);
            }
			break;
		case TRANSFERRING:
		case TALKING:
			/* Send BYE here */
			if (this->rtp) {
				this->rtp->stop_audio(szcallid);
			}
			this->dialog_change_status(dialog, HANGING_UP);
			this->bye(dialog);
			break;
		case ANSWERING: /* 200 OK has been sent, but no ACK has come in yet. */
			break;
        case PUTTING_ONHOLD:
		case ONHOLD:
			/* Send BYE */
            if (this->rtp) {
                this->rtp->stop_audio(szcallid);
            }
            this->dialog_change_status(dialog, HANGING_UP);
            this->bye(dialog);
			break;
		case CANCELLING_CALL:
			/* Do nothing, we're already trying to dispose of this DIALOG/session */
			break;
		default:
			break;

	}
	return;
}

/* This should only allow calls with a status of ONHOLD */
void SIGNAL::unhold( char *callid ) {
    char sip_invite[3000];
    char generated_sdp[1000];
    char my_address[100];
    char server_ip[100];
    DIALOG *dialog = 0;
    if (!this->isregistered()) return;
    if (!this->IPandport_deedle(my_address)) return;
    if (!this->IPandport_server(server_ip)) return;
    dialog = this->get_dialog(callid);
    if (!dialog) return;
//    this->generate_sdp(generated_sdp, (char *)"sendrecv");
    this->generate_sdp(dialog, generated_sdp, (char *)"sendrecv");
    if(dialog->callinfo.status != ONHOLD) {
        printf("**** THIS DIALOG IS NOT ON HOLD *****\n");
        return;
    }
    
    
    sprintf(sip_invite, "\
INVITE sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;rport;branch=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: %d INVITE\r\n\
User-Agent: Deedle Core\r\n\
Content-Type: application/sdp\r\n\
Content-Length: %lu\r\n\r\n%s",
dialog->dialog_info.theirs.extension, server_ip, /* INVITE */
my_address, dialog->dialog_info.branch, /* Via */
dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress, dialog->dialog_info.theirs.tag, /* To */
dialog->dialog_info.mine.extension, my_address, dialog->dialog_info.mine.tag, /* From */
this->user_info.extension, my_address, /* Contact */
callid, /* Call-ID */
++dialog->dialog_info.mine.cseq, /* CSeq */
(unsigned long)strlen(generated_sdp),
generated_sdp);
    
    
    //printf("%s\n", sip_invite);
    showsip(sip_invite);
    db_send(this->psocket->s, sip_invite, strlen(sip_invite), 0);

    return;
}

/* This should only allow calls with a status of TALKING */
void SIGNAL::hold( char *callid ) {
    char sip_invite[3000];
    char generated_sdp[1000];
    char my_address[100];
    char server_ip[100];
    DIALOG *dialog = 0;
    if (!this->isregistered()) return;
    if (!this->IPandport_deedle(my_address)) return;
    if (!this->IPandport_server(server_ip)) return;
    dialog = this->get_dialog(callid);
    if (!dialog) return;
//    this->generate_sdp(generated_sdp, (char *)"inactive");
    this->make_hold_sdp(dialog, generated_sdp);
    if(dialog->callinfo.status != TALKING) return;
    
    
    sprintf(sip_invite, "\
INVITE sip:%s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;rport;branch=%s\r\n\
To: <sip:%s@%s>;tag=%s\r\n\
From: <sip:%s@%s>;tag=%s\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n\
CSeq: %d INVITE\r\n\
User-Agent: Deedle Core\r\n\
Content-Type: application/sdp\r\n\
Content-Length: %lu\r\n\r\n%s",
dialog->dialog_info.theirs.extension, server_ip, /* INVITE */
my_address, dialog->dialog_info.branch, /* Via */
dialog->dialog_info.theirs.extension, dialog->dialog_info.theirs.ipaddress, dialog->dialog_info.theirs.tag, /* To */
dialog->dialog_info.mine.extension, my_address, dialog->dialog_info.mine.tag, /* From */
this->user_info.extension, my_address, /* Contact */
callid, /* Call-ID */
++dialog->dialog_info.mine.cseq, /* CSeq */
(unsigned long)strlen(generated_sdp),
generated_sdp);

    
    
    
    //printf("%s\n", sip_invite);
    showsip(sip_invite);
    db_send(this->psocket->s, sip_invite, strlen(sip_invite), 0);

    this->dialog_change_status(dialog, PUTTING_ONHOLD);
    return;
}

void SIGNAL::call(char *callto, size_t len) {
	char invite[2048], generated_sdp[1000];
	char my_address[100];
	char server_ip[100];
	char my_tag[200], my_branch[200];
	char my_callid[500];
	char p_asserted_identity[300];
    char xintercom[100];
	SIP *sipmessage = 0;
	DIALOG *dialog = 0;
	AUDIO_STREAM_ID ssrc = 0;

	if (!callto) return;
	if (!this->isregistered()) return;
	if (!this->IPandport_deedle(my_address)) return;
	if (!this->IPandport_server(server_ip)) return;
	if (this->rtp) this->rtp->prime_audio(); /* Prime audio */

    memset(p_asserted_identity, 0, 300);
    memset(xintercom, 0, 100);
    if (len >= 10 ) {
        if (strlen(this->user_info.callerid) > 0) {
            sprintf(p_asserted_identity, "P-Asserted-Identity: <sip:%s@%s>\r\nRemote-Party-ID: \"Deedle User\" <sip:%s@%s>\r\n", this->user_info.callerid, my_address, this->user_info.callerid, my_address);
        }
    } else {
        if (callto[0] >= '1' && callto[0] <= '9') {
            sprintf(xintercom, "X-Intercom: 1\r\n");
        }
    }
        
	ssrc = this->generate_sdp(generated_sdp, (char *)"sendrecv");
	this->newbranch(my_branch);
	this->newtag(my_tag);
	sprintf(my_callid, "%lu%.*s%d@%s", (unsigned long)time(0), (int)len, callto, (int)(rand() % 1000000), my_address);
	sprintf(invite, "\
INVITE sip:%.*s@%s SIP/2.0\r\n\
Via: SIP/2.0/TCP %s;rport;branch=%s\r\n\
To: <sip:%.*s@%s>\r\n\
From: <sip:%d@%s>;tag=%s\r\n\
Contact: <sip:%d@%s;transport=tcp>\r\n\
Call-ID: %s\r\n%s%s\
CSeq: 101 INVITE\r\n\
Allow: OPTIONS, NOTIFY, INVITE, ACK, BYE, CANCEL, MESSAGE, REFER\r\n\
Max-Forwards: 70\r\n\
User-Agent: Deedle Core\r\n\
Content-Type: application/sdp\r\n\
Content-Length: %lu\r\n\r\n%s",
(int)len, callto, server_ip, /* INVITE */
my_address, my_branch, /* Via */
(int)len, callto, server_ip, /* To */
this->user_info.extension, my_address, my_tag, /* From */
this->user_info.extension, my_address, /* Contact */
my_callid, /* Call-ID */
p_asserted_identity, xintercom,
(unsigned long)strlen(generated_sdp),
generated_sdp);

	sipmessage = new SIP(invite);
    
    // Here, on new INVITE
    
	//MEMORY::add_pointer_to_track(sipmessage, (char *)"signalling.cpp > SIGNAL::call(): sipmessage = new SIP(invite);");
	if (sipmessage) {
		if (this->add_dialog(sipmessage)) {
			dialog = this->get_dialog(my_callid);
		}
	}

	if (dialog) {
		//dialog->callinfo.status = CONFIRMING;
		dialog->callinfo.audio_stream_id = ssrc;
		dialog->dialog_info.mine.ssrc = ssrc;
        dialog->dialog_info.mine.media_version = ssrc;
        dialog->dialog_info.mine.session_identifier = ssrc;

		if (this->psocket) {
			showsip(invite);
			db_send(this->psocket->s, invite, strlen(invite), 0);
		}
	} else {
		printf("** failed to create dialog for new invite!!!!\n");
	}
	return;
}

void SIGNAL::call(char *callto) {
	if (!callto) return;
	this->call(callto, strlen(callto));
	return;
}


void SIGNAL::answer(char *szcallid) {
	char szanswer[2048], generated_sdp[1000];
	SIP *message = 0;
	size_t sdplen = 0;
	DIALOG *dialog = 0;
	dialog = this->get_dialog(szcallid);
	if (!dialog) return;
	message = dialog->primary;
	sdplen = this->generate_sdp(dialog, generated_sdp, 0);
	if (!sdplen) return;


	sprintf(szanswer, "\
SIP/2.0 200 Ok\r\n\
Via: %.*s %.*s:%u;rport=%u;received=%s;branch=%.*s\r\n\
To: %.*s;tag=%s\r\n\
From: %.*s\r\n\
Call-ID: %.*s\r\n\
CSeq: %d %.*s\r\n\
Contact: \"%s\" <sip:%d@%s:%u>\r\n\
Content-Type: application/sdp\r\n\
Content-Length: %lu\r\n\r\n",
(int)message->message.via_header->sip_headerinfo.sip_via.version.length, // VIA SIP/2.0/TCP
message->message.via_header->sip_headerinfo.sip_via.version.string,  // VIA SIP/2.0/TCP
(int)message->message.via_header->sip_headerinfo.sip_via.ipaddress.length, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.ipaddress.string, // VIA IP Address
message->message.via_header->sip_headerinfo.sip_via.usrport, // VIA Port
this->signal_port,
this->voip_server_ip,
(int)message->message.via_header->sip_headerinfo.sip_via.branch.length,
message->message.via_header->sip_headerinfo.sip_via.branch.string,
(int)message->message.to_header->value.length, // To
message->message.to_header->value.string, // To
dialog->dialog_info.mine.tag, // To (tag)
(int)message->message.from_header->value.length, // From
message->message.from_header->value.string, // From
(int)message->message.callid_header->value.length, // Call-ID
message->message.callid_header->value.string, // Call-ID
dialog->dialog_info.theirs.cseq, // CSeq
(int)message->message.csq_header->sip_headerinfo.sip_cseq.method.length, // CSeq Method
message->message.csq_header->sip_headerinfo.sip_cseq.method.string, // CSeq Method
((strlen(this->user_info.name) > 0) ? this->user_info.name : ""),
this->user_info.extension,
this->user_info.ipaddress,
this->user_info.rport,
(unsigned long)sdplen);
	strcat(szanswer, generated_sdp);
	/*
	printf("**** Answer Message ****\n");
	printf("%s", szanswer);
	printf("\n\n");
	*/
	this->dialog_change_status(dialog, ANSWERING);
	if (this->rtp) {
		if (this->rtp->start_audio(message->message.content.sdp.audio_address,
			message->message.content.sdp.audio_port,
			dialog->dialog_info.callid,
			dialog->callinfo.audio_stream_id) > 0) {
			dialog->audio_active = 1;
		}
	}
	if (this->psocket) {
		showsip(szanswer);
		db_send(this->psocket->s, szanswer, strlen(szanswer), 0);
	}
	return;
}


/* Delete this */
int SIGNAL::isSIPrequest(char *ptr) {
	int retval = 0, j = 0;
	char *lineend = 0;
	if (!ptr) return 0;
	if (strlen(ptr) < 3) return 0;
	for (j = 0; j < sizeof(SIGNAL::sip_request_list) / sizeof(char *); j++) {
		if (this->matches(ptr, (char *)SIGNAL::sip_request_list[j])) {
			lineend = strstr(ptr, "\n");
			if (this->r_matches(lineend, (char *)"SIP/2.0")) {
				return 1;
			}
		}

	}
	return retval;
}


/*
If the string passed is the beginning of a SIP response,
the statuscode is returned.  Otherwise 0 is returned.
*/
int SIGNAL::isSIPresponse(char *ptr) {
	int statuscode = 0;
	char *lineend = 0, *statuscodeend = 0, *statuscodeptr = 0;
	if (!ptr) return 0;
	lineend = strstr(ptr, "\n");
	if (!lineend) return 0; /* There's no \r\n */
	if (lineend - ptr > 400) return 0; /* The line-ending is > 400 bytes away, too long */
	if (strncmp(ptr, "SIP/2.0 ", 8)) return 0; /* The string doesn't start with SIP identifier */
	statuscodeptr = &ptr[8];
	if (ptr[11] != ' ') return 0; /* The status code is not three digits */
	statuscodeend = &ptr[11];
	statuscode = (int)strtoull(statuscodeptr, &statuscodeend, 10);
	return statuscode;
}


char *SIGNAL::getnextSIP(char *ptr) {
	int i = 0, ptrlen = 0;
	if (!ptr) return 0;
	ptrlen = (int)strlen(ptr);
	for (; i < ptrlen; i++) {
		if (SIP::issip(&ptr[i])) return &ptr[i];
	}
	return 0;
}


time_t SIGNAL::timetoreregister(void) {
	if (this->shuttingdown) {
		return (time(0) + 5000);
	}
	if (this->registration) {
		return ((this->registration->stats.created - 20) + this->registration->message.expires);
	}
	return (time(0) + 5000);
}

time_t SIGNAL::expires(void) {
	time_t exp = time(0);
	if (this->registration) {
		return (this->registration->stats.created + this->registration->message.expires);
	}
	return exp;
}

DIALOG *SIGNAL::get_dialog(char *callid, size_t len) {
	size_t i = 0;
	db_lock_mutex(&this->dialog_mutex);
	for (; i < MAX_DIALOG_COUNT; i++) {
		if (!strncmp(this->dialog_list[i].dialog_info.callid, callid, len)) {
			db_unlock_mutex(&this->dialog_mutex);
			return &this->dialog_list[i];
		}
	}
	db_unlock_mutex(&this->dialog_mutex);
	return 0;
}



DIALOG *SIGNAL::get_dialog(char *callid) {
	size_t i = 0;
	db_lock_mutex(&this->dialog_mutex);
	for (; i < MAX_DIALOG_COUNT; i++) {
		if (!strcmp(this->dialog_list[i].dialog_info.callid, callid)) {
			db_unlock_mutex(&this->dialog_mutex);
			return &this->dialog_list[i];
		}
	}
	db_unlock_mutex(&this->dialog_mutex);
	return 0;
}

size_t SIGNAL::get_call_list(CALLINFO **ci) {
	size_t call_count = 0, i = 0, j = 0;
	CALLINFO *ci_array = 0;
	DIALOG *dialog = 0;
	if (!ci) return 0;
	*ci = 0;

	db_lock_mutex(&this->dialog_mutex);
	for (i = 0; i < MAX_DIALOG_COUNT; i++) {
		dialog = &this->dialog_list[i];
		if (dialog->inuse == 1) {
			if (dialog->isinvite) call_count++;
		} else if (dialog->inuse == 0) {
			continue;
		} else {
			printf("WTF?! Corrupt memory here...\n");
		}
	}

	if (call_count == 0) {
		db_unlock_mutex(&this->dialog_mutex);
		return 0;
	}

	//ci_array = (CALLINFO *)MEMORY::calloc_(sizeof(CALLINFO), call_count, (char *)"signalling.cpp > SIGNAL::get_call_list(): ci_array = (CALLINFO *)calloc()");
	ci_array = (CALLINFO *)calloc(sizeof(CALLINFO), call_count);

	for (i = 0; i < MAX_DIALOG_COUNT; i++) {
		dialog = &this->dialog_list[i];
		if (!dialog->inuse || !dialog->isinvite) continue;
		memmove(&ci_array[j], &dialog->callinfo, sizeof(CALLINFO));
		strcpy(ci_array[j++].callid, dialog->dialog_info.callid);
	}

	db_unlock_mutex(&this->dialog_mutex);
	*ci = ci_array;
	return call_count;
}


void SIGNAL::dialog_change_status(DIALOG *d, CALL_STATUS newstatus, int force) {
    CALLINFO *ci = 0;
    if (!d) return;
    if (d->callinfo.status == newstatus && !force) return;
    d->last_status_change = time(0);
    d->callinfo.status = newstatus;
    if (d->isinvite) {
        this->check_dialogs();
        if (this->call_status_change_callback) {
            if (this->do_not_disturb == 0) {
                ci = (CALLINFO *)calloc(sizeof(CALLINFO), 1);
                memmove(ci, &d->callinfo, sizeof(CALLINFO));
                strcpy(ci->callid, d->dialog_info.callid);
                this->call_status_change_callback(this, ci);
            } else {
                printf("signalling.cpp > dialog_change_status(): This SIGNAL is set to DO NOT DISTURB.\n\n");
            }
        } else {
            printf("signalling.cpp > dialog_change_status(): No callback defined.\n\n");
        }
    }
}


void SIGNAL::dialog_change_status(DIALOG *d, CALL_STATUS newstatus) {
	CALLINFO *ci = 0;
	if (!d) return;
	if (d->callinfo.status == newstatus) return;
	d->last_status_change = time(0);
	d->callinfo.status = newstatus;
	if (d->isinvite) {
		this->check_dialogs();
		if (this->call_status_change_callback) {
            if (this->do_not_disturb == 0) {
                ci = (CALLINFO *)calloc(sizeof(CALLINFO), 1);
                memmove(ci, &d->callinfo, sizeof(CALLINFO));
                strcpy(ci->callid, d->dialog_info.callid);
                this->call_status_change_callback(this, ci);
            } else {
                printf("signalling.cpp > dialog_change_status(): This SIGNAL is set to DO NOT DISTURB.\n\n");
            }
		} else {
			printf("signalling.cpp > dialog_change_status(): No callback defined.\n\n");
		}
	}
}



/*
This function is called after each SIP message.
It goes through each dialog we have and determines if the
phone should be ringing or not.
*/
void SIGNAL::check_dialogs(void) {
	size_t i = 0;
	DIALOG *dialog = 0;
    int total_call_count = 0;
	int calls_talking = 0;
	int calls_on_hold = 0;
	int calls_ringing_in = 0;
	int calls_ringing_out = 0;
	for (; i < MAX_DIALOG_COUNT; i++) {
		dialog = &this->dialog_list[i];
		if (!dialog->inuse) continue;
		if (dialog->callinfo.status == TALKING) {
			calls_talking++;
            total_call_count++;
		} else if (dialog->callinfo.status == RINGING_IN) {
			calls_ringing_in++;
            total_call_count++;
		} else if (dialog->callinfo.status == RINGING_OUT) {
			calls_ringing_out++;
            total_call_count++;
		} else if (dialog->callinfo.status == ONHOLD) {
			calls_on_hold++;
            total_call_count++;
        } else if (dialog->callinfo.status == TRANSFERRING) {
            total_call_count++;
        } else if (dialog->callinfo.status == CONFIRMING) {
            total_call_count++;
        } else if (dialog->callinfo.status == ANSWERING) {
            total_call_count++;
		}
	}
    this->calls_talking = calls_talking;
    this->total_call_count = total_call_count;
//    printf("\nCalls Talking: %d\n", calls_talking);
//    printf("total_call_count: %d\n", total_call_count);
//    printf("Calls Ringing in: %d\n\n", calls_ringing_in);
	if (this->rtp) {
		if ( /* The phone is not ringing */ !this->rtp->is_ringing()) {
			if ( /* There are incoming calls */ calls_ringing_in > 0) {
				if ( /* We are not on the phone */ calls_talking == 0 && calls_ringing_out == 0) {
					if (this->do_not_disturb == 0) {
						this->rtp->start_ring();
						if (this->ringing_status_callback) this->ringing_status_callback(1);
					}

				}
			}
		} else /* The phone is currently ringing */ {
			if ( /* There are no incoming calls */ calls_ringing_in == 0 ) {
				this->rtp->stop_ring();
				if (this->ringing_status_callback) this->ringing_status_callback(0);
			}
		}
        if (calls_ringing_in == 0) {
            this->rtp->stop_ring();
        }
	}
	return;
}

/*
Returns:

> 0 - Success (Returns number of dialogs)

0 - Failed because DIALOG already exists.

-1 - Failed because Call-ID for parameter #1 could not be extracted.

-2 - Out of memory.

*/
DIALOG *SIGNAL::add_dialog(SIP *sipmessage) {
	char *szcallid = 0, *szcstime = 0, *szcstimeend = 0, *sztarget = 0;
	size_t lencallid = 0, lencstime = 0, lentarget = 0, i = 0;
	time_t temp_start_time = 0;
	DIALOG *dialog = 0;
	struct _headerlist *header = 0;
	SIPSTRING *current_value = 0;
    CALL_STATUS new_call_status = UNKNOWN;
    
    
	szcallid = sipmessage->message.callid_header->value.string;
	lencallid = sipmessage->message.callid_header->value.length;
	if (!szcallid || lencallid == 0) return 0; /* Not Call-ID found in SIP */
	dialog = this->get_dialog(szcallid, lencallid);
	if (dialog) return dialog;


	if (sipmessage->sipsize != strlen(sipmessage->pmessage)) {
		printf("signalling.cpp > add_dialog(): Failing due to sipmessage->sipsize \"%lu\" != strlen(sipmessage->pmessage) \"%lu\"!\n", (unsigned long)sipmessage->sipsize, (unsigned long)strlen(sipmessage->pmessage));
		printf("+---------  Message Causing Error ---------------------------------------------------------+\n");
		printf("%s\n", sipmessage->pmessage);
		printf("+------------------------------------------------------------------------------------------+\n\n");
		return 0;
	}

	db_lock_mutex(&this->dialog_mutex);


	dialog = 0;

	/* Check for any empty elements to re-use */
	for (i = 0; i < MAX_DIALOG_COUNT; i++) {
		if (this->dialog_list[i].inuse == 0) {
			dialog = &this->dialog_list[i];
			break;
		}
	}

	if (!dialog) {
		db_unlock_mutex(&this->dialog_mutex);
		return 0;
	}
	dialog->inuse = 1;
	/* Fill out DIALOG info */

	/* Add our tag to this dialog */
	this->newtag(dialog->dialog_info.mine.tag);

	dialog->dialog_errors = 0;
	dialog->last_ring = 0;

	dialog->callinfo.dialog = dialog;

	dialog->dialog_created_time = time(0);

	/* Set the Call-ID for this dialog */
	sprintf(dialog->dialog_info.callid, "%.*s",
		((lencallid > 499) ? 499 : (int)lencallid),
		szcallid);

	/* Set the branch */
	sprintf(dialog->dialog_info.branch, "%.*s",
		((sipmessage->message.via_header->sip_headerinfo.sip_via.branch.length > 499) ? 499 : (int)sipmessage->message.via_header->sip_headerinfo.sip_via.branch.length),
		sipmessage->message.via_header->sip_headerinfo.sip_via.branch.string);



	sprintf(dialog->dialog_info.mine.extension, "%d", this->user_info.extension);

	sprintf(dialog->dialog_info.mine.ipaddress, "%s", this->user_info.ipaddress);
	dialog->dialog_info.mine.port = this->user_info.rport;
	dialog->callinfo.status = UNKNOWN;
	dialog->isinvite = 0;
	if (sipmessage->message.request.length > 0) {
		if (!strncmp(sipmessage->message.request.string, "SUBSCRIBE", 9)) {
            if (!sipmessage->isSIPresponse()) {
                dialog->dialog_info.mine.cseq = sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence;
            }
			dialog->callinfo.status = SUBSCRIPTION;
		} else if (!strncmp(sipmessage->message.request.string, "INVITE", 6)) {
			dialog->isinvite = 1;
			if ( /* This is an outgoing call the "From" has our extension */
				sipmessage->message.from_header->sip_headerinfo.uri->iextension() ==
				(unsigned long long) this->user_info.extension) {

				dialog->dialog_info.mine.cseq = sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence;
				dialog->callinfo.is_incoming = 0;
				/* Set "talking_to_number" which is the number in the "To" field */

                sztarget = sipmessage->message.to_header->sip_headerinfo.uri->_extension.string;
                lentarget =sipmessage->message.to_header->sip_headerinfo.uri->_extension.length;
                
				if (lentarget > 0) {
					sprintf(dialog->callinfo.talking_to_number, "%.*s", ((lentarget < 100) ? (int)lentarget : 99), sztarget);
                    if (lentarget < 10) {
                        if ( /* It's not a special dial instruction */ sztarget[0] >= '1' && sztarget[0] <= '9') {
                            dialog->callinfo.is_intercom = 1;
                        }
                    }
				}
				/*  Fill out the THEIR fields here... */

                /* Check if this is an intercom call */
                
				/* Add their tag to dialog! */
				if (sipmessage->message.to_header->sip_headerinfo.uri->_tag.length > 0) {
					sprintf(dialog->dialog_info.theirs.tag, "%.*s",
						((sipmessage->message.to_header->sip_headerinfo.uri->_tag.length > 499) ? 499 : (int)sipmessage->message.to_header->sip_headerinfo.uri->_tag.length),
						sipmessage->message.from_header->sip_headerinfo.uri->_tag.string);
				}

				current_value = &sipmessage->message.from_header->sip_headerinfo.uri->_tag;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.mine.tag, "%.*s", (int)current_value->length, current_value->string);
				}


				current_value = &sipmessage->message.to_header->sip_headerinfo.uri->_extension;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.extension, "%.*s", (int)current_value->length, current_value->string);
				}

				current_value = &sipmessage->message.to_header->sip_headerinfo.uri->_ipaddress;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.ipaddress, "%.*s", (int)current_value->length, current_value->string);
				}
				current_value = &sipmessage->message.to_header->sip_headerinfo.uri->_username;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.name, "%.*s", (int)current_value->length, current_value->string);
				}
				current_value = &sipmessage->message.to_header->sip_headerinfo.uri->_port;
				if (current_value->length > 0) {
					char *temp_stopspot = &current_value->string[current_value->length];
					dialog->dialog_info.theirs.port = (unsigned short)strtoul(current_value->string, &temp_stopspot, 10);
				}


				/*
				When done, put a function that when a response is receives - right after it looks up
				any existing dialogs, it updates the dialog with any pertinent information.
				*/


                new_call_status = RINGING_OUT;
			} else /* This is an incoming call */ {
				dialog->callinfo.is_incoming = 1;
				dialog->dialog_info.theirs.cseq = sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence;
				dialog->dialog_info.mine.cseq = 101;

				/* Add their tag to dialog! */
				if (sipmessage->message.from_header->sip_headerinfo.uri->_tag.length > 0) {
					sprintf(dialog->dialog_info.theirs.tag, "%.*s",
						((sipmessage->message.from_header->sip_headerinfo.uri->_tag.length > 499) ? 499 : (int)sipmessage->message.from_header->sip_headerinfo.uri->_tag.length),
						sipmessage->message.from_header->sip_headerinfo.uri->_tag.string);
				}

				current_value = &sipmessage->message.from_header->sip_headerinfo.uri->_extension;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.extension, "%.*s", (int)current_value->length, current_value->string);
				}

				current_value = &sipmessage->message.from_header->sip_headerinfo.uri->_ipaddress;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.ipaddress, "%.*s", (int)current_value->length, current_value->string);
				}
				current_value = &sipmessage->message.from_header->sip_headerinfo.uri->_username;
				if (current_value->length > 0) {
					sprintf(dialog->dialog_info.theirs.name, "%.*s", (int)current_value->length, current_value->string);
				}
				current_value = &sipmessage->message.from_header->sip_headerinfo.uri->_port;
				if (current_value->length > 0) {
					char *temp_stopspot = &current_value->string[current_value->length];
					dialog->dialog_info.theirs.port = (unsigned short)strtoul(current_value->string, &temp_stopspot, 10);
				}

				/* Transfer Caller's phone number to the CALLINFO structure */
				sztarget = sipmessage->message.from_header->sip_headerinfo.uri->extension();
				lentarget = strlen(sztarget);
				if (lencallid > 0) {
					sprintf(dialog->callinfo.talking_to_number, "%.*s", ((lentarget < 100) ? (int)lentarget : 99), sztarget);
				}

				/* Set the caller's name in the CALLINFO structure */
				sztarget = sipmessage->getheaderval("X-CallerName", &lentarget);
				if (sztarget && lentarget > 0) {
					sprintf(dialog->callinfo.talking_to_name, "%.*s", ((lentarget < 100) ? (int)lentarget : 99), sztarget);
				}


                sztarget = sipmessage->getheaderval("X-Unique-ID", &lentarget);
                if (sztarget && lentarget > 0) {
                    sprintf(dialog->callinfo.uniqueid, "%.*s", ((lentarget < 200) ? (int)lentarget : 199), sztarget);
                }

                //X-Unique-ID
                
                
				/* Set the DID number */
				header = sipmessage->getheader("Diversion");
				if (header) {
					/* Asterisk set a "Diversion" header */
					sztarget = header->sip_headerinfo.uri->extension();
					lentarget = strlen(header->sip_headerinfo.uri->extension());
					if (lentarget > 0) {
						sprintf(dialog->callinfo.did, "%.*s", ((lentarget < 100) ? (int)lentarget : 99), sztarget);
					}
				} else {
					printf("** No \"Diversion\" header, looking for X-DID **\n");
					sztarget = sipmessage->getheaderval("X-DID", &lentarget);
				}

				if (sztarget && lentarget > 0) {
					sprintf(dialog->callinfo.did, "%.*s", ((lentarget < 100) ? (int)lentarget : 99), sztarget);
				}

				/* Set Media Code */

				/* Set Media List ID */

				/* Set Media Name */

				/* Set CALLINFO::is_intercom */

                /* Set the caller's name in the CALLINFO structure */
                dialog->callinfo.is_intercom = 0;
                sztarget = sipmessage->getheaderval("X-Intercom", &lentarget);
                if (sztarget && lentarget > 0) {
                    char *endptr_ = &sztarget[lentarget];
                    dialog->callinfo.is_intercom = (int)strtoull(sztarget, &endptr_, 10);
                    if (dialog->callinfo.is_intercom) {
                        //sipmessage->show_message();
                        printf("signal.cpp > add_dialog(): Incoming Intercom  !!!\n\n");
                    }
                }

				/* Set call time  "X-Server-Time" */
				dialog->callinfo.call_start_time = time(0);
				szcstime = sipmessage->getheaderval("X-Server-Time", &lencstime);
				if (szcstime && lencstime) {
					szcstimeend = &szcstime[lencstime];
					temp_start_time = (time_t)strtoull(szcstime, &szcstimeend, 10);
					if (temp_start_time > 0) {
						dialog->callinfo.call_start_time = strtoull(szcstime, &szcstimeend, 10);
					}
				}
				/* Set Unique ID "X-Unique-ID" */
                new_call_status = RINGING_IN;
			}
		}
	}
	dialog->primary = sipmessage;
	db_unlock_mutex(&this->dialog_mutex);
    
    this->dialog_change_status(dialog, new_call_status);
    
	return dialog;
}

const char *SIGNAL::status_to_text(CALL_STATUS cs) {
	switch (cs) {
		case HUNGUP:
			return "HUNG UP";
		case HANGING_UP:
			return "HANGING UP";
		case RINGING_IN:
			return "RINGING IN";
		case RINGING_OUT:
			return "RINGING OUT";
		case ANSWERING:
			return "ANSWERING";
		case TALKING:
			return "TALKING";
        case PUTTING_ONHOLD:
            return "PUTTING ON HOLD";
		case ONHOLD:
			return "ON HOLD";
		case TRANSFERRING:
			return "TRANSFERRING";
		case CANCELLING_CALL:
			return "CANCELLING CALL";
		case CONFIRMING:
			return "CONFIRMING";
		case SUBSCRIPTION:
			return "SUBSCRIPTION";
			break;
		default:
			break;
	}
	return "UNKNOWN";

}

void SIGNAL::abort_dialog(DIALOG *dialog) {
	if (!dialog) return;
	printf("* Immediately freeing dialog \"%s\" (Status: %s).\n", dialog->dialog_info.callid, this->status_to_text(dialog->callinfo.status));
	if (dialog->primary) {
		delete dialog->primary;
	} else {
		printf(" ** WARNING ** We destroyed a dialog that had a NULL pointer in DIALOG::primary **\n");
	}
	memset(dialog, 0, sizeof(DIALOG));
}

int SIGNAL::destroy_dialog(char *szcallid) {
	DIALOG *dialog = 0;
	if (!szcallid) return 0;
	dialog = this->get_dialog(szcallid);
	if (!dialog) return 0;
	if (this->rtp) {
		this->rtp->stop_audio(szcallid);
	}
	if (!this->call_status_change_callback)
		printf("SIGNAL::destroy_dialog(): Setting timer to destroy dialog \"%s\"\n", dialog->dialog_info.callid);
	dialog->destroy_after = time(0) + 5;
	return 1;
}




int SIGNAL::pullSIPoffstack(struct SIGNAL::__socket *s) {
	char *sip = 0;
	char debug[400];
	int i = 0, messages_processed = 0;
	size_t sipsize = 0;
	//unsigned short rport = 0;
	char *rportstop = 0;
	int responsecode = 0, pass_to_application = 1;
	//struct signal_message *newmsgptr = 0;
	SIP *sipmessage = 0;
	DIALOG *dialog = 0;

	if (s->buffersize == 0) return 0;
	do {
		sipmessage = 0;
		if (sip) {
			/* This will return 0 if no complete message is available, or the size in bytes of the top SIP message. */
			sipsize = this->getwholeSIPmessage(sip);
			if ( /* We have an entire SIP message */ sipsize > 0) {
				if (sipsize < 50000) { /* We should NEVER have a SIP message this large. */

									   /* Check if this is an OPTIONS, or a REGISTRATION */
					sipmessage = new SIP(sip, sipsize);
					//MEMORY::add_pointer_to_track(sipmessage, (char *)"signalling.cpp > SIGNAL::pullSIPoffstack(): sipmessage = new SIP(sip, sipsize);");
#ifdef SIGNAL_SHOW_MY_SIP
					sipmessage->show_message();
#endif
					responsecode = sipmessage->isSIPresponse();
					if (sipmessage) {
						s->last_response = time(0);
						if ( /* This is a response */ responsecode) {
							dialog = this->get_dialog(sipmessage->message.callid_header->value.string,
								sipmessage->message.callid_header->value.length);

							if (dialog) {
								/* Check tags */
								if (strlen(dialog->dialog_info.theirs.tag) == 0 &&
									sipmessage->message.to_header->sip_headerinfo.uri->_tag.length > 0) {
									sprintf(dialog->dialog_info.theirs.tag, "%.*s",
										(int)sipmessage->message.to_header->sip_headerinfo.uri->_tag.length,
										sipmessage->message.to_header->sip_headerinfo.uri->_tag.string);
									printf("Assigning the remote tag of \"%s\" to this dialog!\n", dialog->dialog_info.theirs.tag);
								}
								if (!dialog->isinvite) {
									sipmessage->show_message();
								}
							}

							if (sipmessage->message.via_header->sip_headerinfo.sip_via.received.length > 0) {
								this->setlocalip(sipmessage->message.via_header->sip_headerinfo.sip_via.received.string,
									sipmessage->message.via_header->sip_headerinfo.sip_via.received.length);
							}
							if (sipmessage->message.via_header->sip_headerinfo.sip_via.rport.length > 0) {
								rportstop = &sipmessage->message.via_header->sip_headerinfo.sip_via.rport.string[sipmessage->message.via_header->sip_headerinfo.sip_via.rport.length];
								this->user_info.rport = (unsigned short)strtoul(sipmessage->message.via_header->sip_headerinfo.sip_via.rport.string, &rportstop, 10);
							}

							switch (responsecode) {
								/*
								? Unauthorized response should always result in a new
								registration if they have the "WWW-Authenticate" header ?
								*/
								case 180: /* Ringing */
								case 100: /* Trying */
										  //sipmessage->show_message();
//                                    if (dialog) {
//                                        this->dialog_change_status(dialog, RINGING_OUT);
//                                    }
									break;
								case 183: /* Session Progress (Early Media?) */
										  //sipmessage->show_message();
									if (dialog) {
										this->dialog_change_status(dialog, RINGING_OUT);
									}
									if (!dialog->audio_active) {
										if (sipmessage->message.content_type == CT_SDP) {
											if (this->rtp) {
												if (this->rtp->start_audio(sipmessage->message.content.sdp.audio_address,
													sipmessage->message.content.sdp.audio_port,
													dialog->dialog_info.callid,
													dialog->dialog_info.mine.ssrc) > 0) {
													dialog->audio_active = 1;
												}
											}

                                        }
									}
									break;
								case 402:
									if (dialog) {
										this->destroy_dialog(dialog->dialog_info.callid);
									}
									break;
								case 404:
									if (dialog) {
										this->destroy_dialog(dialog->dialog_info.callid);
									}
									break;
								case 481: /* Call Leg/Transaction does not exist */
									if (dialog) {
                                        switch (dialog->callinfo.status) {
                                            case CANCELLING_CALL:
                                                /* For some reason our CANCEL failed */
                                                this->dialog_change_status(dialog, HANGING_UP);
                                                this->bye(dialog);
                                                break;
                                            case TRANSFERRING:
                                            case PUTTING_ONHOLD:
                                                this->ack(dialog);
                                            default:
                                                this->dialog_change_status(dialog, HUNGUP);
                                                this->destroy_dialog(dialog->dialog_info.callid);
                                                break;
                                        }
									}
									break;
								case 487:
									if (dialog) {
										this->dialog_change_status(dialog, HUNGUP);
										this->destroy_dialog(dialog->dialog_info.callid);
									}
								case 401: /* 401 will never go to the application */
										  //sipmessage->message

									if (JSTRING::matches((char *)"register", sipmessage->message.request.string)) {
										if (!this->registration_info.auth_sent) {
											//OutputDebugString("Handle 401 to REGISTER attempt.\n");
											if (sipmessage->message.authinfo.stale == 0) {
												//OutputDebugString("\tNonce is not stale.  Sending auth.\n");
												// process 401, auth, send auth
												if ( /* This message contains a reported IP Address */
													sipmessage->message.via_header->sip_headerinfo.sip_via.received.length > 0) {
													if ( /* The reported IP Address doesn't match what we think it is (due to NAT) */
														!JSTRING::matches(this->user_info.ipaddress,
															sipmessage->message.via_header->sip_headerinfo.sip_via.received.string)) {
														/* Set new IP Address on this SIGNAL instance */
														sprintf(this->user_info.ipaddress, "%.*s",
															(int)sipmessage->message.via_header->sip_headerinfo.sip_via.received.length,
															sipmessage->message.via_header->sip_headerinfo.sip_via.received.string);
													}
												}
												if (sipmessage->message.via_header->sip_headerinfo.sip_via.usrport != this->user_info.rport) {
													this->user_info.rport = sipmessage->message.via_header->sip_headerinfo.sip_via.usrport;
												}
												/*
												if (this->registration_info.failed_attempts == 0) {

												sprintf(debug, "***** This Deedle's reported IP and port: %s:%u\n",
												this->user_info.ipaddress,
												this->user_info.rport);
												OutputDebugString(debug);
												}*/
												this->auth(sipmessage, s);
												this->registration_info.auth_sent = 1;
											} else {
												OutputDebugString("NONCE was stale. Ignoring this message.\n");
											}
										} else {
											printf("* Auth sent and rejected.\n");
                                            sprintf(this->string_error_condition, "SIP Server rejected our authorization with a 401. (Check auth name and password?) ");
                                            this->registration_info.last_registration_attempt = time(0);
                                            if (++this->registration_info.failed_attempts < 3) {
                                                this->registration_info.auth_sent = 0;
                                                if (this->registration) {
                                                    delete this->registration;
                                                    this->registration = 0;
                                                }
                                            } else /* We've failed 3 attempts to register */ {
                                                printf("* Failed THREE registrations attempts.  Shutting down this instance of SIGNAL.\n\n");
                                                sprintf(this->string_error_condition, "SIP Server rejected 3 REGISTER attempts");
                                                if (this->registration) {
                                                    delete this->registration;
                                                    this->registration = 0;
                                                }
                                                this->shut_down = 1;
                                                this->shuttingdown = 1;
                                                this->signal_thread_running = 0;
                                            }
										}


									} else if (JSTRING::matches((char *)"OPTIONS", sipmessage->message.request.string)) {
										if (this->user_info.ipconfirmed == 0) {
											this->user_info.ipconfirmed = 1;
										}
										//OutputDebugString("<< OPTIONS qualify response received >>\n\n");
									} else /* This was either an INVITE, or a REFER to which we received a 401 Unauthorized */ {

										// Maybe set up a dialog for OPTIONS?

										if (dialog) {
											/*  This was unauthorized on this dialog */
											//sipmessage->show_message();

											if (dialog->isinvite) {
												if ( /* We're setting up a new call */ dialog->callinfo.status == RINGING_OUT) {
													// Authenticate and resend invite.

													this->ack(dialog);
													if (dialog->dialog_errors++ <= 3) {
														this->auth(dialog, sipmessage);
													} else /* We've failed four times.  Quit trying on this. */ {
														/* Set an error for the user? */
														//this->ack(dialog);
														this->dialog_change_status(dialog, CANCELLING_CALL);
														this->destroy_dialog(dialog->dialog_info.callid);
													}
												}
											} else if (!strncmp(dialog->primary->message.request.string, "SUBSCRIBE", 9)) {
												printf("SUBSCRIBE NEEDS AUTHENTICATION!!\n");
												if (dialog->dialog_errors++ <= 3) {
													this->auth(dialog, sipmessage);
												} else /* We've failed four times.  Quit trying on this. */ {
													this->destroy_dialog(dialog->dialog_info.callid);
												}
											} else {
												printf("*** WHY ARE WE HERE?! ***\n");
											}

										} else {
											OutputDebugString("Unauthorized (Not passed to application)...\n");
											sprintf(debug, "%d Parsed Header(s):\n", sipmessage->message.headercount);
											OutputDebugString(debug);
											for (i = 0; i < sipmessage->message.headercount; i++) {
												sprintf(debug, "%d. Header: %.*s\nValue: %.*s\n",
													(i + 1),
													(int)sipmessage->message.headers[i]->header.length,
													sipmessage->message.headers[i]->header.string,
													(int)sipmessage->message.headers[i]->value.length,
													sipmessage->message.headers[i]->value.string);
												OutputDebugString(debug);
											}
											OutputDebugString("\n\n");
										}


									}

									pass_to_application = 0;
									break;
								case 500:
									if (JSTRING::matches(sipmessage->message.request.string, (char *)"REGISTER")) {
                                        sprintf(this->string_error_condition, "SIP Server Error (SIP Code: 500 )");
										if (this->registration) {
											if (this->registration->message.expires == 0) {
												sprintf(debug, "\n\n !!!! 500 Server Error on un-REGISTER. Trying again... !!!!\n\n");
												OutputDebugString(debug);
												this->sipregister(s, 0);
											} else {
												sprintf(debug, "\n\n !!!! 500 Server Error on REGISTER with Expiry of %d seconds... !!!!\n\n", this->registration->message.expires);
												OutputDebugString(debug);
												//MEMORY::deleted(this->registration);
												delete this->registration;
												this->registration = 0;
												this->current_registration_expires = 0;
											}

										}
									} else {
										sipmessage->show_message();
									}
									pass_to_application = 0;
									break;
								case 403:
									/*
									We've received a 403 Response.  Let's try to un-REGISTER, and then try
									to REGISTER.
									*/
									if (JSTRING::matches(sipmessage->message.request.string, (char *)"REGISTER")) {
                                        sprintf(this->string_error_condition, "SIP Server rejected our authorization.");
										printf("**** Received a 403 message to our registration attempt!****\n");
										if (this->registration) {
											//MEMORY::deleted(this->registration);
											delete this->registration;
											this->registration = 0;
										}
										this->sipregister(s, 0);
										pass_to_application = 0;
									}
									break;
								case 202:
									if (dialog) {
										if (!strncmp(sipmessage->message.request.string, "REFER", 5)) {
											this->dialog_change_status(dialog, TRANSFERRING);
											if (this->rtp) this->rtp->stop_audio(dialog->dialog_info.callid);
										}
									}
									break;
								case 200:
									if ( /* This 200 has to do with an established dialog */ dialog) {

										if ( /* This is a valid response to our last CSeq */
											sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence ==
											dialog->dialog_info.mine.cseq) {
											switch (dialog->callinfo.status) {
												case CANCELLING_CALL:
													/* We got a 200 OK on a cancel.  Weird, it should be a 4xx */
													break;
												case CONFIRMING:
												case RINGING_OUT:
													/* Prob: An INVITE was answered */
													this->dialog_change_status(dialog, TALKING);
                                                    if (dialog->callinfo.is_intercom == 1) {
                                                        if (this->rtp) {
                                                            this->rtp->intercom_beep();
                                                        }
                                                    }
													/* Start Audio, if it's not already begun */
													if (!dialog->audio_active) {
														if (sipmessage->message.content_type == CT_SDP) {
															if (this->rtp) {
																if (this->rtp->start_audio(sipmessage->message.content.sdp.audio_address,
																	sipmessage->message.content.sdp.audio_port,
																	dialog->dialog_info.callid,
																	dialog->dialog_info.mine.ssrc) > 0) {
																	dialog->audio_active = 1;
																}
															}
														}
													}

													this->ack(dialog);
													break;
                                                case PUTTING_ONHOLD:
                                                    if (sipmessage->message.content.sdp.audio_direction == AD_INACTIVE) {
                                                        this->dialog_change_status(dialog, ONHOLD);
                                                        if(this->rtp){
                                                            this->rtp->stop_audio(dialog->dialog_info.callid);
                                                            dialog->audio_active = 0;
                                                        }
                                                    } else {
                                                        this->dialog_change_status(dialog, TALKING);
                                                        printf("\n\n");
                                                        printf("$$$ FAILED TO PUT CALL ON HOLD!!!! $$$$\n");
                                                        sipmessage->show_message();
                                                        printf("\n\n");
                                                    }
                                                    this->ack(dialog);
                                                    break;
                                                case ONHOLD: // Got a 200 OK and the call is on hold.
                                                    if(sipmessage->message.content.rawcontent != 0) { // We DO have SDP for audio
                                                        switch (sipmessage->message.content.sdp.audio_direction) {
                                                            case AD_SENDRECV: /* The SDP in the 200 OK wants audio */
                                                                if (this->rtp) {
                                                                    if (this->rtp->start_audio(sipmessage->message.content.sdp.audio_address,
                                                                       sipmessage->message.content.sdp.audio_port,
                                                                       dialog->dialog_info.callid,
                                                                       dialog->dialog_info.mine.ssrc) > 0) {
                                                                        printf("**** DIALOG SET BACK TO TALKING *****\n");
                                                                        dialog->audio_active = 1;
                                                                        this->dialog_change_status(dialog, TALKING);
                                                                        
                                                                    } else {
                                                                        printf("***** FAILED TO START AUDIO ******\n");
                                                                    }
                                                                } else {
                                                                    printf("**** NO RTPENGINE TRYING TO UNHOLD CALL ****\n");
                                                                }
                                                                break;
                                                            default:
                                                                printf("**** FAILED TO TAKE CALL OFF OF HOLD!\n");
                                                                break;
                                                        }
                                                        
                                                        
                                                        
                                                    }
                                                    this->ack(dialog);
                                                    break;
												case TALKING:
													// Possible re-INVITE
													break;
                                                case HUNGUP:
												case HANGING_UP:
													if ( /* If this is a 200 on a BYE */
														JSTRING::matches(sipmessage->message.request.string, (char *)"BYE")) {
                                                        this->ack(dialog);
														printf("200 OK Received for our BYE.  Scheduling destruction of dialog.\n");
														this->dialog_change_status(dialog, HUNGUP);
														if (this->rtp) {
															this->rtp->stop_audio(dialog->dialog_info.callid);
														}
														this->destroy_dialog(dialog->dialog_info.callid);
                                                    } else {
                                                        printf("*** 200 OK ***\n");
                                                        sipmessage->show_message();
                                                        printf("***********************\n");
                                                    }
													break;
												default:
													printf("\nUnexpected 200 OK *********************\n");
													sipmessage->show_message();
													break;
											}
										}

									} else /* This 200 was not in response to a dialog we're tracking */ {
										if (JSTRING::matches(sipmessage->message.request.string, (char *)"REGISTER")) {
                                            this->string_error_condition[0] = 0;
											if (sipmessage->message.expires == 0) {
												/* We are unregistered */
												if (this->registered_callback) {
													this->registered_callback(this, 0);
												} else {
//                                                    OutputDebugString("\n\n<------ Successfully un-REGISTERED ------------->\n\n");
                                                    this->string_error_condition[0] = 0;
												}
												if (this->registration) {
													//MEMORY::deleted(this->registration);
													delete this->registration;
												}
												this->registration = 0;
												this->current_registration_expires = 0;
											} else {
												if (this->registered_callback) {
													this->registered_callback(this, 1);
												} else {
//                                                    OutputDebugString("\n\n<------ Successfully REGISTERED ------------->\n\n");
                                                    this->string_error_condition[0] = 0;
												}
												SIP *regrequest = this->registration;
												this->registration = sipmessage;
												sipmessage->message.expires = regrequest->message.expires;
												//MEMORY::deleted(regrequest);
												delete regrequest;
												this->current_registration_expires = time(0) + this->registration->message.expires;
												this->registration_info.auth_sent = 0;
												this->registration_info.failed_attempts = 0;
											}
											pass_to_application = 0;
										} else if (JSTRING::matches(sipmessage->message.request.string, (char *)"OPTIONS")) {
											//printf("200 OK to our OPTIONS\n");
										} else {
											printf("?! WTF ?! *****************\n");
											sipmessage->show_message();
										}
									}
									break;
								default: /* unhandled response code */
                                    sipmessage->show_message();
                                    if ( /* This is a final response */ responsecode >= 400) {
                                        if (dialog) {
                                            printf("*** Got a FINAL SIP response on a dialog.  Destroying it.\n");
                                            if (dialog->isinvite) {
                                                this->ack(dialog);
                                            }
                                            
                                            switch (dialog->callinfo.status) {
                                                case PUTTING_ONHOLD:
                                                case TRANSFERRING:
                                                    /* It appears the HOLD or TRANSFER operation we
                                                     attempted failed, so just continue TALKING */
                                                    this->dialog_change_status(dialog, TALKING);
                                                    break;
                                                case TALKING:
                                                    /* I don't know why we'd get a RESPONSE code
                                                     while we're still TALKING, but for now, ignore it.
                                                     */
                                                    break;
                                                default:
                                                    /* To err on the side of caution and saving resources,
                                                     hang up this dialog and schedule it for destruction.
                                                     */
                                                    this->dialog_change_status(dialog, HUNGUP);
                                                    this->destroy_dialog(dialog->dialog_info.callid);
                                                    break;
                                            }
                                        }
                                    }
									break;
							}
							if (/* We're not shutting down and we're not registered */
                                !this->shuttingdown && !this->registration) {
								/* Here, we've received a response to something and we are NOT shutting down
								and we are NOT registered, so let's register.
								*/
								//sipmessage->show_message();
								if (!this->isregistered()) {
									if (this->registered_callback) {
										this->registered_callback(this, -1);
									}
								}
								if (/* It's been more than 5 seconds since the last
                                     registration attempt, retry registraion
                                     */
                                    (time(0) - this->registration_info.last_registration_attempt) > 5) {
									//if (!this->registered_callback) {
//                                        OutputDebugString("\n<<< Starting REGISTRATION >>>\n");
									//}
									this->sipregister(s, REGISTRATION_EXPIRATION);
								}
							}

						} else /* This is a request */ {
							dialog = this->get_dialog(sipmessage->message.callid_header->value.string,
								sipmessage->message.callid_header->value.length);
							if (!strncmp(sip, "OPTIONS", 7)) {
//                                this->ok(sipmessage);
                                this->response(sipmessage, 200);
							} else if (JSTRING::matches(sip, (char *)"INVITE")) {

								/* Invite received, or new invite */

								//sipmessage->show_message();
								if ( /* There is NOT dialog established for this NEW SIP message */ !dialog) {
                                    if (this->rtp) this->rtp->prime_audio();
									dialog = this->add_dialog(sipmessage);
									if (!dialog) {
										printf("*** WARNING ***\nsignalling.cpp > pullSIPoffstack(): add_dialog() Failed, so this call is being rejected!\n\n");
//                                        this->busy(sipmessage);
                                        this->response(sipmessage, 486);
									} else {
										if (this->do_not_disturb) {
											printf("Rejecting call id \"%s\" from \"%s\"\n", dialog->dialog_info.callid, dialog->callinfo.talking_to_number);
											this->hangup(dialog->dialog_info.callid);
										}
									}
								} else {
									/* Possible re-INVITE */
									printf("*** Possible re-INVITE\n");

									/* Either send a 200 OK, or a 488 Not Acceptable Here */

									/* Expect an ACK */

									if ( /* CSeq is OK.  It is HIGHER than the last one processed */
										sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence >
										dialog->dialog_info.theirs.cseq) {
										dialog->dialog_info.theirs.cseq = sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence;

										
                                        this->response(sipmessage, 488);
									} else {
										printf("*** OLD CSeq (%d)! The current remote CSeq for this DIALOG is %d!\n",
											sipmessage->message.csq_header->sip_headerinfo.sip_cseq.sequence
											, dialog->dialog_info.theirs.cseq);
										sipmessage->show_message();
									}

								}

								/*
								Information:

								All PSTN termination Deedle servers attach a universally unique identifier ("X-Unique-ID" header)
								to INVITEs that are forwarded to User-Agents and other Deedle servers.




								Algo:
								If (We have this dialog (by Call-ID) instantiated)

								If (This CSeq is <= the "invite_list[]"'s current CSeq)

								1. Increment "dialog_errors" on this dialog

								If (We have this CSeq, and we've sent a response)

								1. Re-send the response we sent before.

								Else If (We DO NOT have this CSeq)

								-- Here, we have received an INVITE with the
								-- same Call-ID as one that's logged, but the
								-- CSeq is LOWER than what's expected.


								Send a 603 Decline






								End If (We have this CSeq, and we've sent a response)

								Else If ( This is a higher CSeq - A new request on this dialog )

								This is a re-INVITE.


								End If (This CSeq is less than the "invite_list[]"'s current CSeq)


								Else If ( There is NO record of this dialog )

								If (this is a DIALOG-CREATING INVITE (initial REQUEST))

								1. Create dialog
								2. Send 180 RINGING
								3. Call "set_new_call_callback" callback
								4. Call RINGING call back
								5. Set up 180 RINGING timer

								Else If (This is NOT a DIALOG-CREATING request)

								Send a 481 Call Leg/Transaction Not Found

								End If (this is a DIALOG-CREATING INVITE (initial REQUEST))



								End If (We have this dialog (by Call-ID) instantiated)
								*/


							} else if (JSTRING::matches(sip, (char *)"NOTIFY")) {
								if (dialog) {
									if (dialog->callinfo.status == TRANSFERRING) {
										if (sipmessage->message.content_type == CT_SIPFRAG) {
											if (sipmessage->message.content.rawcontent != 0) {
												int done_with_dialog = 0;
												if (strstr(sipmessage->message.content.rawcontent, "SIP/2.0 1")) {
													//                                                    printf("REFER is still pending.\n");
													/* Temporary Message */
												} else if (strstr(sipmessage->message.content.rawcontent, "SIP/2.0 4")) {
													/* Transfer failed */
													//                                                    printf("REFER failed and asterisk probably hung up the call.\n");
													done_with_dialog = 1;
												} else if (strstr(sipmessage->message.content.rawcontent, "SIP/2.0 5")) {
													/* Transfer failed */
													//                                                    printf("REFER failed and asterisk probably hung up the call.\n");
													done_with_dialog = 1;
												} else if (strstr(sipmessage->message.content.rawcontent, "SIP/2.0 200")) {
													/* Transfer succeeded */
													//                                                    printf("REFER was successful\n");
													done_with_dialog = 1;
												} else {
													done_with_dialog = 1;
													printf("Could not find any apropos info in NOTIFY content: \"%s\"\n", sipmessage->message.content.rawcontent);
												}
//                                                this->ok(sipmessage);
                                                this->response(sipmessage, 200);
												if (done_with_dialog) {
													this->hangup(dialog->dialog_info.callid);
													//                                                    printf("Hanging up dialog after REFER.\n");
												}
											} else {
												//                                                printf("SIPFRAG CONTENT WAS MISSING!!\n");
//                                                this->ok(sipmessage);
                                                this->response(sipmessage, 200);
											}
										} else {
											printf("Invalid CONTENT TYPE for transfer!\n");
											printf("%s\n\n", sipmessage->message.content.rawcontent);
//                                            this->ok(sipmessage);
                                            this->response(sipmessage, 200);
										}

									} else {
										if (sipmessage->message.content.rawcontent != 0) {
											printf("%s\n\n", sipmessage->message.content.rawcontent);
										} else {
											sipmessage->show_message();
										}
//                                        this->ok(sipmessage);
                                        this->response(sipmessage, 200);
									}
								} else {
//                                    printf("* NOTIFY WITHOUT DIALOG *\n");
                                    
/*
 
 * NOTIFY WITHOUT DIALOG *
 Messages-Waiting: no
 Message-Account: sip:asterisk@173.192.209.79;transport=TCP
 Voice-Message: 0/0 (0/0)
 */
                                    if (sipmessage->message.content_type == CT_SIMPLE_MESSAGE_SUMMARY) { // Voicemail notification
                                        int vm_line_itr = 0;
                                        struct JSTRING::jsstring **vm_lines = 0;
                                        struct JSTRING::jsstring vm_header_val;
                                        vm_lines = JSTRING::split(sipmessage->message.content.rawcontent, '\n', (size_t)sipmessage->message.contentlength);
                                        int vm_new = 0, vm_old = 0, vm_new_urgent = 0, vm_old_urgent = 0, vm_waiting = 0;
                                        
                                        if (vm_lines) {
                                            while (vm_lines[vm_line_itr] != 0) {
                                                vm_header_val.length = 0;
                                                vm_header_val.ptr = 0;
                                                vm_header_val.ptr = JSTRING::headervalue(vm_lines[vm_line_itr]->ptr, '\n', &vm_header_val.length);
                                                vm_header_val.ptr = JSTRING::trim(vm_header_val.ptr, &vm_header_val.length, &vm_header_val.length);
                                                if (vm_header_val.length > 0) {
                                                    if (JSTRING::matches(vm_lines[vm_line_itr]->ptr, (char *)"messages-waiting")) {
                                                        if (JSTRING::matches(vm_header_val.ptr, (char*)"yes")) { // We have voice mail
                                                            vm_waiting = 1;
                                                        } else {
                                                            vm_waiting = 0;
                                                        }
                                                    } else if (JSTRING::matches(vm_lines[vm_line_itr]->ptr, (char *)"voice-message")) {
                                                        struct JSTRING::jsstring **vm_qtys = 0;
                                                        int vm_qty_itr = 0;
                                                        vm_qtys = JSTRING::split(vm_header_val.ptr, ' ', vm_header_val.length);
                                                        if (vm_qtys) {
                                                            while (vm_qtys[vm_qty_itr] != 0) {
                                                                if (vm_qtys[vm_qty_itr]->length > 0) {
                                                                    if (vm_qtys[vm_qty_itr]->ptr[0] == '(') { // This is marked as URGENT
                                                                        char *vm_urgent = 0;
                                                                        size_t vm_urgent_len = 0;
                                                                        vm_urgent = JSTRING::between(vm_qtys[vm_qty_itr]->ptr, "()", &vm_urgent_len);
                                                                        if (vm_urgent_len > 0) {
                                                                            char *qty_new = 0, *qty_old = 0, *qty_end = 0;
                                                                            size_t vm_num_len = 0;
                                                                            qty_new = JSTRING::stringfield(vm_urgent, '/', vm_urgent_len, 1, &vm_num_len);
                                                                            if (vm_num_len > 0 && qty_new) {
                                                                                qty_end = &qty_new[vm_num_len];
                                                                                vm_new_urgent = (int)strtoul(qty_new, &qty_end, 10);
                                                                            }
                                                                            qty_old = JSTRING::stringfield(vm_urgent, '/', vm_urgent_len, 2, &vm_num_len);
                                                                            if (vm_num_len > 0 && qty_old) {
                                                                                qty_end = &qty_old[vm_num_len];
                                                                                vm_old_urgent = (int)strtoul(qty_old, &qty_end, 10);
                                                                            }
                                                                        }
                                                                    } else { // Not URGENT
                                                                        char *qty_new = 0, *qty_old = 0,
                                                                            *qty_end = 0;
                                                                        size_t vm_num_len = 0;
                                                                        qty_new = JSTRING::stringfield(vm_qtys[vm_qty_itr]->ptr, '/', vm_qtys[vm_qty_itr]->length, 1, &vm_num_len);
                                                                        if (vm_num_len > 0 && qty_new) {
                                                                            qty_end = &qty_new[vm_num_len];
                                                                            vm_new = (int)strtoul(qty_new, &qty_end, 10);
                                                                        }
                                                                        qty_old = JSTRING::stringfield(vm_qtys[vm_qty_itr]->ptr, '/', vm_qtys[vm_qty_itr]->length, 2, &vm_num_len);
                                                                        if (vm_num_len > 0 && qty_old) {
                                                                            qty_end = &qty_old[vm_num_len];
                                                                            vm_old = (int)strtoul(qty_old, &qty_end, 10);
                                                                        }
                                                                    }
                                                                }
                                                                vm_qty_itr++;
                                                            }
                                                            JSTRING::freesplit(vm_qtys);
                                                        } else {
                                                            printf("** Voicemail QUANTITIES parsed.\n\n");
                                                        }
                                                    }
                                                }
                                                vm_line_itr++;
                                            }
                                            JSTRING::freesplit(vm_lines);
                                        }
                                        // Send voicemail alert here!
                                        if (this->voicemail_callback) {
                                            //void(*voicemail_callback)(int extension, int messages_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent)
                                            this->voicemail_callback(
                                                 this->user_info.extension,
                                                 vm_waiting,
                                                 vm_new,
                                                 vm_new_urgent,
                                                 vm_old,
                                                 vm_old_urgent);
                                        }
                                        
                                    } else {
                                        if (sipmessage->message.content.rawcontent != 0) {
                                            printf("%s\n\n", sipmessage->message.content.rawcontent);
                                        } else {
                                            sipmessage->show_message();
                                        }
                                    }
                                    this->response(sipmessage, 200);
								}



							} else if (JSTRING::matches(sip, (char *)"CANCEL")) {
								//sipmessage->show_message();
								/* Cancel a ringing_in call */
								if (!this->call_status_change_callback)
									printf("* Got a CANCEL message for Call-ID: \"%s\"\n\n", dialog->dialog_info.callid);
								if (dialog) {
                                    
                                    if (!sipmessage->getheader("Reason")) { // There is no reason
//                                        printf("\n\n*************** MISSED CALL *****************\n\n");
                                        dialog->callinfo.call_missed = 1;
                                    } else { // There IS a reason code
                                        int hi = 0, reason_param_itr = 0;
                                        struct _headerlist *header = 0;
                                        for (; hi < sipmessage->message.headercount; hi++) {
                                            header = sipmessage->message.headers[hi];
                                            if (header) {
                                                if (JSTRING::matches(header->header.string, (char *)"reason")) {
                                                    struct JSTRING::jsstring **reason_params = 0;
                                                    reason_params = JSTRING::split(header->value.string, ';', header->value.length);
                                                    if (reason_params) {
                                                        char *reason_parameter = 0;
                                                        int cause_number = 0;
                                                        int reason_sip_code = 0; // Set to 1 if the reason is SIP, 0 if it's Q.850
                                                        size_t reason_parameter_length = 0;
                                                        reason_param_itr = 0;
//                                                        printf("\"Reason\" Parameters:\n");
                                                        while (reason_params[reason_param_itr] != 0) { // Go through this Reason Line
                                                            reason_parameter = JSTRING::trim(reason_params[reason_param_itr]->ptr,
                                                                 &reason_params[reason_param_itr]->length,
                                                                 &reason_parameter_length);
//                                                            if (reason_parameter_length > 0) { // We have successfully gotten a parameter
//                                                                printf("param: %.*s\n", (int)reason_parameter_length, reason_parameter);
//                                                            }

                                                            if (reason_param_itr == 0) {
                                                                // This should be either "SIP" or "Q.850"
                                                                reason_sip_code = 0;
                                                                if (JSTRING::matches(reason_parameter, (char *)"SIP")) {
                                                                    reason_sip_code = 1;
                                                                }
                                                            } else {
                                                                if (JSTRING::matches(reason_parameter, (char *)"cause")) {
                                                                    size_t cause_code_length = 0;
                                                                    char *cause_start = 0, *cause_end = 0;
                                                                    cause_start = JSTRING::stringfield(reason_parameter, '=', reason_parameter_length, 2, &cause_code_length);
                                                                    cause_number = 0;
                                                                    if (cause_code_length > 0) {
                                                                        cause_start = JSTRING::trim(cause_start, &cause_code_length, &cause_code_length);
                                                                        if (cause_code_length > 0) {
                                                                            cause_end = &cause_start[cause_code_length];
                                                                            cause_number = (int)strtoul(cause_start, &cause_end, 10);
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            reason_param_itr++;
                                                        }
                                                        
                                                        // Check values on this line, This is a CANCEL
                                                        if (
                                                            (reason_sip_code == 1 && cause_number == 200) ||
                                                            (reason_sip_code == 0 && cause_number == 26)
                                                            ) {
                                                            dialog->callinfo.call_missed = 0;
//                                                            printf("\n\n########### THIS CALL WAS ANSWERED ELSEWHERE ###########\n\n");
                                                        } else { // This was a missed call!
//                                                            printf("\n\n*************** MISSED CALL *****************\n\n");
                                                            dialog->callinfo.call_missed = 1;
                                                        }
                                                        
                                                        
                                                        printf("\n");
                                                        JSTRING::freesplit(reason_params);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    
                                    /*****************
                                     If the Reason is Q.850;cause=16 (OR possibly if the
                                     "Reason" is NOT SIP;cause=200) then we set DIALOG::CALLIFO::call_missed
                                     to the value of 1 (one).
                                     
                                     Possibly analyze BYE messages also.
                                     
                                     Reason: SIP ;cause=200 ;text="Call completed elsewhere"
                                     Reason: Q.850 ;cause=16 ;text="Terminated"
                                     Reason: SIP ;cause=600 ;text="Busy Everywhere"
                                     Reason: SIP ;cause=580 ;text="Precondition Failure"
                                     
                                     
                                     Cause 16 appears to be THEY hung up.
                                     Cause 26 is another user answered
                                     
                                     * ANSWERED *
                                     Reason: SIP;cause=200;text="Call completed elsewhere"
                                     Reason: Q.850;cause=26
                                     
                                     
                                     * MISSED CALL *
                                     
                                     ------ deedleserver3.com ------
                                     CANCEL sip:317@208.54.86.250:57987;transport=tcp SIP/2.0
                                     Via: SIP/2.0/TCP 107.182.238.227:5060;rport;branch=z9hG4bKPj28074e97-ff71-41d0-b910-40657b24b2d3;alias
                                     From: "+16824336622" <sip:+16824336622@107.182.238.227>;tag=92fca4e9-b7fb-47dc-b556-776b8b779a9f
                                     To: <sip:317@208.54.86.250>
                                     Call-ID: 5e95e970-ebc4-4d41-b554-31ca3e2b1133
                                     CSeq: 5708 CANCEL
                                     Reason: Q.850;cause=0
                                     Max-Forwards: 70
                                     User-Agent: Asterisk PBX 15.4.0
                                     Content-Length:  0
                                     
                                     
                                     ------ deedleserver2.com ------
                                     CANCEL sip:317@208.54.86.250:27856;transport=tcp SIP/2.0
                                     Via: SIP/2.0/TCP 173.192.209.79:5060;branch=z9hG4bK409f9f14;rport
                                     Max-Forwards: 70
                                     From: "DFW-TAR" <sip:+19405777489@173.192.209.79>;tag=as7d321764
                                     To: <sip:317@208.54.86.250:27856;transport=tcp>
                                     Call-ID: 189af15c7000a6de4b1df1711efb55c0@173.192.209.79:5060
                                     CSeq: 102 CANCEL
                                     User-Agent: Asterisk PBX 15.5.0
                                     Content-Length: 0
                                     
                                     
                                    ******************/
                                    
                                    
									this->dialog_change_status(dialog, CANCELLING_CALL);
									this->request_terminated(dialog);
								} else {
									printf("***** Need to send a 481 Call Leg/Transaction Not Found !!!!!!  *******\n");
                                    this->response(sipmessage, 481);
								}
							} else if (JSTRING::matches(sip, (char *)"BYE")) {
								/* They hung up, we got a BYE */
								if (dialog) {
									this->dialog_change_status(dialog, HUNGUP);
									this->destroy_dialog(dialog->dialog_info.callid);
//                                    this->ok(sipmessage);
                                    this->response(sipmessage, 200);
								} else {
									printf("*** Got a BYE when we DIDN'T HAVE A DIALOG! SEND 481 ERROR! ****\n");
//                                    this->ok(sipmessage);
                                    this->response(sipmessage, 200);
								}

							} else if (JSTRING::matches(sip, (char *)"ACK")) {
								/* The incoming ACK means that THEY sent the INVITE */
								//sipmessage->show_message();
								if (dialog) {
									switch (dialog->callinfo.status) {
										case HANGING_UP:
										case CANCELLING_CALL: /* We send a 487 or a 603 in response to the INVITE */
											this->dialog_change_status(dialog, HUNGUP);
											this->destroy_dialog(dialog->dialog_info.callid);
											break;
										case ANSWERING: /* We sent a 200 OK in response to the invite */
											printf("*** Call \"%s\" has been ANSWERED.\n", dialog->dialog_info.callid);
											this->dialog_change_status(dialog, TALKING);
                                            if (dialog->callinfo.is_intercom == 1) {
                                                if (this->rtp) {
                                                    this->rtp->intercom_beep();
                                                }
                                            }
											break;
										default:
											break;
									}
								}


								/* ACK depends on the CALLINFO::status as to what we do from here */


							} else if (JSTRING::matches(sip, (char *)"MESSAGE")) {
                                if (dialog) {
                                    sipmessage->show_message();
                                    if (sipmessage->message.content_type == CT_TEXT) {
                                        if (sipmessage->message.content.rawcontent) {
//                                            printf("Call info to parse: %s\n", sipmessage->message.content.rawcontent);
                                            struct JSTRING::jsstring **message_parse = 0;
                                            message_parse = JSTRING::split(sipmessage->message.content.rawcontent, '~', (size_t)sipmessage->message.contentlength);
                                            if (message_parse) {
                                                int mp_itr = 0;
                                                int value_length = 0;
                                                char *value_ptr = 0;
//                                                printf("\n\n");
                                                for (; message_parse[mp_itr] != 0; mp_itr++) {
                                                    value_ptr = JSTRING::stringfield(message_parse[mp_itr]->ptr, '=', (int)message_parse[mp_itr]->length, 2, &value_length);
                                                    if (value_length > 0) {
                                                        if (JSTRING::matches((char *)"CID", message_parse[mp_itr]->ptr)) {
                                                            if (!dialog->callinfo.is_intercom) {
                                                                int send_change_alert = 0;
                                                                if (dialog->callinfo.did[0] == 0) { // No DID set.
                                                                    if (strncmp(this->user_info.callerid, value_ptr, value_length)) {
                                                                        send_change_alert = 1;
                                                                    }
                                                                    sprintf(dialog->callinfo.did, "%.*s", value_length, value_ptr);
                                                                } else {
                                                                    if (strncmp(dialog->callinfo.did, value_ptr, value_length)) {
                                                                        sprintf(dialog->callinfo.did, "%.*s", value_length, value_ptr);
                                                                        send_change_alert = 1;
                                                                    }
                                                                }
                                                                if (send_change_alert) { // DID change CID change
                                                                    this->dialog_change_status(dialog, dialog->callinfo.status, 1);
                                                                    printf("Setting \"My Number\" for this call as: \"%s\"\n", dialog->callinfo.did);
                                                                }
                                                            }
                                                        } else if (JSTRING::matches((char *)"Unique-ID", message_parse[mp_itr]->ptr)) {
                                                            if (!dialog->callinfo.is_intercom) {
                                                                int send_change_alert = 0;
                                                                if (dialog->callinfo.uniqueid[0] == 0) { // No Unique ID.
                                                                    if (value_length > 0) {
                                                                        sprintf(dialog->callinfo.uniqueid, "%.*s", ((value_length<200)?value_length:199), value_ptr);
                                                                        send_change_alert = 1;
                                                                    }
                                                                }
                                                                if (send_change_alert) { // DID change CID change
                                                                    this->dialog_change_status(dialog, dialog->callinfo.status, 1);
                                                                    printf("Setting the \"Unique-ID\" for this call as: \"%s\"\n", dialog->callinfo.uniqueid);
                                                                }
                                                            }

                                                        }
                                                    }
                                                }
//                                                printf("\n\n");
                                                JSTRING::freesplit(message_parse);
                                            }
                                        }
                                    }
                                } else {
                                    sipmessage->show_message();
                                }
                                this->response(sipmessage, 200);
							}
						}
					}





					/* We need to remove the variables associated with passing SIP messages to the application */

					/*
					if (pass_to_application) {
					if (this->message_stack_size == SIGNAL_MAX_SIP_STACK_SIZE) {
					for (i = 1; i < SIGNAL_MAX_SIP_STACK_SIZE; i++) {
					memmove(&this->message_stack[i - 1], &this->message_stack[i], sizeof(struct signal_message));
					}
					this->message_stack_size = SIGNAL_MAX_SIP_STACK_SIZE - 1;
					newmsgptr = &this->message_stack[this->message_stack_size];
					}
					else {
					newmsgptr = &this->message_stack[this->message_stack_size];
					}
					memset(newmsgptr, 0, sizeof(struct signal_message));
					newmsgptr->message = (char *)calloc((sipsize + 1), 1);
					newmsgptr->messagesize = (int)sipsize;
					newmsgptr->signal_message_received = time(0);
					memmove(newmsgptr->message, sip, sipsize);
					this->message_stack_size++;
					messages_processed++;
					}
					*/



					/* Clear this message off of the connection's buffer */
					s->buffersize -= (int)(sipsize + (sip - s->buffer));
					memmove(s->buffer, &sip[sipsize], s->buffersize);
					memset(&s->buffer[s->buffersize], 0, (SIGNAL_TCP_BUFFER_SIZE - s->buffersize));

					if (sipmessage != this->registration) {
						if (sipmessage) {
							int free_message = 0;
							if (!dialog) free_message = 1;
							if (dialog) {
								if (dialog->primary != sipmessage) {
									free_message = 1;
								}
							}
							if (free_message) {
								//MEMORY::deleted(sipmessage);
								delete sipmessage;
							}
						}
					}


				} else {
					/* The SIP Message in the connection buffer was WAY too large! > 5000 bytes! */
					OutputDebugString("signalling.cpp > SIGNAL::pullSIPoffstack(): We got a sip message from a connection that was > 50KBs. We're dropping it.\n");
					s->buffersize -= (int)(sipsize + (sip - s->buffer));
					memmove(s->buffer, &sip[sipsize], s->buffersize);
					memset(&s->buffer[s->buffersize], 0, (SIGNAL_TCP_BUFFER_SIZE - s->buffersize));
				}
			} else {
				/* We're breaking because this isn't a complete SIP message. */
				break;
			}
		}
//        this->check_dialogs();
		sip = this->getnextSIP(s->buffer);
	} while (sip);
	if (messages_processed > 0) {
		sprintf(debug, "signalling.cpp > SIGNAL::pullSIPoffstack(): %d SIP messages queued for retrieval.\n", messages_processed);
		OutputDebugString(debug);
	}
	return messages_processed;
}

#ifdef __APPLE__
void *signal_thread(void *lParam) {
#elif _WIN32
DWORD WINAPI signal_thread(void *lParam) {
#endif
	SIGNAL *signal = (SIGNAL *)lParam;

	/* Pointer to dynamically allocated array of SIP messages to be retrieved */
	//struct signal_message *signal_messages = 0;

	/* Number of SIP messages in queue */
	//int signal_message_count = 0;

	time_t looptimer = 0, retry_connect = 0;

	size_t bytesread = 0;//, bufferbytesremaining = 0;


	int ibytessent = 0, totalbytessent = 0, sendretries = 0;
	int messages_sent = 0, takeabreak = 1;
	struct SIGNAL::_send_queue *sq = 0;
	struct SIGNAL::_send_queue *sqnew = 0;
	DIALOG *dialog = 0;

	//int oktorun = 1, i = 0, j = 0;
	int i = 0;
	char temprxbuffer[SIGNAL_TCP_BUFFER_SIZE];
	char debug[500];
	/* Variables for analyzing inbound packet */
	/*
	char *requestmethod = 0, *lineend = 0, *foundrequest = 0;
	char *responsestart = 0, *foundresponse = 0,
	*statuscode = 0, *statuscodeend = 0, *stopptr = 0,
	*current_working_ptr = 0;
	*/
	char *sip = 0;
    
//    printf("****** STARTING SIGNAL_THREAD() *********\n");

//    printf("Starting loop in signal_thread()\n");
	while (signal->signal_thread_running) {
		takeabreak = 1;
		looptimer = time(0);
		signal->lock();
		if (!signal->connected) { // Connect here!
			
			if (retry_connect < looptimer) {
				if (!signal->shuttingdown) {
//                    printf("Calling SIGNAL::voipconnect()\n");
					signal->voipconnect();
					if (signal->connected) {
						retry_connect = 0;
					} else {
						OutputDebugString("signal_thread(): signal->voipconnect() failed.  We'll try again in 2 seconds...\n");
						retry_connect = time(0) + 2;
						takeabreak = 1;
					}
				}
			}

			/* We're not connected here */
			// Possibly go ahead and send a SIP message back to the application so that it stops
			// trying to contact the server until we're connected?

		} else /* We're connected */ {
			sq = 0;
			sqnew = 0;
			messages_sent = 0;
			/*sendretries = 0;
			totalbytessent = 0;
			ibytessent = 0;*/


			if (signal->send_queue_count) takeabreak = 0;
			/* Send out all pending requests send data */
			for (i = 0; i < signal->send_queue_count; i++) {
				ibytessent = 0;
				totalbytessent = 0;
				sendretries = 0;
				//sprintf(debug, "signal->send_queue_count=%d,\t Sending #%d\n", signal->send_queue_count, i);
				//OutputDebugString(debug);

				sq = &signal->send_queue[i];

				while (totalbytessent < sq->len) {
					signal->psocket->last_connection_check = looptimer;
					ibytessent = (int)db_send(signal->psocket->s, &sq->send_buffer[ibytessent], sq->len - ibytessent, 0);
					/*
					sprintf(debug, "signalling.cpp > signal_thread(): send %d bytes.\n", ibytessent);
					OutputDebugString(debug);
					*/
					if ( /* We sucessfully sent some data. */  ibytessent > 0) {
						totalbytessent += ibytessent;
					} else /* No data was sent */ {
						if (sendretries++ > 5) {
							sprintf(debug, "signalling.cpp > signal_thread(): Failed to data, stack position: %d\n", i);
							OutputDebugString(debug);
							OutputDebugString(sq->send_buffer);
							OutputDebugString("*********************************************\n\n");
							sq->last_send_attempt = time(0);
							totalbytessent = 0;
							// sending data
							break;
						}
						Sleep(100);
					}
				}



				/* Done trying to send message "i" from send_queue */

				if (/* This last message failed to send.  Stop trying for now */ totalbytessent == 0) {
					if (!signal->shuttingdown) signal->voipconnect();
					retry_connect = 0;
					break;
				} else {
//                    sprintf(debug, "\n\n**** SENT %d bytes***********************************************\n", totalbytessent);
//                    OutputDebugString(debug);
//                    OutputDebugString(sq->send_buffer);
//                    OutputDebugString("**********************************************************\n\n");
					//MEMORY::free_(sq->send_buffer);
					free(sq->send_buffer);
					sq->send_buffer = 0;
					messages_sent++;
				}
			}


			if ( /* All messages were sent */  messages_sent == signal->send_queue_count && signal->send_queue_count > 0) {
				//OutputDebugString("Send queue is now empty.\n");
				//MEMORY::free_(signal->send_queue);
				free(signal->send_queue);
				signal->send_queue_count = 0;
				signal->send_queue = 0;
			} else {
				if (signal->send_queue_count > 0) {
					memmove(signal->send_queue, &signal->send_queue[messages_sent], sizeof(struct SIGNAL::_send_queue) * (signal->send_queue_count - messages_sent));
					signal->send_queue_count -= messages_sent;
					//sqnew = (struct SIGNAL::_send_queue *)MEMORY::realloc_(signal->send_queue, (signal->send_queue_count * sizeof(struct SIGNAL::_send_queue)));
					sqnew = (struct SIGNAL::_send_queue *)realloc(signal->send_queue, (signal->send_queue_count * sizeof(struct SIGNAL::_send_queue)));
					if (/* The OS failed to reallocate our memory */ sqnew == 0) {
						OutputDebugString("signalling.cpp > signal_thread(): realloc() failed after sending messages from \"send_queue\"\n");
					} else /* All is good */ {
						signal->send_queue = sqnew;
					}
				}
			}






			/* We're connected. Here do periodic things that need doing with an active connection */


			if (looptimer - signal->psocket->last_response > 15) {
				OutputDebugString("signalling.cpp > signal_thread(): It's been 15 seconds since we've heard from the server.  Starting new connection.\n");
				if (!signal->shuttingdown) signal->voipconnect();
			} else if (!signal->isregistered() && (looptimer - signal->psocket->last_response > 5)) {
				/* If we're NOT REGISTERed, then go through our process every 5 seconds */
                if (looptimer - signal->psocket->last_connection_check > 1) {
                    if ( (signal->registration_info.failed_attempts == 0) ||
                        (signal->registration_info.failed_attempts > 0 && (time(0) - signal->registration_info.last_registration_attempt > 5))) {
                        signal->options();
                        signal->psocket->last_connection_check = looptimer;
                    }
                }
			} else if (looptimer - signal->psocket->last_response > 10) {
				if (looptimer - signal->psocket->last_connection_check > 1) {
					signal->options();
					signal->psocket->last_connection_check = looptimer;
				}
			}





			/* Check our registration */



			/*
			Check and parse incoming SIP messages to prep for our SIP stack to handle
			- while doing this -
			Look into the parsed SIP messages and make sure the server's "rport" still matches the IP
			address and port we have cached.  If it DOES, then great.  If NOT, we will actually need to
			do something about the *already built* SIP messages in the outbound queue.  Maybe have the application
			rebuild and resubmit them?
			*/




			/* Check the status of outbound SIP messages.  Send the ones that need sending, see if we need
			to resend any, etc...*/

			/* Go through sockets.  Read and check their statuses */
			for (i = 0; i < 10; i++) {
				//sprintf(debug, "Checking socket %d.\n", i);
				//OutputDebugString(debug);
				if (signal->_socket[i].s) {
					//OutputDebugString("\tSocket valid...reading it...\n");
					/* We have a valid socket descriptor */

					/* Read everything from this socket */
					// size_t bytesread = 0, bufferbytesremaining = 0;



					memset(temprxbuffer, 0, SIGNAL_TCP_BUFFER_SIZE);
					bytesread = recv(signal->_socket[i].s, temprxbuffer, SIGNAL_MAX_BUFFER_SIZE, 0);
					if (bytesread == 0) {
						/* socket was closed */
						OutputDebugString("\tThis socket was closed..weird...\n");
						if (&signal->_socket[i] == signal->psocket) {
							if (!signal->shuttingdown) signal->voipconnect();
							retry_connect = 0;
						} else {
							shutdown(signal->_socket[i].s, 2);
							closesocket(signal->_socket[i].s);
							memset(&signal->_socket[i], 0, sizeof(struct SIGNAL::__socket));
						}
					} else if (bytesread == -1) {
						/* See what's up */
						//bytesread = 0;
#ifdef __APPLE__
						switch (errno) {
                            case EINPROGRESS:
							case EWOULDBLOCK:
								break;
							default:
								if (&signal->_socket[i] == signal->psocket) {
									if (!signal->shuttingdown) signal->voipconnect();
									retry_connect = 0;
								}
								break;
						}
#elif _WIN32
						switch (WSAGetLastError()) {
							case WSAEWOULDBLOCK: /* EWOULDBLOCK */
												 // Do nothing.  There's just no data to be read right now.
								break;
							default:
								if (&signal->_socket[i] == signal->psocket) {
									if (!signal->shuttingdown) signal->voipconnect();
									retry_connect = 0;
								}
								break;
						}
#endif



					} else if (bytesread > 0) { /* data recd got data new data*/
												/* We got a little data huhruh */
						takeabreak = 0;

						//sprintf(debug, "%d bytes read on connection # %d.\n\n", bytesread, i);
						//OutputDebugString(debug);
						//                        printf("%.*s", bytesread, temprxbuffer);

						if (JSTRING::matches(temprxbuffer, (char *)"\r\n") && bytesread == 2) {
//                            OutputDebugString("*** SIP TCP KEEPALIVE RESPONSE RECEIVED ***\n");
						} else if (JSTRING::matches(temprxbuffer, (char *)"\r\n\r\n") && bytesread == 4) {
//                            OutputDebugString("*** SIP TCP KEEPALIVE REQUEST RECEIVED ***\n");
							if (signal->psocket) {
								db_send(signal->psocket->s, (char *)"\r\n", 2, 0);
							}
						}
						sip = signal->getnextSIP(temprxbuffer);
						char *this_connections_sip = signal->getnextSIP(signal->_socket[i].buffer);
						if ( /* 1. There is SIP in this packet */ sip) {
							//OutputDebugString("This packet contains SIP.\n");

							if ( /* 1. There is SIP in this packet,
								 2. The SIP in this buffer is NOT at the start of the packet received
								 meaning it could be to finish up a SIP message that is paritally
								 buffered. */ sip != temprxbuffer) {

								if (/*  1. There is SIP in this packet,
									2. The SIP in this packet is NOT at the start of the packet received
									meaning it could be to finish up a SIP message that is paritally
									buffered.
									3. There is SIP in this connection's buffer */ this_connections_sip) {

									/*
									Questions now: Is the SIP buffered and aligned with the START of the buffer, or
									is there superfluous data buffered at the start that we need to clear out?
									*/

									if ( /* 1. There is SIP in this packet,
										 2. The SIP in this packet is NOT at the start of the packet received
										 meaning it could be to finish up a SIP message that is paritally
										 buffered.
										 3. There is SIP in this connection's buffer
										 4. This connection's buffer STARTS with a SIP message. Yay! */
										this_connections_sip == signal->_socket[i].buffer) {

										/*
										The data buffered on this connection is all intact.  It looks like it's a
										SIP message waiting for completion.
										*/

										// work here

										if (signal->_socket[i].buffersize + bytesread < SIGNAL_MAX_BUFFER_SIZE) {
											/* New data will fit.  Buffer it. */
											memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize], temprxbuffer, bytesread);
											signal->_socket[i].buffersize += (int)bytesread;
										} else {
											/* We will overflow our buffer if we add this into it. */
											/* Steps: */
											/*
											1. Buffer the data UP TO the SIP in the packet
											2. Call signal->pullSIPoffstack() and check if it removed
											a message from the connection buffer, freeing up enough
											space to continue buffering.
											A. It did free up buffer space:
											i. Buffer and be done.
											ii. Drop the oldest information from the
											buffer and just buffer the newest SIP message.
											*/
											if (signal->_socket[i].buffersize + (sip - temprxbuffer) < SIGNAL_MAX_BUFFER_SIZE) {
												/* Buffer data from "temprxbuffer" up to "sip" */
												memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize], temprxbuffer, (sip - temprxbuffer));
												signal->_socket[i].buffersize += (int)(sip - temprxbuffer);
												bytesread -= (sip - temprxbuffer);
												signal->pullSIPoffstack(&signal->_socket[i]);

												if (signal->_socket[i].buffersize + bytesread < SIGNAL_MAX_BUFFER_SIZE) {
													/*
													If the remaining SIP message data in this packet will now fit into the connection buffer
													*/
													memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize],
														sip, bytesread);
													signal->_socket[i].buffersize += (int)bytesread;
												} else {
													memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
													memmove(signal->_socket[i].buffer, sip, bytesread);
													signal->_socket[i].buffersize = (int)bytesread;
												}
											} else {
												/* Buffering even this data will overflow the connection buffer. */
												/* Drop the long connection buffer and move everything from "sip" on into the
												connection buffer.
												*/
												signal->_socket[i].buffersize = 0;
												memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
												memmove(signal->_socket[i].buffer, sip, (bytesread - (sip - temprxbuffer)));
											}
										}
									} else /* This connection's buffer has SIP, but doesn't start with it,  */ {
										/*
										There is SIP waiting for completion in this connection's buffer, but there's some gibberish
										before it.  We need to move the data from "this_connecions_sip", up to the front of the buffer,
										to "signal->_socket[i].buffer", then add the data from "temprxbuffer" (bytesread) to the end of
										this connection's buffer if it will fit.
										*/

										signal->_socket[i].buffersize -= (int)(this_connections_sip - signal->_socket[i].buffer);
										memmove(signal->_socket[i].buffer, this_connections_sip, signal->_socket[i].buffersize);
										if (signal->_socket[i].buffersize + (sip - temprxbuffer) < SIGNAL_MAX_BUFFER_SIZE) {
											/* Buffer data from "temprxbuffer" up to "sip" */
											memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize], temprxbuffer, (sip - temprxbuffer));
											signal->_socket[i].buffersize += (int)(sip - temprxbuffer);
											bytesread -= (sip - temprxbuffer);
											signal->pullSIPoffstack(&signal->_socket[i]);

											if (signal->_socket[i].buffersize + bytesread < SIGNAL_MAX_BUFFER_SIZE) {
												/*
												If the remaining SIP message data in this packet will now fit into the connection buffer
												*/
												memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize],
													sip, bytesread);
												signal->_socket[i].buffersize += (int)bytesread;
											} else {
												memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
												memmove(signal->_socket[i].buffer, sip, bytesread);
												signal->_socket[i].buffersize = (int)bytesread;
											}
										} else {
											/* Buffering even this data will overflow the connection buffer. */
											/* Drop the long connection buffer and move everything from "sip" on into the
											connection buffer.
											*/
											signal->_socket[i].buffersize = 0;
											memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
											memmove(signal->_socket[i].buffer, sip, (bytesread - (sip - temprxbuffer)));
										}

									}
								} else /* Just received data has SIP, but doesn't start with it, and there is no SIP in this connection's buffer */ {
									/*
									No, this connection is not waiting for any more data to complete a SIP message, it
									appears we just receive some crappy out-of-band data..Let's ignore it and start buffering
									based on the start of our recognizable SIP.
									*/
									/* Here, we'll move data from "sip" ( (bytesread - (sip-temprxbuffer)) bytes) to "signal->_socket[i].buffer" and set
									"signal->_socket[i].buffersize" to equal bytesread - (sip-temprxbuffer);
									*/
									// work here

									memmove(signal->_socket[i].buffer, sip, bytesread - (sip - temprxbuffer));
									signal->_socket[i].buffersize = (int)(bytesread - (sip - temprxbuffer));
								}
							} else /* 1. The start of temprxbuffer is the start of a SIP message. */ {
								/* This packet is the start of a SIP message. */
								// work here
								memmove(signal->_socket[i].buffer, temprxbuffer, bytesread);
								signal->_socket[i].buffersize = (int)bytesread;
							}
						} else /* There is NO SIP in this packet */ {
							if ( /* There is SIP in this connection's buffer */ this_connections_sip) {

								if ( /* The SIP in the connection buffer is NOT at the beginning */ this_connections_sip != signal->_socket[i].buffer) {
									/* Move the SIP we found to the head of the buffer */
									signal->_socket[i].buffersize -= (int)(this_connections_sip - signal->_socket[i].buffer);
									memmove(signal->_socket[i].buffer, this_connections_sip, signal->_socket[i].buffersize);

									/* Zero out the balance of this connection's memory */
									memset(signal->_socket[i].buffer, 0, (SIGNAL_TCP_BUFFER_SIZE - signal->_socket[i].buffersize));
								}
								if ((bytesread + signal->_socket[i].buffersize) < SIGNAL_TCP_BUFFER_SIZE) {
									memmove(&signal->_socket[i].buffer[signal->_socket[i].buffersize], temprxbuffer, bytesread);
									signal->_socket[i].buffersize += (int)bytesread;
								} /* This NON-SIP containing packet added with the SIP packet in the buffer exceeds the maximum allowed TCP size */ else {
									/* Clear this connection's buffer and don't do anything with this data. */
									if (signal->_socket->buffersize > 0) {
										memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
										signal->_socket[i].buffersize = 0;
									}

								}
							} else /* We received a packet with NO SIP, and there is NO SIP buffered on this connection */ {
								if (/* There is data buffered on this connecion anyways */ signal->_socket[i].buffersize > 0) {
									/*
									There IS data buffered.  So now, something is trying
									to fill this connection's buffer with unintelligible crap.
									*/
									signal->_socket[i].buffersize = 0;
									memset(signal->_socket[i].buffer, 0, SIGNAL_TCP_BUFFER_SIZE);
								} else /* 1. We received a packet containing NO SIP
									   2.There is NO DATA buffered on this connection. */ {
									   /* We need to do nothing.  Ignore this packet. */

								}
							}
						}

						/*
						Move any complete SIP messages from this connection to the message_stack for
						retrieval by the application.
						*/
						signal->pullSIPoffstack(&signal->_socket[i]);
						/* Update the time at which we last rec'd data on this socket */
						signal->_socket[i].last_response = looptimer;


					}






					if (/* This socket is scheduled for destruction */ signal->_socket[i].destroy_at_time > 0) {
						/* This socket is scheduled for destruction */
						if ( /* Time is up for this socket */ looptimer > signal->_socket[i].destroy_at_time) {
							if ( /* But wait! We've received data on this socket since destruction was scheduled */ signal->_socket[i].last_response > (looptimer - 120)) {
								/*
								Data was rec'd on this socket within the last two
								minutes EVEN THOUGH it was scheduled for destruction.
								So.
								We will POSTPONE its scheduled destruction time to
								120 seconds after the last bit of data was rec'd.
								*/
								signal->_socket[i].destroy_at_time =
									(signal->_socket[i].last_response + 120);
							} else /* No data rec'd in last 2 mins. Destroy it. */ {
								/* Rev'd no data within the last two minutes */
								sprintf(debug, "signalling.cpp > signal_thread() > Destroying socket %d.\n", i);
								takeabreak = 0;
								OutputDebugString(debug);
								shutdown(signal->_socket[i].s, 2);
								closesocket(signal->_socket[i].s);
								memset(&signal->_socket[i], 0, sizeof(struct SIGNAL::__socket));
								if (signal->psocket == &signal->_socket[i]) {
									signal->connected = 0;
									signal->psocket = 0;
								}
							}
						}
					}
				} // if (signal->_socket[i].s)
			} // for (i = 0; i < 10; i++)


			  /* Check on the status of our dialogs */
			db_lock_mutex(&signal->dialog_mutex);
			for (i = 0; i < MAX_DIALOG_COUNT; i++) {
				dialog = &signal->dialog_list[i];
				if (!dialog->inuse) continue;
				if ( /* This dialog is not ready to be destroyed */
					time(0) < dialog->destroy_after || dialog->destroy_after == 0) {

					switch (dialog->callinfo.status) {
						case HUNGUP:
							if ( /* This dialog has been HUNGUP for over TWENTY seconds */ time(0) - dialog->last_status_change > 20) {
								signal->abort_dialog(dialog);
							}
							break;
						case HANGING_UP:
							/* 
								This status is ok - we don't have to do anything as long as
								WE have recently sent a dialog-ending message i.e. CANCEL or BYE
								
								We should be courteous and at least attempt to send one more before destroying
								this dialog.

							*/
							break;
						case RINGING_IN:
							if ( /* It's been ringing for less than MAX_RING_TIMEOUT */ time(0) - dialog->dialog_created_time < MAX_RING_TIMEOUT) {
								if ( /* It's been over six seconds since we sent our last RINGING */ time(0) - dialog->last_ring > 6) {
									signal->ringing(dialog);
									dialog->last_ring = time(0);
								}
							} else /* It's been ringing for OVER MAX_RING_TIMEOUT... */ {
								printf("* This call has been RINGING_IN for over %d seconds, hanging up.\n", MAX_RING_TIMEOUT);
								signal->hangup(dialog->dialog_info.callid);
							}
							break;
						case TRANSFERRING:
						case RINGING_OUT:
							if ( /* It's been ringing out for more than MAX_RING_TIMEOUT seconds */ time(0) - dialog->dialog_created_time > MAX_RING_TIMEOUT) {
								printf("* This call has been RINGING_OUT for over %d seconds, sending BUSY.\n", MAX_RING_TIMEOUT);
								signal->hangup(dialog->dialog_info.callid);
							}
							break;
						case ANSWERING: /* We've sent a 200 OK to their INVITE and have NOT received an ACK */
							if ( /* Five seconds is too long to try to answer a call */ time(0) - dialog->last_status_change > 5) {
								signal->hangup(dialog->dialog_info.callid);
							}
							break;
						case TALKING:
							break;
						case ONHOLD:
							break;
						case CANCELLING_CALL:
							if (time(0) - dialog->last_status_change > 5) {
								signal->abort_dialog(dialog);
							}
							break;
						case CONFIRMING:
							break;
						case SUBSCRIPTION:
							break;
						default: /* A reason we haven't prepared for or UNKNOWN? */
							break;

					}
				} else /* This dialog is ready for destruction destroy dialog*/ {
					//printf("* Destroying dialog for call id: %s\n", dialog->dialog_info.callid);
					if (dialog->primary) {
						delete dialog->primary;
					} else {
						printf(" ** WARNING ** We destroyed a dialog that had a NULL pointer in DIALOG::primary **\n");
					}
					memset(dialog, 0, sizeof(DIALOG));
				}
			}
			db_unlock_mutex(&signal->dialog_mutex);







			if (looptimer > signal->timetoreregister()) {
//                OutputDebugString("\n\n <<<< BEGINNING re-REGISTERation NOW >>>> \n\n");
				signal->sipregister(signal->psocket, REGISTRATION_EXPIRATION);
			}
		}
		signal->unlock(); /* Do NOT call private class methods or change private vars below here!! */

		if (takeabreak) {
			Sleep(50);
		}
	}
	signal->_close();
//    OutputDebugString("signalling.cpp > signal::signal_thread > Exiting thread.\n");
	return 0;
}
_sip::_sip() {
	this->isparsed = 0;
	this->message.timestamp = 0;
	this->message.headers = 0;
	this->message.headercount = 0;
	memset(this->pmessage, 0, MAX_SIP_MESSAGE_SIZE);
	this->message.statuscode = 0;
}

_sdp::_sdp() {
	this->audio_port = 0;
	this->audio_address[0] = 0;
	this->incoming_ssrc = 0;
	this->ulaw_offered = 0;
	this->audio_direction = AD_INACTIVE;
	memset(&this->audio_destination, 0, sizeof(struct sockaddr_in));
}

_sdp::~_sdp() {
	return;
}


/* URI Class */
void _sip_uri::init(char *uri, size_t len) {
	//SIPSTRING *sstemp = 0;
	SIPSTRING sipuri;
	//SIPSTRING tag;
	char *tagstring = 0;
	int tagstringlen = 0;
	char *portstring = 0;
	char *ipstart = 0;
	char *uriparameters = 0;
	size_t portstringlen = 0, iplength = 0;
	this->_uri.string = (char *)SIGNAL::empty;
	this->_uri.length = 0;
	this->_uriparameters.length = 0;
	this->_uriparameters.string = 0;
	this->_extension.length = 0;
	this->_ipaddress.length = 0;
	this->_port.length = 0;
	this->_tag.length = 0;
	this->_username.length = 0;
	this->_extension.string = (char *)SIGNAL::empty;
	this->_ipaddress.string = (char *)SIGNAL::empty;
	this->_port.string = (char *)SIGNAL::empty;
	this->_tag.string = (char *)SIGNAL::empty;
	this->_username.string = (char *)SIGNAL::empty;

	if (!uri) return;

	this->_uri.string = JSTRING::trim(uri, len, &this->_uri.length);
	this->_username.string = JSTRING::stringfield(uri, '<', len, 1, &this->_username.length);
	if ( /* There is a QUOTE in the string */
		JSTRING::haschar(this->_username.string, '"', this->_username.length)) {
		this->_username.string = JSTRING::between(this->_username.string, "\"", &this->_username.length);
		this->_username.string = JSTRING::trim(this->_username.string, &this->_username.length, &this->_username.length);
	}
	sipuri.string = JSTRING::between(uri, "<>", &sipuri.length);

	/* Parse "sipuri" which will now contain "sip:317@xxx.xxx.xxx.xxx:yyyy[;transport=tcp]" */
	if (JSTRING::haschar(sipuri.string, '@')) {
		this->_extension.string = JSTRING::between(sipuri.string, ":@", &this->_extension.length);
		ipstart = JSTRING::stringfield(sipuri.string, '@', '>', 2, &iplength);
	} else {
		ipstart = JSTRING::stringfield(sipuri.string, ':', '>', 2, &iplength);
	}
	if (JSTRING::charpos(ipstart, ':', '>') > 0) {
		/* there is a port included */
		this->_ipaddress.string = JSTRING::between(sipuri.string, "@:", &this->_ipaddress.length);
		portstring = JSTRING::stringfield(this->_ipaddress.string, ':', '>', 2, &portstringlen);
		if (JSTRING::haschar(portstring, ';', portstringlen)) {
			/* There are attributes after the port*/
			this->_port.string = JSTRING::stringfield(portstring, ';', 1, &this->_port.length);
		} else {
			this->_port.string = JSTRING::stringfield(portstring, '>', 1, &this->_port.length);
		}
	} else {
		if (this->_extension.length > 0) {
			this->_ipaddress.string = JSTRING::between(sipuri.string, "@>", &this->_ipaddress.length);
		} else {
			this->_ipaddress.string = JSTRING::stringfield(ipstart, '>', 1, &this->_ipaddress.length);
		}
		this->_port.string = 0;
		this->_port.length = 0;
	}
	if (this->_port.length > 0) {
		uriparameters = &this->_port.string[this->_port.length];
	} else if (this->_ipaddress.length > 0) {
		uriparameters = &this->_ipaddress.string[this->_ipaddress.length];
	}
	if (uriparameters) {
		if (uriparameters[0] == ';') {
			this->_uriparameters.string =
				JSTRING::stringfield(uriparameters, '>', 1, &this->_uriparameters.length);
		}
	}


	if (JSTRING::haschar(sipuri.string, ';', sipuri.length)) {
		/*We'll ignore all ";" delimited parameters. */

	}
	/* Get tag */
	tagstring = JSTRING::stringfield(uri, '>', '\n', 2, &tagstringlen);

	if (tagstring) {
		this->_tag.string =
			JSTRING::trim(
				JSTRING::stringfield(tagstring, '=', '\n', 2, &this->_tag.length),
				&this->_tag.length,
				&this->_tag.length);
	}

}

_sip_uri::_sip_uri(char *username, char *ipaddress, unsigned short port, char *tag) {

}

_sip_uri::_sip_uri(char *ipaddress, unsigned short port, char *tag) {

}

_sip_uri::_sip_uri(char *ipaddress, unsigned short port) {

}
_sip_uri::_sip_uri(char *username, struct sockaddr_in *ipaddr, unsigned short port, char *tag) {

}
_sip_uri::_sip_uri(struct sockaddr_in *ipaddr, unsigned short port, char *tag) {

}
_sip_uri::_sip_uri(struct sockaddr_in *ipaddr, unsigned short port) {

}

_sip_uri::_sip_uri(char *uri, size_t len) {
	this->init(uri, len);
}

_sip_uri::_sip_uri(char *uri) {
	if (!uri) return;
	this->init(uri, strlen(uri));
}


_sip_uri::~_sip_uri() {

}

char *_sip_uri::username(void) {
	static char un[500]; // *un = 0;
	if (this->_username.length > 0) {
		memset(un, 0, 500);
		memcpy(un, this->_username.string, this->_username.length);
		return un;
	}
	un[0] = 0;
	return un;
}

char *_sip_uri::username(char *val) {
	return (char *)SIGNAL::empty;
}
char *_sip_uri::extension(void) {
	static char un[500]; // *un = 0;
	if (this->_extension.length > 0) {
		memset(un, 0, 500);
		memcpy(un, this->_extension.string, this->_extension.length);
		return un;
	}
	un[0] = 0;
	return un;
}

unsigned long long _sip_uri::iextension(void) {
	char *start = 0, *end = 0;
	unsigned long long extension = 0;
	if (this->_extension.length > 0) {
		start = this->_extension.string;
		if (start[0] == '+') {
			start = &start[1];
		}
		end = &this->_extension.string[this->_extension.length];
		extension = strtoull(start, &end, 10);
	}
	return extension;
}

char *_sip_uri::extension(char *val) {
	return (char *)SIGNAL::empty;
}
char *_sip_uri::ipaddress(void) {
	static char un[500]; // *un = 0;
	if (this->_ipaddress.length > 0) {
		memset(un, 0, 500);
		memcpy(un, this->_ipaddress.string, this->_ipaddress.length);
		return un;
	}
	un[0] = 0;
	return un;
}
char *_sip_uri::ipaddress(char *val) {
	return (char *)SIGNAL::empty;
}
char *_sip_uri::port(void) {
	static char un[500]; // *un = 0;
	if (this->_port.length > 0) {
		memset(un, 0, 500);
		memcpy(un, this->_port.string, this->_port.length);
		return un;
	}
	un[0] = 0;
	return un;
}

char *_sip_uri::port(char *val) {
	return (char *)SIGNAL::empty;
}

char *_sip_uri::tag(void) {
	static char un[500]; // *un = 0;
	if (this->_tag.length > 0) {
		memset(un, 0, 500);
		memcpy(un, this->_tag.string, this->_tag.length);
		return un;
	}
	un[0] = 0;
	return un;
}

char *_sip_uri::tag(char *val) {
	return (char *)SIGNAL::empty;
}

char *_sip_uri::uri(void) {
	static char un[500]; // *un = 0;
	memset(un, 0, 500);
	if (this->_username.length > 0) {
		sprintf(un, "\"%.*s\" <sip:", (int)this->_username.length, this->_username.string);
	} else {
		sprintf(un, "<sip:");
	}
	if (this->_extension.length > 0) {
		strncat(un, this->_extension.string, this->_extension.length);
		strcat(un, "@");
	}
	if (this->_ipaddress.length > 0) {
		strncat(un, this->_ipaddress.string, this->_ipaddress.length);
	} else {
		strcat(un, "localhost");
	}
	if (this->_port.length > 0) {
		strcat(un, ":");
		strncat(un, this->_port.string, this->_port.length);
	}
	if (this->_uriparameters.length > 0) {
		strncat(un, this->_uriparameters.string, this->_uriparameters.length);
	}
	strcat(un, ">");
	if (this->_tag.length > 0) {
		strcat(un, ";tag=");
		strncat(un, this->_tag.string, this->_tag.length);
	}
	return un;
}


