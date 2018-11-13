// signal.h (c)2018 Justin Jack
// deedlecore project - libsignal.a / libsignal.lib
//
#pragma once
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _WINSOCKAP_
#define _WINSOCKAP_
#endif // !_WINSOCKAP_

#define REGISTRATION_EXPIRATION 300

#define DEEDLEBOX_VERSION "2.0"

#define MAX_SIP_MESSAGE_SIZE 5000

#define MAX_DIALOG_COUNT 100

#define MAX_RING_TIMEOUT 180

//#define SIGNAL_SHOW_MY_SIP 1


#ifdef SIGNAL_SHOW_MY_SIP
#define showsip(x) printf("*** SENDING ****\n%s\n***************************\n\n", x)
#else
#define showsip(x) 0
#endif

/* Make sure "windows.h" isn't included before here*/

#include <stdio.h>
#ifdef _WIN32
#include <db_x_platform.h>
#else
#include "/Users/justinjack/Documents/Deedle Cross Platform Library/db_x_platform.h"
#endif
#ifdef _WIN32
#include <rtp.hpp>
#else
#include "/Users/justinjack/Documents/RTP Library/rtp.hpp"
#endif

#ifdef _WIN32
#include <Ws2tcpip.h>
#include <WinInet.h>
#include <IPHlpApi.h>
#endif

#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#include "jstring.h"
#include "md5.h"


#ifndef OutputDebugString
#define OutputDebugString(x) printf("%s", x)
#endif

/*
This is the number of registrations that we will maintain.  It will also be representative
of the number of ACTIVE connections through which signalling will pass.
*/
#define SIGNAL_ACTIVE_REGISTRATION_COUNT 3
#define SIGNAL_ERROR -1
#define SIGNAL_MAX_SIP_STACK_SIZE 100
#define SIGNAL_TCP_BUFFER_SIZE 65536
#define SIGNAL_MAX_BUFFER_SIZE (SIGNAL_TCP_BUFFER_SIZE-1)
#define SIGNAL_FAIL(x) {this->last_message=x; return SIGNAL_ERROR;}
#ifndef LOWER
#define LOWER(x) ((x >= 'A' && x <= 'Z')?(x+32):x)
#endif
//#define EMPTY (char *)SIGNAL::empty;


typedef enum _CONTENT_TYPES {
	CT_UNKNOWN,
	CT_SDP,
	CT_SIPFRAG,
	CT_TEXT,
	CT_HTML,
	CT_SIMPLE_MESSAGE_SUMMARY
} CONTENT_TYPES;




typedef enum _CALL_STATUS {
	HUNGUP,
	HANGING_UP,
	RINGING_IN,
	RINGING_OUT,
	ANSWERING,
	TALKING,
	ONHOLD,
	TRANSFERRING,
	CANCELLING_CALL,
	CONFIRMING,
	SUBSCRIPTION,
    PUTTING_ONHOLD,
	UNKNOWN
} CALL_STATUS;


typedef enum _voip_protos {
	VOIP_SIP,
	VOIP_BODY
} VOIP_PROTOCOLS;

typedef enum {
	SENDAUTH,
	RESTART
} SIPREGISTERSTEP;

typedef enum sip_header_type {
	SIG_VIA,
	SIG_FROM,
	SIG_TO,
	SIG_CONTACT,
	SIG_CSEQ,
	SIG_CALLID,
	SIG_ALLOWEVENTS,
	SIG_CONTENTLENGTH,
	SIG_CONTENTTYPE,
	SIG_WWWAUTHENTICATE,
	SIG_SUPPORTED,
	SIG_USERAGENT,
	SIG_SERVER,
	SIG_MAXFORWARDS,
	SIG_ALLOW,
	SIG_ACCEPT,
	SIG_CUSTOM,
	SIG_EXPIRES,
	SIG_DIVERSION,
	SIG_P_ASSERT_ID,
	SIG_UNKNOWN
} SIP_HEADER_TYPE;

