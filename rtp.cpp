//
//  rtp.cpp
//  deedlecore project - librtp.a
//
//  Created by Justin Jack on 8/17/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//
#include <stdio.h>
#include "memcheck.hpp"
#include <db_x_platform.h>
#include "rtp.hpp"

_RTP_::_RTP_() {
    this->audio = 0;
    this->ringer_status = 0;
    start_audio_engine(&this->audio, -1, -1, 0, 0);
    start_network(&this->audio);
}

_RTP_::_RTP_( void(*audio_ready)(void *) ) {
    this->audio = 0;
    this->ringer_status = 0;
    start_audio_engine(&this->audio, -1, -1, audio_ready, this);
    start_network(&this->audio);
}


void _RTP_::set_ring_device(int ring_device_number) {
	if (ring_device_number >= 0 && ring_device_number < this->audio->device_count) {
		if (this->audio->audio_devices[ring_device_number].is_output == 1) {
			printf("Ring Device Change!\n");
			this->audio->audio_ring_device = ring_device_number;
			return;
		} else {
			printf("Device number %d (%s) is not an output device.\n", ring_device_number, this->audio->audio_devices[ring_device_number].device_name);
		}
	}
	printf("Failed to change output device.\n");
}

int _RTP_::set_ring_device(char *devname) {
	if (!devname || !this->audio) return -1;
	return audio_set_ring_device(this->audio, devname);
}


void _RTP_::intercom_beep( void ) {
    if (!this->audio) return;
    this->audio->audio_timeout = time(0) + 20;
    this->audio->intercom_beep_status = 1;
    return;
}

int _RTP_::get_active_ring_device(void) {
	return this->audio->active_audio_input_device;
}


int _RTP_::get_active_input_device(void) {
	return this->audio->active_audio_input_device;
}

int _RTP_::get_active_output_device(void) {
	return this->audio->active_audio_output_device;
}


char *_RTP_::refresh_audio_device_list(void) {
    static const char no_devices[] = "[]";
    if (!this->audio) return (char *)no_devices;
    scan_for_new_audio_devices(audio);
    while (audio->refreshing_devices) Sleep(1);
    return audio->json_audio_device_list;
}

int _RTP_::is_ringer_on(void) {
    return this->audio->ringer_on;
}

void _RTP_::set_ringer_off(void) {
    if (!this->audio) return;
    printf("Ringer set OFF\n");
    this->audio->ringer_on = 0;
}

void _RTP_::prime_audio( void ) {
    if (!this->audio) return;
    this->audio->audio_timeout = time(0) + 20;
}

void _RTP_::set_ringer_on(void) {
    if (!this->audio) return;
    printf("Ringer set ON\n");
    this->audio->ringer_on = 1;
}


int _RTP_::active_input_device(void) {
    return this->audio->active_audio_input_device;
}
int _RTP_::active_output_device(void) {
    return this->audio->active_audio_output_device;
}

int _RTP_::active_ring_device(void) {
    return this->audio->active_audio_ring_device;
}


void _RTP_::set_ring_volume( int percent ) {
    set_ringer_volume(percent);
}

int _RTP_::get_ring_volume( void ) {
    return get_ringer_volume();
}


_RTP_::~_RTP_() {
    stop_network();
    stop_audio_engine(&this->audio);
//    printf("* RTP Library shutdown...\n");
}

AUDIO_STREAM_ID _RTP_::start_audio( char *ipaddress, unsigned short port, char *sip_call_id, AUDIO_STREAM_ID myssrc) {
    return start_audio_stream(this->audio, sip_call_id, ipaddress, port, myssrc);
}

int _RTP_::stop_audio( char *sipcallid ) {
    return stop_audio_stream(this->audio, sipcallid);
}

void _RTP_::set_device_change_callback( void (*callback)(int, int, int, char *)) {
    if (this->audio) {
        this->audio->audio_device_change = (void (*)(int, int, int, char *))callback;
    }
}

int _RTP_::getdevicecount( void ) {
    if (!this->audio) return 0;
    return this->audio->device_count;
}

int _RTP_::getdeviceinfo( int index ) {
    memset(&this->requested_device, 0, sizeof(struct _audio_devices));
    if (index >= 0 && index < this->audio->device_count) {
        memcpy(&this->requested_device, &this->audio->audio_devices[index], sizeof(struct _audio_devices));
        return 1;
    }
    return 0;
}


void _RTP_::start_ring( void ) {
    this->audio->ringing = 1;
}

void _RTP_::stop_ring( void ) {
    this->audio->audio_timeout = time(0) + 20;
    this->audio->ringing = 0;
}

int  _RTP_::is_ringing( void ) {
    return this->audio->ringing;
}


int _RTP_::get_input_device( char *devicename ) {
    int i = 0;
    for (i = 0; i < this->audio->device_count; i++) {
        if (!strcmp(this->audio->audio_devices[i].device_name, devicename)) {
            if (this->audio->audio_devices[i].is_input) return i;
        }
    }
    return -1;
}

int _RTP_::get_output_device( char *devicename ) {
    int i = 0;
    for (i = 0; i < this->audio->device_count; i++) {
        if (!strcmp(this->audio->audio_devices[i].device_name, devicename)) {
            if (this->audio->audio_devices[i].is_output) return i;
        }
    }
    return -1;
}


void _RTP_::set_input_device(int input_device_number) {
    if (input_device_number >= 0 && input_device_number < this->audio->device_count) {
        if (this->audio->audio_devices[input_device_number].is_input == 1) {
//            printf("Input Device Change!\n");
            this->audio->audio_input_device = input_device_number;
            return;
        }
    }
    printf("Failed to change input device.\n");
}

void _RTP_::set_output_device(int output_device_number) {
    if (output_device_number >= 0 && output_device_number < this->audio->device_count) {
        if (this->audio->audio_devices[output_device_number].is_output == 1) {
//            printf("Output Device Change!\n");
            this->audio->audio_output_device = output_device_number;
            return;
        } else {
            printf("Device number %d (%s) is not an output device.\n", output_device_number, this->audio->audio_devices[output_device_number].device_name);
        }
    }
    printf("Failed to change output device.\n");
}


char *_RTP_::json_audio_device_list( void ) {
    static const char no_devices[] = "[]";
    if (!this->audio->json_audio_device_list) return (char *)no_devices;
    return (char *)this->audio->json_audio_device_list;
}


int _RTP_::set_input_device(char *devname) {
	if (!devname || !this->audio) return -1;
	return audio_set_input_device(this->audio, devname);
}


int _RTP_::set_output_device(char *devname) {
	if (!devname || !this->audio) return -1;
	return audio_set_output_device(this->audio, devname);
}






