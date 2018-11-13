//
//  audio.h
//  deedlecore project - librtp.a
//
//  Created by Justin Jack on 8/13/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//  Rev: 2

#ifndef audio_h
#define audio_h

#include <stdio.h>
#include <portaudio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <pthread.h>
#endif

#ifdef _WIN32
#include <db_x_platform.h>
#else
#include "/Users/justinjack/Documents/Deedle Cross Platform Library/db_x_platform.h"
#endif


#ifdef __APPLE__
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <time.h>
#endif

#include "sounds.h"

/* Sounds in sounds.h */
// ringtone
// dialtone
// intercomring
// intercom_in
// sound_sample
// answerpark
/***************/

#ifndef RTP_AUDIO_TIMEOUT
#define RTP_AUDIO_TIMEOUT 30
#endif



/* Audio Definitions */

#define AUDIO_FRAME_SIZE 320

#define ABS(x) ((x<0)?(x*-1):x)
#define DIFF(x, y) ABS((short)((short)x-(short)y))


#define JITTER_FRAMES 2

#define AUDIO_BUFFER_SIZE 50

#define UDP_MAX_CONNECTIONS 20

#define GAIN_ADJUST( sample ) ((short)((double)sample * gain))
#define VOLUME_ADJUST(sample) ((short)((double)sample * volume))
#define GV_ADJUST(sample, volume_or_gain) ((short)((double)sample * volume_or_gain))
#define RING_ADJUST(sample) ((short)((double)sample * ring_volume))

#ifdef __APPLE__
static pthread_t audio_alive = 0;
#else
static HANDLE audio_alive = 0;
#endif
static int audio_run_state = 0;
static int audio_thread_return_value = 1;

static double volume = 1.00;
static double gain = 1.00;
static double ring_volume = 0.50;

typedef enum _sample_buffer_status {
    SAMPLE_BUFFER_EMPTY,
    SAMPLE_BUFFER_READY,
    SAMPLE_BUFFER_FILLING
} SAMPLE_BUFFER_STATUS;

/*
static int _a_in_device;
static int _a_out_device;
static int _a_in_device_running;
static int _a_out_device_running;
 */


/***  uLAW coded definitions ****/
#define BIAS 0x84
#define CLIP 32635
static const short cnvtr[] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
static const short etab[] = {0,132,396,924,1980,4092,8136,16764};
/********************************/

/*
 The default average amplitude for which we assume silence.
 This GLOBAL variable will be modified at run-time based
 on measurements from the microphone under certain situations -
 Where we're sure there is no audio playing...
 
 */
static int ASSUMED_SILENCE = 100;

struct _sample {
    short average_amplitude; /* The average amplitude of this block. We will use this
                              figure to help determine if we have a period of silence
                              during which we'll play catch-up and advance the
                              "readblock" for if our "current_avg_jitter" time
                              is dropping.  This will reduce our "jitter_buffer_size" and
                              thus reduce latency (frame-received-to-playback time)
                              */
    short pcm[160];          /* 320 bytes - 20 milliseconds of audio */
    SAMPLE_BUFFER_STATUS buffer_status;
};

typedef unsigned long AUDIO_STREAM_ID;

typedef struct _audio_stream_list {
    int connection_number;
    struct sockaddr_in client;
    char ipaddress[100];
    char sip_call_id[500];
    unsigned short port;
    AUDIO_STREAM_ID id;
    AUDIO_STREAM_ID senderid;
    time_t time_active;
} AUDIO_STREAM_LIST;

struct _incoming_connection {
    time_t inuse;
    time_t started_at;
    unsigned long last_time_stamp;
    unsigned short last_sequence;
    int connectionnumber;
    int readblock, readblock_out;
    int writeblock, writeblock_out;
    int start_mixing; /* This stream has buffered enough data to start mixing it into the playback (to PC audio) stream */
    char sip_call_id[500];
    double volume;
    struct sockaddr_in client_address;
    struct _sample incoming_samples[AUDIO_BUFFER_SIZE];
    struct _sample outbound_samples[AUDIO_BUFFER_SIZE];
    