typedef struct _sipstring {
	char *string;
	size_t length;
} SIPSTRING;



typedef enum _AUDIO_DIRECTION {
	AD_INACTIVE,
	AD_SENDRECV,
	AD_SENDONLY,
	AD_RECVONLY
} AUDIO_DIRECTION;


/*
SIPSTRING *trim(char *string);
SIPSTRING *trim(char *string, size_t len);
int trim(char *string, size_t len, SIPSTRING *ss);
int haschar(char *string, char testchar, int len);
int haschar(char *string, char testchar, size_t len);
char *between(char *string, const char *frame, int *length);
static char *stringfield(char *string, const char delimiter, int fieldno, int *length);
*/


typedef class _sip_uri {
private:
	void init(char *uri, size_t len);
public:
	SIPSTRING _uri;
	SIPSTRING _username;
	SIPSTRING _tag;
	SIPSTRING _extension;
	SIPSTRING _ipaddress;
	SIPSTRING _port;
	SIPSTRING _uriparameters;
	_sip_uri(void);


	_sip_uri(char *username, char *ipaddress, unsigned short port, char *tag);
	_sip_uri(char *ipaddress, unsigned short port, char *tag);
	_sip_uri(char *ipaddress, unsigned short port);
	_sip_uri(char *username, struct sockaddr_in *ipaddr, unsigned short port, char *tag);
	_sip_uri(struct sockaddr_in *ipaddr, unsigned short port, char *tag);
	_sip_uri(struct sockaddr_in *ipaddr, unsigned short port);

	_sip_uri(char *uri);
	_sip_uri(char *uri, size_t len);
	~_sip_uri();
	/* Functions */
	char *username(void);
	char *username(char *val);
	char *extension(void);
	char *extension(char *val);
	/* Returns the extension as an unsigned long long if it is numeric */
	unsigned long long iextension(void);
	char *ipaddress(void);
	char *ipaddress(char *val);
	char *port(void);
	char *port(char *val);
	char *tag(void);
	char *tag(char *val);
	char *uri(void);
} SIP_URI;

typedef class _sdp {
public:
	/* Creates a new SDP object for SIP INVITES */
	_sdp(void);
	/* Cleans up our SDP object */
	~_sdp();

	unsigned short audio_port;
	char audio_address[100];
	struct sockaddr_in audio_destination;
	AUDIO_STREAM_ID incoming_ssrc;
	int ulaw_offered;
	AUDIO_DIRECTION audio_direction;

} SIP_SDP;

struct _headerinfo {
	SIP_HEADER_TYPE headertype;
	SIP_URI *uri;
	struct _sip_cseq {
		int sequence;
		SIPSTRING method;
	} sip_cseq;
	struct _sip_via {
		SIPSTRING version; // SIP/2.0
		SIPSTRING transport; // TCP
		SIPSTRING ipaddress; // Local Deedle IP
		SIPSTRING branch;
		SIPSTRING received;
		SIPSTRING rport; // Default: 5060
		unsigned short usrport; /*rport as unsigned short */
		JSTRING::jsstring **via_params;
	} sip_via;
	void *ptr;
};


struct _headerlist {
	SIPSTRING header;
	SIPSTRING value;
	struct _headerinfo sip_headerinfo;
};

struct SIP_message {
	int authneeded;
	int statuscode; /* This will be greater than ZERO if it was a SIP response */
	int contentlength;
	CONTENT_TYPES content_type;
	char media_audio_ip[100];
	unsigned short media_audio_port;
	AUDIO_STREAM_ID sdp_ssrc;
	AUDIO_STREAM_ID sdp_session_id;
	int expires;
	time_t timestamp;
	char needresponse; /* This will be a 1 if we need to answer this request */
	SIPSTRING request; /* This will be populated if it's a request */
	struct _headerlist **headers; /* List of headers and their values */
	struct _headerlist *from_header, *to_header, *via_header, *csq_header, *callid_header, *contact_header;
	int headercount; /* Quantity of elements in the "headerlist" array */
	struct _authinfo {
		char *authvalue; /* The part after the "WWW-Authenticate" header */
		SIPSTRING realm;
		SIPSTRING nonce;
		SIPSTRING opaque;
		SIPSTRING algorithm;
		SIPSTRING qop;
		int stale;
		SIPREGISTERSTEP step;
	} authinfo; /* Information pertaining to the authentication */
	struct _content {
		char *rawcontent;
		SIP_SDP sdp;
	} content; /* Probably not needed here considering the application will be processing the SDP (or other content) */
};




