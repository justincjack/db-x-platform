//
//  rtp.hpp
//  deedlecore project - librtp.a
//
//  Created by Justin Jack on 8/17/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#ifndef rtp_hpp
#define rtp_hpp

#include <portaudio.h>
#include "audio.h"
#include "network.h"


static const char settings_file[] = "/deedle2.conf";
#ifdef __APPLE__
static const char settings_path[] = "~/Library/deedle";
#elif __WIN32
#endif



class _RTP_ {
private:
    int ringer_status;
public:
    _RTP_();
    _RTP_( void(*audio_ready)(void *));
    ~_RTP_();
    struct _audio_buffer *audio;
    /* AUDIO_STREAM_ID */ unsigned long start_audio( char *ipaddress, unsigned short port, char *sip_call_id, unsigned long myssrc);
    int stop_audio( char *sipcallid );
    struct _audio_stream_list *get_audio_stream( int * );
    void playring(void);
    void stopringing( void );
    int getdevicecount( void );
    const PaDeviceInfo *deviceInfo; // deprecated
    struct _audio_devices requested_device;
    int getdeviceinfo( int index );
    char *refresh_audio_device_list();
    void set_input_device(int input_device_number);
    void set_output_device(int output_device_number);
	void set_ring_device(int ring_device_number);
	int set_input_device(char *);
	int set_output_device(char *);
	int set_ring_device(char *);
	int get_input_device( char * );
    int get_output_device( char * );
    int active_input_device(void);
    int active_output_device(void);
    int active_ring_device(void);
	int get_active_input_device( void );
	int get_active_ring_device(void);
	int get_active_output_device( void );
	char *json_audio_device_list( void );
    void start_ring(void);
    void stop_ring(void);
    int is_ringing(void);
    
    int is_ringer_on(void);
    void set_ringer_off(void);
    void set_ringer_on(void);
    void set_ring_volume( int percent );
    int get_ring_volume( void );
    void prime_audio( void );
    void intercom_beep(void);
    void set_device_change_callback(void(*)(int, int, int, char *));
    
    
};


typedef _RTP_ RTPENGINE;








#endif /* rtp_hpp */