    /* RTP info */
    unsigned short media_index;
    AUDIO_STREAM_ID audio_stream_id;
    AUDIO_STREAM_ID their_audio_stream_id;
    unsigned long media_time_stamp;
};

struct _sound_list {
    unsigned long long length;
    short *pointer;
};

struct _audio_devices {
    int device_id;
    char device_name[200];
    int is_input;
    int is_output;
};

/* Our shared audio object */
struct _audio_buffer {
    PaStream *instream, *outstream;
    int input_status;
    int output_status;
    SEMAPHORE clock_sync;
    int readblock;           /* The next block that's ready to feed to PortAudio to play */
    size_t writeblock;          /* The next block to which we're going to write inbound audio from the network */
    int current_avg_jitter;  /* The current average measurement of jitter (in milliseconds) over the last 10 seconds */
    size_t jitter_buffer_size;  /* The number of 20 millisecond audio blocks we're keeping as a buffer between
                                 the "writeblock" and the "readblock"
                                 */
    
    
    /* Copy of buffer played to speaker for echo cancellation */
    short audio_played[16000]; // Two seconds of audio
    short aec_amp_diff_list[16000];
    short aec_diff_pos;
    short aec_write_position;
    short aec_delay; /* In number of samples */
    
    /* Audio Initialization tracking */
    int init_count;
    
    
    
    /* Intercom beep info */
    int intercom_beep_offset;
    int intercom_beep_status;
    
    int portaudio_initialized;
    
    /* Call backs */
	void *audio_ready_data;
    void(*audio_ready)(void *);
    void(*audio_device_change)(int input_device, int output_device, int ring_device, char *device_list);
    
    
       
    /* Flag to temporarily turn on audio engine in preparation for a call */
    time_t audio_timeout;
    
	/* Ringer info */
	int audio_ring_device;
	int active_audio_ring_device;
	int audio_default_ring_device;

    int audio_input_device;
    int active_audio_input_device;
    int audio_output_device;
    int active_audio_output_device;
    int active_stream_count;
    int audio_default_input_device;
    int audio_default_output_device;
    
    
    
    /* Ring properties */
    int ringing;
    int audio_ringing;
    int ringer_on;
    int ring_status;
    time_t ring_toggle_timer;
    size_t ring_sample_ptr;
    PaStream *ring_stream;
    int ringing_silence;
    size_t frames_rang;
    
    char *json_audio_device_list;
    struct _audio_devices audio_devices[50];
    int device_count;
    struct _sample samples_to_play[AUDIO_BUFFER_SIZE]; /* Fifteen samples (300 ms) of audio */
    
    SOCKET udp_socket;

    MUTEX buffer_mutex;
    
    /* The IP Addresses and ports to which we'll send out audio */
    size_t outbound_stream_count;
    struct _incoming_connection connection_list[UDP_MAX_CONNECTIONS];
    /************************************************************/
    
    struct sockaddr_in iplist[100]; /* For broadcasting */
    
    /* Extra sound sample input */
    struct _sound_list sound_list[6];
    short *slip_in_sound;
    unsigned long long sound_length;
    unsigned long long sound_position;
    

    int scan_for_devices;
    int refreshing_devices;
    
    
};

/*****************************************/