typedef class _sip {
private:
public:
	~_sip();
	_sip();
	_sip(char *sipmessage);
	_sip(char *sipmessage, size_t length);

	/* Information on the status of this message */
	struct _stats {
		time_t created, lastsendattempt, timesentsuccessfully;
		size_t sendattempts;
	} stats;

	static const char *sip_request_list[];


	/* 1 if message is parsed or 0 if it is not */
	int isparsed;
	struct SIP_message message;
	/* A pointer to the raw message */

	char pmessage[MAX_SIP_MESSAGE_SIZE];


	struct _headerlist *addheader(char *line);
	void clear(void);

	/*
	Returns a pointer to the VALUE of the header requested
	by the string pointed at by the first argument.  If the
	return value is not NULL (the header was found), the
	length of the VALUE will be available in the variable
	pointed at by "value_length"
	*/
	struct _headerlist *getheader(const char *header);


	/*
	Returns a pointer to the VALUE of the header requested
	by the string pointed at by the first argument.  If the
	return value is not NULL (the header was found), the
	length of the VALUE will be available in the variable
	pointed at by "value_length"
	*/
	char *getheaderval(const char *header, size_t *value_length);

	void show_message(void);

	int issip();
	static int issip(char *);
	int isSIPrequest();
	static int isSIPrequest(char *);

	int isSIPresponse();
	static int isSIPresponse(char *);

	int parseSIP();
	size_t sipsize;
	void setsip(char *);
	void setsip(char *, size_t);
} SIP;





/* Possible results in SIGNAL::last_message */
typedef enum _SIGNAL_FAIL {
	SIGNAL_NODATA,
	SIGNAL_BUFF_TOO_SMALL,
	SIGNAL_CONNECT_INVALID_IP,
	SIGNAL_CONNECT_NO_PORT,
	SIGNAL_PACKET_TOO_LARGE,
	SIGNAL_OUT_OF_MEMORY
} SIGNAL_FAIL;

struct signal_message {
	/* This buffer must be freed after being retrieved by SIGNAL::nextmessage() */
	char *message;

	/* Size in bytes of this message */
	int messagesize;

	/* The time an INBOUND message was received */
	time_t signal_message_received;

	/* The time after which this message should be destroyed (0 means don't destroy) */
	time_t  signal_message_scheduled_destroy;

	/* The time at which this OUTBOUND message was successfully sent (0 means not sent) */
	time_t signal_message_sent;
};

struct _CALLINFO;
struct sip_dialogs;
typedef struct sip_dialogs DIALOG;

typedef struct _CALLINFO {
	DIALOG *dialog;
	CALL_STATUS status;
	AUDIO_STREAM_ID audio_stream_id;
	char talking_to_number[100];
	char talking_to_name[100];
	void *userdata; /* For custom data pertaining to the call */
	char did[100]; /* The Caller-ID for this call.  For incoming calls, this is the number that was dialed by the caller, for outbound calls, this is the number that shows from which the call was made */
	char callid[500];
    char uniqueid[200];
	time_t call_start_time;
	int is_incoming;
	int is_intercom;
	double unique_call_id; /* Assigned by Asterisk to identify this call */
    int call_missed; // This signals if this call was answered by someone (or voicemail) or if the caller hung up.
} CALLINFO;



struct sip_dialogs {
	char inuse;
	size_t dialog_errors;
	time_t dialog_created_time;
	time_t destroy_after;
	time_t last_ring;
	time_t last_status_change;
	CALLINFO callinfo;
	int audio_active;
	int isinvite;
	struct _dialog_info {
		char callid[500];
		char branch[500];
		struct _mine {
			int cseq;
            unsigned long session_identifier;
            unsigned long media_version;
			char tag[500];
			char extension[100];
			char name[100];
			char ipaddress[100];
			AUDIO_STREAM_ID ssrc;
			unsigned short port;
		} mine, theirs;
	} dialog_info;
	SIP *primary;
};

struct signal_settings;
typedef struct signal_settings SIGNAL_SETTINGS;

/* signal CLASS, begin signal class */
class SIGNAL {
private:
	int application_ready;
public:
	time_t last_options;
	int shuttingdown;
	int cseq_registration;
	SIP *registration;
	time_t current_registration_expires;
	RTPENGINE *rtp;
    char string_error_condition[500];
	int do_not_disturb;
    struct _reg_info {
        int failed_attempts;
        int auth_sent;
        time_t last_registration_attempt;
    } registration_info;

    
    int shut_down;
    
	/* Voicemail waiting */
	struct _voicemail {
		/*
		Messages-Waiting: no
		Fax-Message: <new>/<old>
		Voice-Message: <new>/<old> (<new urgent>/<old urgent>)
		*/
		int messages_waiting;
		int new_messages;
		int old_messages;
		int new_urgent_messages;
		int old_urgent_messages;
	};

    int total_call_count;
    int calls_talking;

	unsigned long iplist[10];
	char *json_ip_list;
	DIALOG dialog_list[MAX_DIALOG_COUNT];

	struct _user_info {
		int extension;
		char name[100];
		char callerid[100];
		char callername[100];
		char password[100];
		char localmachineip[50]; /* This will not change.  It reflects which IP address (by inference which Network interface) on which this connection was established */
		char ipaddress[50]; /* This is the IP address we use in our SIP messages.  It will by default be the localmachineip, but can change based on SIP rport */
		unsigned short rport; /* Port on which we're receiving SIP messages.  Default is 5060, but may change based on SIP rport */
		int ipconfirmed;
	} user_info;


	/* Struct containing connection registration information. */
	struct _signal_registration {
		char signal_registration_nonce[256],
			signal_registration_domain[256],
			signal_registration_packet[3000];
		int signal_registration_last_expiry;
		time_t signal_registration_expires;
		unsigned short signal_registration_rport;
	} signal_registration;


	struct signal_message message_stack[SIGNAL_MAX_SIP_STACK_SIZE];


	/* Returns whether or not we have a valid registration */
	int isregistered(void);

	/* Return the time at which the registration expires */
	time_t expires(void);

	/* Returs the time at which we should re-REGISTER */
	time_t timetoreregister(void);

//    static void audio_ready( void * );

	/* The number of message_stack slots used */
	int message_stack_size;

	/* Get the password */
	char *password(void);

	/* Set the password */
	void password(char *);

	void set_caller_id(const char *);
	void set_caller_name(const char *);


	/* Sends a 487 Request Terminated Message */
	void request_terminated(DIALOG *);

	/* Sends an OK to a SIP message */
	void ok(SIP *);

	/* Transfers a call */
	void transfer(char *callid, char *xferto);

    /* Converts s SIP response code to text */
    const char *response_to_text(int sip_response);
    
    /* Send a SIP response */
    void response( SIP *message, int sip_response );
    
    
    /* Hold and unhold a call */
    void hold( char *);
    void unhold(char *);
    
	/* Rejects an INVITE with a 486 Busy Here */
	void busy(SIP *);
	void busy(DIALOG *);

	void cancel(DIALOG *);

	void bye(DIALOG *);

	void ringing(DIALOG *);

	void ack(DIALOG *dialog);

	void answer(char *szcallid);