#ifdef __cplusplus
extern "C" {
#endif
#ifdef __APPLE__
    pthread_t start_audio_engine( struct _audio_buffer **, int input_device, int output_device, void(*audio_ready_callback)(void *), void *audio_callback_data);
	void *audio_thread(void *);
#else
	HANDLE start_audio_engine(struct _audio_buffer **, int input_device, int output_device, void(*audio_ready_callback)(void *), void *audio_callback_data);
	DWORD WINAPI audio_thread(void *);
#endif
    int stop_audio_engine(struct _audio_buffer **paudio);
    int audio_running(void);
    unsigned long long timestamp( void );
    int ring_callback( const void *input, void *output, unsigned long samplecount, const PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags statusflags, void *userData );
    /*
    static int audio_out( const void *input, void *output, unsigned long samplecount, const PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags statusflags, void *userData );
    static int audio_in( const void *input, void *output, unsigned long samplecount, const PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags statusflags, void *userData );
     */
    int add_broadcast_ip( const char *ipaddress ); /* For Lee to implement */
    int del_broadcast_ip( const char *ipaddress ); /* For Lee to implement */
    int set_volume( int percentage ); // The parameter this takes is a percentage from 0 - 100
    int set_gain( int percentage ); // The parameter this takes is a percentage from 0 - 100
    int get_volume( void ); // Returns the speaker volume as a percentage
    int get_gain( void ); // Returns the microphone gain as a percentage;
    int set_ringer_volume( int percentage );
    int get_ringer_volume( void );
    /*
    db_inline short ulaw2linear( unsigned char ulawbyte );
    db_inline unsigned char linear2ulaw(short sample);
     */
    
    int audio_slip_sound( struct _audio_buffer *audio, int sound_number );
    static db_inline short ulaw2linear( unsigned char ulawbyte ) {
        short sample=0;
        unsigned short exponent=0, mantissa=0;
        ulawbyte = ~ulawbyte;
        exponent = (ulawbyte & 0x70) >> 4;
        mantissa = ulawbyte & 0x0f;
        sample = mantissa << (exponent + 3);
        sample = sample + etab[exponent];
        if (ulawbyte & 0x80)
            sample = -sample;
        return sample;
    }
    
    static db_inline unsigned char linear2ulaw(short sample) {
        int sign=0;
        short exponent=0, mantissa=0;
        unsigned char ulawbyte=0;
        sign = (sample >> 8) & 0x80;        /* set aside the sign */
        if (sign != 0)
            sample = -sample;        /* get magnitude */
        if (sample > CLIP)
            sample = CLIP;        /* clip the magnitude */
        sample+=BIAS;
        exponent = cnvtr[ ( sample >> 7 ) & 0xFF  ];
        mantissa = (sample >> (exponent + 3)) & 0x0F;
        ulawbyte = ~(sign | (exponent << 4) | mantissa);
        if (ulawbyte==0)
            ulawbyte=2;
        return ulawbyte;
    }
    
   
    
    /* Returns the unique identifier for this stream */
    AUDIO_STREAM_ID start_audio_stream( struct _audio_buffer *audio, char *sip_call_id, char *ipaddress, unsigned short port, AUDIO_STREAM_ID myssrc);
    
    /* Returns 1 if the stream was successfully stopped */
    int stop_audio_stream( struct _audio_buffer *audio, char *sipcallid );
    void reset_audio_connection( struct _incoming_connection *connection_buffer );
    
    AUDIO_STREAM_LIST *get_active_streams(struct _audio_buffer *audio, size_t *listsize);

    void flush_all_audio_buffers( struct _audio_buffer *audio );
    
    int audio_set_input_device( struct _audio_buffer *audio, char *);
    int audio_set_output_device(  struct _audio_buffer *audio, char * );
	int audio_set_ring_device(struct _audio_buffer *audio, char *);
    
    
    void scan_for_new_audio_devices(struct _audio_buffer *audio);
    int audio_refresh_device_list(struct _audio_buffer *audio);
    int get_stream_count( struct _audio_buffer *audio );
    
    void start_ringing( struct _audio_buffer *audio );
    void stop_ringing( struct _audio_buffer *audio );
    
    void play_intercom_beep( struct _audio_buffer *audio );
    
    PaError init_audio( struct _audio_buffer *audio );
    PaError free_audio(struct _audio_buffer *audio);
    
    void restart_audio( struct _audio_buffer * );

    
#ifdef __cplusplus
}
#endif





#endif /* audio_h */