	void hangup(char *szcallid);

	void subscribe(unsigned long);

	/* Converts any text in the string to corresponding DTMF digits */
	static void digit_convert(char *);


	/* Sends a DTMF relay to all dialogs that are TALKING */
	void digit(char);

	/* Dials the string in "char *" on TALKING dialogs as DTMF tones. */
	void dial_digits(char *);
	void dial_digits(char *, size_t);

	void call(char *callto, size_t len);
	void call(char *callto);

	void check_dialogs(void);

	size_t generate_sdp(DIALOG *dialog, char *buffer, char *audio_state);
    size_t make_hold_sdp(DIALOG *dialog, char *buffer);
	AUDIO_STREAM_ID generate_sdp(char *buffer, char *audio_state);
	char *setlocalipaddress();


	/*
	An array of "_socket" structs that we'll use to
	receive any straggling messages that could come from the
	server if it happens to send packet to the old connection.
	When we instantiate a new TCP connection, we will roll
	"psocket" to the next _socket to use and scheduled a time
	to destroy the old socket.
	*/

	struct __socket {

		/* Socket descriptor */
		SOCKET s;

		/* Time this socket was created */
		time_t created;

		/* Time after which this socket should be cleared out */
		time_t destroy_at_time;

		/* The last time this socket sent or received data*/
		time_t last_used;

		/* Buffer into which we load our data */
		char buffer[SIGNAL_TCP_BUFFER_SIZE];

		/* The size of the cached data so far. */
		int buffersize;


		time_t last_connection_check;



		/*
		The time of the last data received from the server, it should never
		exceed 10 seconds.  There should never be a period of more than
		10 seconds during which we haven't heard from the server.
		*/
		time_t last_response;


	} _socket[10];

	/*
	This function checks its respective connection's buffer for SIP
	messages.  It will look from top to bottom and pull SIP messages
	off (moving them onto the class' output stack), moving the remaining
	data forward in the connection buffer.
	*/
	int pullSIPoffstack(struct SIGNAL::__socket *s);


	/* Pointer to the active socket */
	struct __socket *psocket;

	/*
	Buffer to store packets to send.  Using heap since a few milliseconds won't make that big of a difference when compared to the
	extensibility of doing it this way.
	*/
	struct _send_queue {
		char *send_buffer;
		int len;
		/* The number of times we should try sending this */
		int max_retry;
		time_t last_send_attempt;
	} *send_queue;

	/* The number of packets waiting to be sent */
	int send_queue_count;

	/* The time at which we last saw an OPTIONS request or response come from the serve */
	time_t signal_last_options;

	/* The time at which we know the server last received a TCP packet from us */
	time_t signal_last_successfull_send;

	/* The Thread ID.  Saved for sending messages (such as WM_QUIT) to our network-worker thread */
	DWORD signal_thread_id;


	/* IP Address of DeedleServer */
	char voip_server_ip[20];

	/* port to which we are SENDING our SIP */
	unsigned short signal_port;

	/* Struct containing port and IP address of Deedle's VoIP server to which we'll connect */
	struct sockaddr_in signal_serveraddr;

	/* Mutex for access to inbound signal stack */
	MUTEX signal_mutex;


	/* Signal for signal_thread to shut down */
	int signal_thread_running;

	/* Handle for the SIGNAL::signal_thread() function running */

#ifdef __APPLE__
	pthread_t db_signal_thread;
#elif _WIN32
	HANDLE db_signal_thread;
#endif
	/* mutex-protected count of inbound messages to handle */
	int signal_available_message_count;

	/* Flag that indicates whether or not we have an active TCP connection */
	int connected;

	/* Closes the current network connection and sets signal_socket to '0' */
	void _close(void);

	/* Establishes lock on protected resources */
	int lock() {
		db_lock_mutex(&this->signal_mutex);
		return 1;
	}

	/* Releases lock on protected resources */
	int unlock() {
		db_unlock_mutex(&this->signal_mutex);
		return 1;
	}



	/*
	Establishes a new connection to the VoIP (DeedleBox) server closing any existing connection
	and updating networking info as needed.
	*/
	int voipconnect(void);

	/* If the IP Address is valid, it returns 1, otherwise, 0 */
	int validateipaddress(char *);

	int queue(char *, int len);

	char *trim(char *ptr, int len);


	/* Returns a pointer to the requested (count) nth line or 0 if that line isn't available */
	char *line(char *ptr, int count);

	/* Returns the number of lines available in the buffer */
	int linecount(char *ptr, int len);

	/* Use the entire buffer, no length specified */
	int linecount(char *ptr);

	/* Returns the length of the line pointed at in "ptr" */
	int linelength(char *ptr);

	/* Returns 1 if the string matches BACKWARDS disregarding whitespace */
	int r_matches(char *haystack, char *needle);

	/* Returns 0 if the two strings don't match (up to the shorter of the two string) */
	int matches(char *str1, char *str2);

	/* Scans the buffer and gets a pointer to the start of a SIP Message */
	char *getnextSIP(char *ptr);

	/* Queues an OPTIONS request */
	void options(void);

	/* Returns the value of this line.  (After the ':') */
	char *headervalue(char *);


	/* Registers on current connection */
	void sipregister(struct SIGNAL::__socket *psocketstruct);
	void sipregister(struct SIGNAL::__socket *psocketstruct, int expires);
	void sipunregister(struct SIGNAL::__socket *psocketstruct);
	void sipunregister(void);


	/* Shut down signalling class */
	void sipshutdown(void);



	/* Let's the SIGNALing subsystem know the application is ready to receive SIP messages */
	void ready(void);

	/* Let's the SIGNALing subsystem know the application is NOT ready to receive SIP messages */
	void unready(void);


	/* Authenticates a REGISTRATION */
	void auth(SIP *message, struct SIGNAL::__socket *psocketstruct);

	/* Authenticates a DIALOG */
	void auth(DIALOG *, SIP *message);

	/* Parses the SIP message */
	//SIP *parseSIPmessage(char *);

	//public:
	/*
	This variable will contain an enumerated _SIGNAL_FAIL value indicating what
	went wrong if a SIGNAL class operation fails.
	*/
	int last_message;
	static const char *sip_request_list[];
	static const char OPTIONS[];
	static const char REGISTER[];
	static const char AUTHREGISTER[];
	static const char *empty;

	SIGNAL(char *username, int extension, char *pwd, char *ipadress, unsigned short port);
	SIGNAL(char *username, int extension, char *ipadress, unsigned short port);
	SIGNAL(const char *username, int extension, char *ipadress, unsigned short port);
	SIGNAL(const char *username, int extension, char *pwd, char *ipadress, unsigned short port);
	SIGNAL(int extension, char *ipadress, unsigned short port);
	SIGNAL(int extension, char *pwd, char *ipadress, unsigned short port);
	SIGNAL(char *ipadress, unsigned short port);
	SIGNAL(unsigned short);
	SIGNAL(SIGNAL_SETTINGS *s);
	SIGNAL();
	~SIGNAL();
	void initialize();

	int setvoipserverip(char *ip);


	int setlocalip(char *ip);
	int setlocalip(char *ip, size_t length);

	/*
	Returns 1 if the pointer points to the beginning of a SIP request.
	Otherwise, 0
	*/
	int isSIPrequest(char *ptr);

	/*
	Returns the response code ( 100-999 ) if the pointer
	points to the beginning of a SIP response.
	*/
	int isSIPresponse(char *ptr);

	/* Determines if the ptr points to the START of a SIP message */
	int isSIP(char *ptr);

	/*
	Returns the number of bytes comprising the entire SIP message.
	The "ptr" variable MUST be a pointer to the start of an already-located
	SIP message beginning.
	*/
	int getwholeSIPmessage(char *ptr);




	/*
	Function to be called by the application if for any reason (possibly no OPTIONS reponse) it
	believes that there's a problem with the conenction.  May be called from within this class also.
	*/
	void reconnect(void);

	void set_rtp(RTPENGINE *rtp_to_use);

	/*
	This function returns the IP Address and port to send in SIP messages so the server knows where
	to send traffic back to.  The SIP stack will call this when compiling SIP messages so that it
	always has the correct info through network changes.

	Populates *buff with this instance of Deedle's IP Address and port like: "xxx.xxx.xxx.xxx:port"

	*buff must be at least 50 bytes

	*/
	int IPandport_deedle(char *buff);

	/*
	Populates *buff with this instance of Deedle's IP Address and port like: "xxx.xxx.xxx.xxx:port"

	*buff must be at least 50 bytes

	*/
	int IPandport_server(char *buff);


	/* Changes in_addr to IPv4 text */
	static char *ip2string(struct in_addr *);

	/* Changes a text-based IP address to an in_addr structure */
	static unsigned long string2ip(char *);
	static unsigned long string2ip(const char *);



	/*
	Fills the buffer with the next SIP message available and returns the number of bytes
	it put into the buffer.  If the buffer is smaller that the size of the message to be passed,
	the last function will return -1 and SIGNAL::last_message will contain information about
	the failure reason.

	Arguments:
	1. "char *" a pointer to the buffer into which we'll load the SIP message
	2. "int" The size of the buffer in bytes.
	*/

	int nextmessage(char *, int);
	/*
	Arguments:
	1. "char *" a pointer to the buffer into which we'll load the SIP message
	2. "int" The size of the buffer in bytes.
	3. A pointer to a "sockaddr" struct into which the address of the server from
	which the message came will be provided.
	*/
	int nextmessage(char *, int, sockaddr_in *);


	void newbranch(char *buff);
	void newcallid(char *buff);
	void newtag(char *buff);


	MUTEX dialog_mutex;

	const char *status_to_text(CALL_STATUS);

	DIALOG *get_dialog(char *callid);
	DIALOG *get_dialog(char *callid, size_t len);
    void dialog_change_status(DIALOG *d, CALL_STATUS newstatus, int force);
	void dialog_change_status(DIALOG *d, CALL_STATUS newstatus);
	DIALOG *add_dialog(SIP *sipmessage);
	void abort_dialog(DIALOG *);
	int destroy_dialog(char *callid);

	/* Working on */
	size_t get_call_list(CALLINFO **ci);

	/* Call backs */
	void(*call_status_change_callback)(SIGNAL *, CALLINFO *);
	void(*ringing_status_callback)(int);
	void(*registered_callback)(SIGNAL *, int);
	void(*voicemail_callback)(int extension, int messages_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent);

	/* Need to implement, callbacks call backs */
};




struct signal_settings {

	/* The extension that correlates to the extension/endpoint defined in Asterisk */
	int extension;
	int do_not_disturb;

	char SIP_server_IP[100];
	unsigned short SIP_server_port;

	/* SIP Authentication info */
	struct _auth {
		char username[100];
		char password[100];
	} auth;

	/* Information about the logged in user */
	struct _user {
		char username[100];
		char callerid[100];
	} userinfo;

	/* RTP engine to use (Multiple SIGNAL classes can share an RTP engine ) */
	RTPENGINE *rtp;

	/* Callbacks to define for telephone events */
	struct _callbacks {
		void(*call_status_change_callback)(SIGNAL *, CALLINFO *);
		void(*ringing_status_callback)(int);
		void(*registered_callback)(SIGNAL *, int);
		void(*voicemail_callback)(int extension, int messages_waiting, int vm_new, int vm_new_urgent, int vm_old, int vm_old_urgent);
	} callbacks;
};




//unsigned long long timestamp(void);

#ifdef __APPLE__
void *signal_thread(void *param);
#elif _WIN32
DWORD WINAPI signal_thread(void *lParam);
#endif



