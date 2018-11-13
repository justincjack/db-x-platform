//
//  audio.c
//  deedlecore project - librtp.a
//
//  Created by Justin Jack on 8/13/18.
//  Copyright © 2018 Justin Jack. All rights reserved.
//

#include "memcheck.hpp"
#include "audio.h"


static int audio_in( const void *input,
                    void *output,
                    unsigned long samplecount,
                    const PaStreamCallbackTimeInfo *timeinfo,
                    PaStreamCallbackFlags statusflags,
                    void *userData ) {
    struct _audio_buffer *audio = (struct _audio_buffer *)userData;
    short *mybuff = (short *)input;
    int i = 0;
    if (audio->samples_to_play[audio->writeblock].buffer_status == SAMPLE_BUFFER_EMPTY) {
        audio->samples_to_play[audio->writeblock].buffer_status = SAMPLE_BUFFER_FILLING;
        for (i = 0; i < 160; i++) {
            audio->samples_to_play[audio->writeblock].pcm[i] = mybuff[i];
        }
        audio->samples_to_play[audio->writeblock].buffer_status = SAMPLE_BUFFER_READY;
        if (++audio->writeblock == AUDIO_BUFFER_SIZE) audio->writeblock = 0;
    }
    return paContinue;
}



/*
 This function is called whenever PortAudio needs an audio buffer to play
 audio out callback audio output callback
 audio out out to speaker speaker out
 */
static int audio_out( const void *input,
                     void *output,
                     unsigned long samplecount,
                     const PaStreamCallbackTimeInfo *timeinfo,
                     PaStreamCallbackFlags statusflags,
                     void *userData ) {
    struct _audio_buffer *audio = (struct _audio_buffer *)userData;
    size_t h = 0;
    int i = -1, j = -1;
    int *readblock = 0;
    int *writeblock = 0;
    time_t currenttime = 0;
    struct _incoming_connection *connection = 0, *outconnection = 0;
    short *somd = 0, *avg_amplitude = 0;
    struct _sample *read_sample = 0;
    SAMPLE_BUFFER_STATUS *psomdstatus = 0;
    SAMPLE_BUFFER_STATUS dummy_status;
    double gv_adjust = 1;
    short mic_in_buffer[160];
    
    currenttime = time(0);
    if (Pa_GetStreamReadAvailable(audio->instream)) {
        if (Pa_ReadStream(audio->instream, &mic_in_buffer, 160) != paNoError) {
            memset(mic_in_buffer, 0, 320);
        }
    } else {
        memset(mic_in_buffer, 0, 320);
    }
    for (j = -1; j < UDP_MAX_CONNECTIONS; j++) { /* Go through every connection and fill it with audio to play! */
        somd = 0;
        avg_amplitude = 0;
        writeblock = 0;
        if (j == -1) {
            /* Mix down to speaker output */
            somd = (short *)output;
            psomdstatus = &dummy_status;
            gv_adjust = volume;
            memset(somd, 0, 320);
            if (audio->intercom_beep_status == 1 ) { // If we're playing an intercom beep.
                if (audio->intercom_beep_offset < (audio->sound_list[3].length - 5) ) {
                    for (h = 0; h < 160; h++) {
                        somd[h]=audio->sound_list[3].pointer[audio->intercom_beep_offset];
                        audio->intercom_beep_offset+=5;
                        if (audio->intercom_beep_offset >= audio->sound_list[3].length) {
                            printf("Done with intercom beep!\n");
                            audio->intercom_beep_offset = 0;
                            audio->intercom_beep_status = 0;
                            break;
                        }
                    }
                }
            } else if (audio->audio_ringing) { // If we're supposed to be ringing
                if (audio->active_audio_ring_device == audio->active_audio_output_device) {
                    size_t ring_size = (sizeof(ringtone) / 2);
                    time_t ttime = time(0);
                    short *pool = (short *)ringtone;
                    int r = 0;
                    if (audio->ring_sample_ptr == 0) {
                        audio->ring_toggle_timer = ttime + 3;
                    }
                    if (!audio->ringing_silence) {
                        audio->frames_rang++;
                        for (r = 0; r < (int)samplecount; r++) {
                            if (audio->ringer_on) {
                                somd[r]=RING_ADJUST(pool[audio->ring_sample_ptr]);
                            }
                            if (++audio->ring_sample_ptr > ring_size) {
                                audio->ring_sample_ptr = 0;
                            }
                        }
                        if (audio->frames_rang > 95) {
                            audio->ringing_silence = 1;
                            audio->ring_toggle_timer = ttime + 3;
                        }
                    } else {
                        if (ttime > audio->ring_toggle_timer) {
                            audio->ringing_silence = 0;
                            audio->ring_sample_ptr = 0;
                            audio->frames_rang = 0;
                        }
                    }
                }
            }

            /* Loop through network connections and mix waiting audio into the PC Speaker buffer */
            for (i = UDP_MAX_CONNECTIONS - 1; i >=0; i--) {
                readblock = 0;
                read_sample = 0;
                connection = &audio->connection_list[i];
                if ( currenttime - connection->inuse <= RTP_AUDIO_TIMEOUT ) {
                    if ( DIFF( ((connection->writeblock - JITTER_FRAMES < 0)? (connection->writeblock + AUDIO_BUFFER_SIZE) :connection->writeblock) , connection->readblock) >= JITTER_FRAMES /*connection->start_mixing == 1*/ ) {
                        if ( /* The readblock points to a ready frame */
                            connection->incoming_samples[connection->readblock].buffer_status == SAMPLE_BUFFER_READY ) {
                            readblock = &connection->readblock;
                            read_sample = connection->incoming_samples;
                            for (h = 0; h < 160; h++) {
                                somd[h]+=read_sample[*readblock].pcm[h];
                            }
                        }
                    }
                }
            }
        } else { // Mix down to this network stream
            // Here, "j" is a network stream
            gv_adjust = 1;
            outconnection = &audio->connection_list[j];
            if ( currenttime - outconnection->inuse <= RTP_AUDIO_TIMEOUT  ) {
                if (outconnection->outbound_samples[outconnection->writeblock_out].buffer_status == SAMPLE_BUFFER_EMPTY  ) {
                    psomdstatus = &outconnection->outbound_samples[outconnection->writeblock_out].buffer_status;
                    somd = outconnection->outbound_samples[outconnection->writeblock_out].pcm;
                    avg_amplitude = &outconnection->outbound_samples->average_amplitude;
                    writeblock = &outconnection->writeblock_out;
                    for (i = UDP_MAX_CONNECTIONS - 1; i >=0; i--) {
                        if ( /* Mix To */ j == i /* Mix From */) {
                            // We don't want to mix to ourselves, so
                            // here, we'll mix in the microphone input
                            //if (audio->samples_to_play[audio->readblock].buffer_status == SAMPLE_BUFFER_READY) {
                                for (h = 0; h < 160; h++) {
                                    //somd[h]+=audio->samples_to_play[audio->readblock].pcm[h];
                                    somd[h]+=mic_in_buffer[h];
                                }
                            //}
                        } else {
                            readblock = 0;
                            read_sample = 0;
                            connection = &audio->connection_list[i];
                            if ( currenttime - connection->inuse <= RTP_AUDIO_TIMEOUT ) {
                                if ( DIFF( ((connection->writeblock - JITTER_FRAMES < 0)? (connection->writeblock + AUDIO_BUFFER_SIZE) :connection->writeblock) , connection->readblock) >= JITTER_FRAMES /*connection->start_mixing == 1*/ ) {
                                    if ( /* The readblock points to a ready frame */
                                        connection->incoming_samples[connection->readblock].buffer_status == SAMPLE_BUFFER_READY ) {
                                        readblock = &connection->readblock;
                                        read_sample = connection->incoming_samples;
                                        for (h = 0; h < 160; h++) {
                                            somd[h]+=read_sample[*readblock].pcm[h];
                                        }
                                    }
                                }
                            }
                        }
                    }
                    *psomdstatus = SAMPLE_BUFFER_READY;
                    if ( ++(*writeblock) == AUDIO_BUFFER_SIZE ) {
                        *writeblock = 0;
                    }
                    db_sem_post(audio->clock_sync); // Signal network.cpp > send_network_thread() that there's audio to send
                } else {
                    printf("audio.cpp > audio_out(): Mixing to %d (port: %u ) BACKED UP\n", j, ntohs(outconnection->client_address.sin_port));
                }
            } else {
                continue;
            }
        }
    }
    
    /* Go through and mark read buffers that we've mixed data from as SAMPLE_BUFFER_EMPTY */
    for (i = UDP_MAX_CONNECTIONS - 1; i >= 0; i--) {
        connection = &audio->connection_list[i];
        if ( currenttime - connection->inuse <= RTP_AUDIO_TIMEOUT) {
            if ( DIFF( ((connection->writeblock - JITTER_FRAMES < 0)? (connection->writeblock + AUDIO_BUFFER_SIZE) :connection->writeblock), connection->readblock) >= JITTER_FRAMES /*connection->start_mixing == 1*/ ) {
                if ( /* The readblock points to a ready frame */
                    connection->incoming_samples[connection->readblock].buffer_status == SAMPLE_BUFFER_READY ) {
                    connection->incoming_samples[connection->readblock].buffer_status = SAMPLE_BUFFER_EMPTY;
                    if (++connection->readblock == AUDIO_BUFFER_SIZE) connection->readblock = 0;
                }
            }
        }
    }
    return paContinue;
}

int ring_callback( const void *input, void *output, unsigned long samplecount, const PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags statusflags, void *userData ) {
    size_t i = 0, ring_size = (sizeof(ringtone) / 2);
    time_t ttime = time(0);
    struct _audio_buffer *audio = (struct _audio_buffer *)userData;
    short *sample = (short *)output, *pool = (short *)ringtone;
    if (!audio->ringing_silence) {
        audio->frames_rang++;
        for (i = 0; i < samplecount; i++) {
            if (audio->ringer_on) {
                sample[i] = RING_ADJUST(pool[audio->ring_sample_ptr]);
            } else {
                sample[i] = 0;
            }
            if (++audio->ring_sample_ptr > ring_size) {
                audio->ring_sample_ptr = 0;
            }
        }
        if (audio->frames_rang > 95) {
            audio->ringing_silence = 1;
            audio->ring_toggle_timer = ttime + 3;
        }
    } else {
        for (i = 0; i < samplecount; i++) {
            sample[i] = 0;
        }
        if (ttime > audio->ring_toggle_timer) {
            audio->ringing_silence = 0;
            audio->ring_sample_ptr = 0;
            audio->frames_rang = 0;
        }
    }
    return paContinue;
}

unsigned long long timestamp( void ) {
#ifdef _WIN32
	return (unsigned long long)GetTickCount();
#else
    unsigned long long retval = 0;
    struct timeval ts;
    gettimeofday(&ts, NULL);
    retval = (((unsigned long long )ts.tv_sec * (unsigned long long)1000000) + (unsigned long long )ts.tv_usec) / 1000;
    return retval;
#endif
}

#ifdef __APPLE__
pthread_t start_audio_engine( struct _audio_buffer **ab, int input_device, int output_device, void(*audio_ready_callback)(void *), void *audio_callback_data) {
#else
HANDLE start_audio_engine(struct _audio_buffer **ab, int input_device, int output_device, void(*audio_ready_callback)(void *), void *audio_callback_data) {
	DWORD threadid = 0;
#endif
    time_t timeout = 0;
    struct _audio_buffer *audio = 0;
    if (audio_alive != 0) {
        return 0;
    }
    if (*ab) {
        MEMORY::free_( (*ab));
    }
    *ab = 0;
    
    /* Create the audio object here */
    
    audio = (struct _audio_buffer *)MEMORY::calloc_(1, sizeof(struct _audio_buffer), (char *)"audio.c > start_audio_engine()");
    
    if (!audio) {
        printf("* audio_thread(): calloc() failed to allocate memory for \"audio\" buffer.\n\n");
        audio_alive = 0;
        return 0;
    }
    
    /* allocate audio init audio */
    audio->init_count = 0;
    audio->ringing = 0;
    audio->audio_ringing = 0;
    audio->audio_ready = audio_ready_callback;
	audio->audio_ready_data = audio_callback_data;
    /* Always start with the ringer on */
    audio->ringer_on = 1;
    audio->active_audio_input_device = audio->audio_input_device = input_device;
    audio->active_audio_output_device = audio->audio_output_device = output_device;
    audio->active_audio_ring_device = audio->audio_ring_device = audio->active_audio_ring_device;
    *ab = audio;
    audio_run_state = 1;

#ifdef __APPLE__
    if (!pthread_create(&audio_alive, 0, &audio_thread, (void *)ab)) {
        timeout = time(0) + 2;
        while (time(0) < timeout) {
            if ( (*ab) != 0 ) {
                break;
            }
            Sleep(1);
        }
        if (!(*ab)) {
            audio_run_state = 0;
            return 0;
        }
        return audio_alive;
    }
#else
	audio_alive = CreateThread(0, 0, &audio_thread, (void *)ab, 0, &threadid);
	if (audio_alive) return audio_alive;
#endif
    audio_run_state = 0;
    audio_alive = 0;
    return 0;
}

int stop_audio_engine(struct _audio_buffer **paudio) {
    void *retval = 0;
    struct _audio_buffer *audio = 0;
    if (paudio) {
        audio = *paudio;
    }
    if (audio_alive == 0) return 0;
#ifdef __APPLE__
    if (pthread_self() == audio_alive) return 0;
    audio_run_state = 0;
    if (audio_alive) {
        pthread_join(audio_alive, &retval);
    }
    if (audio) {
        if (audio->json_audio_device_list) {
            MEMORY::free_(audio->json_audio_device_list);
        }
        MEMORY::free_(audio);
        *paudio = 0;
    }
    return *((int *)retval);
#else 
	audio_run_state = 0;
	if (WaitForSingleObject(audio_alive, 5000) == WAIT_OBJECT_0) {
		return 1;
	}
	return 0;

#endif
}

int audio_running( void ) {
    return ((audio_alive != 0) ?1:0);
}


void scan_for_new_audio_devices(struct _audio_buffer *audio) {
//    printf("scan_for_new_audio_devices()\n");
    //if (!audio->ring_stream) {
        //printf("\tNot ringing, so we'll refresh audio devices!\n");
        audio->refreshing_devices = 1;
        audio->scan_for_devices = 1;
		/*
    } else {
        printf("\tWe have a call ringing.  We won't auto-refresh audio devices right now...\n");
    }
	*/
    return;
}

int audio_refresh_device_list(struct _audio_buffer *audio) {
    int i = 0, j = 0;
    size_t k = 0;
    const PaDeviceInfo *deviceInfo;
    char buffer[400];
    static char old_json[3000] = {0};
    size_t audio_device_name_length = 0;
	audio->audio_default_ring_device = Pa_GetDefaultOutputDevice();
    audio->audio_default_input_device = Pa_GetDefaultInputDevice();
	audio->audio_default_output_device = audio->audio_default_ring_device;

    audio->device_count = Pa_GetDeviceCount();
    for (i = 0; i < audio->device_count; i++) {
        if ( /* There are more than 50 devices (we're not ready for this many) */ i > 50) break;
        deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo) {
            audio->audio_devices[j].device_id = i;
            audio_device_name_length = strlen(deviceInfo->name);
            sprintf(audio->audio_devices[j].device_name, "%.*s", ((audio_device_name_length > 199)?199:(int)audio_device_name_length), deviceInfo->name);
            if (deviceInfo->maxInputChannels > 0) {
                audio->audio_devices[j].is_input = 1;
            }
            if (deviceInfo->maxOutputChannels > 0) {
                audio->audio_devices[j].is_output = 1;
            }
            j++;
        }
    }
    audio->device_count = j;
    /* Zero out the remainder of the device list */
    memset(&audio->audio_devices[i], 0, sizeof(struct _audio_devices) * (50-j));
    
    if (audio->json_audio_device_list) {
        MEMORY::free_(audio->json_audio_device_list);
        audio->json_audio_device_list = 0;
    }
    audio->json_audio_device_list = (char *)MEMORY::calloc_(audio->device_count, 400, (char *)"audio.c > audio_refresh_device_list(): audio->json_audio_device_list");
    
    /* Build JSON */
    audio->json_audio_device_list[0] = '[';
    for (i = 0; i < audio->device_count; i++) {
        if (i > 0) strcat(audio->json_audio_device_list, ",");
        memset(buffer, 0, 400);
        sprintf(buffer, "{\"id\":%d,\"input\":%d,\"output\":%d,\"name\":\"",
                audio->audio_devices[i].device_id,
                audio->audio_devices[i].is_input,
                audio->audio_devices[i].is_output);
        k = strlen(buffer);
        for (j = 0; j < (int)strlen(audio->audio_devices[i].device_name); j++) {
            if (audio->audio_devices[i].device_name[j] == '\"') {
                buffer[k++] = '\\';
            }
            buffer[k++] = audio->audio_devices[i].device_name[j];
        }
        strcat(buffer, "\"}");
        strcat(audio->json_audio_device_list, buffer);
    }
    strcat(audio->json_audio_device_list, "]");
    audio->refreshing_devices = 0;
    if (strcmp(old_json, audio->json_audio_device_list)) {
//        printf("OLD JSON differs from New JSON!\nOLD:\n%s\n\nNEW:\n%s\n\n", old_json, audio->json_audio_device_list);
        sprintf(old_json, "%.*s", ((strlen(audio->json_audio_device_list) > 2999)?2999:(int)strlen(audio->json_audio_device_list)), audio->json_audio_device_list);
        return 1;
    }
    return 0;
}

PaError init_audio( struct _audio_buffer *audio ) {
    PaError err = Pa_Initialize();
    if (err == paNoError) {
        audio->init_count++;
    }
    return err;
}

PaError free_audio(struct _audio_buffer *audio) {
    PaError err = Pa_Terminate();
    if (err == paNoError) {
        audio->init_count--;
    }
    return err;
}
    
    
    
/*
 
 Prime audio (check / set up input/output devices and streams ) when:
 1. A new INVITE comes in.
 2. A new INVITE is created to go out.
 
 
 */
    
    
/*
 
 restart_audio()
 
 This function should be called in the following situations:
 
 1. The user is telling us to use a new device.
 2. A validity check fails for a current stream.
 3. When audio is primed AND there are NO active network streams
 
 
 
 */
    
    int check_input_device( int index ) {
        PaError err = 0;
        PaStreamParameters pasp;
        const PaDeviceInfo *padi = 0;
        if (index == -1) return 0;
        padi = Pa_GetDeviceInfo(index);
        if (!padi) return 0;
        pasp.device = index;
        pasp.channelCount = 1;
        pasp.sampleFormat = paInt16;
        pasp.hostApiSpecificStreamInfo = 0;
        pasp.suggestedLatency = padi->defaultLowInputLatency;
        err = Pa_IsFormatSupported(&pasp, 0, 8000);
        if (err == paNoError) return 1;
        return 0;
    }
    
    
    int check_output_device(int index ) {
        PaError err = 0;
        PaStreamParameters pasp;
        const PaDeviceInfo *padi = 0;
        if (index == -1) return 0;
        padi = Pa_GetDeviceInfo(index);
        if (!padi) return 0;
        pasp.device = index;
        pasp.channelCount = 1;
        pasp.sampleFormat = paInt16;
        pasp.hostApiSpecificStreamInfo = 0;
        pasp.suggestedLatency = padi->defaultLowOutputLatency;
        err = Pa_IsFormatSupported(0, &pasp, 8000);
        if (err == paNoError) return 1;
        return 0;
    }
    
/* restart audio */
    
void restart_audio( struct _audio_buffer *audio ) {
    int audio_devs_available = 0;
    int i = 0;
    PaError err = 0;
    PaStreamParameters pasp_in, pasp_out, pasp_ring;
    PaStream *ring_stream = audio->ring_stream,
        *instream = audio->instream,
        *outstream = audio->outstream;
    int ring_dev = audio->active_audio_ring_device, out_dev = audio->active_audio_output_device, in_dev = audio->active_audio_input_device;
    audio->ring_stream = 0;
    audio->instream = 0;
    audio->outstream = 0;
//    printf("audio.cpp > restart_audio(): restart_audio()\n");
    if (ring_stream) {
        if (Pa_IsStreamActive(ring_stream)) {
            Pa_CloseStream(ring_stream);
        }
    }
    if (instream) {
        if (Pa_IsStreamActive(instream)) {
            Pa_CloseStream(instream);
        }
    }
    if (outstream) {
        if (Pa_IsStreamActive(outstream)) {
            Pa_CloseStream(outstream);
        }
    }
    if ( /* Portaudio is initialized */ audio->init_count) {
        
        while (audio->init_count) {
            if (free_audio(audio) != paNoError) {
                audio->init_count = 0;
                break;
            }
        }
    }
    /* Here, we have the entire audio system shut down. */
    
    /* Started the audio system fresh */
    if (init_audio(audio) != paNoError) {
        printf("audio.cpp > restart_audio(): Pa_Initialize() failed!\n\n");
    }
    
    /* Here, we've loaded all new audio devices because we've restarted Portaudio */
    
    audio_refresh_device_list(audio);

    
    if (!check_input_device(audio->audio_default_input_device)) {
        /* Here, our DEFAULT input device WON'T work. */
        audio->active_audio_input_device = -1;
        audio->audio_default_input_device = -1;
        audio->audio_input_device = -1;
        /* Notify application that we have no MIKE, Magic or otherwise */
    } else {
        audio_devs_available++;
    }
    
    
    if (!check_output_device(audio->audio_default_output_device)) {
        /* Here, we have NO default OUTPUT device. */
        audio->audio_output_device = -1;
        audio->active_audio_output_device = -1;
        audio->audio_default_output_device = -1;
        
        /* No output device means possible way to play ring */
        audio->audio_ring_device = -1;
        audio->active_audio_ring_device = -1;
        audio->audio_default_ring_device = -1;
        
        /* Notify Application that we don't have an OUTPUT device */
    } else {
        audio_devs_available++;
    }
    
    if (audio_devs_available == 2) {
        /* We have established that we have a place to play audio and from which we
         can RECEIVE audio */
        
        if ( audio->audio_input_device == -1) { /* If no input device was chosen specifically */
            audio->audio_input_device = audio->active_audio_input_device = audio->audio_default_input_device;
        } else { /* There is a input device requested in audio->audio_input_device */
            if ( check_input_device(audio->audio_input_device)) { // The requested device will work
                audio->active_audio_input_device = audio->audio_input_device;
            } else { // The requested device will NOT work
                audio->audio_input_device = audio->active_audio_input_device = audio->audio_default_input_device;
            }
        }
        
        if ( /* If no ring device is specified */ audio->audio_ring_device == -1) {
            audio->audio_ring_device = audio->active_audio_ring_device = audio->audio_default_ring_device;
        } else {
            if ( check_output_device(audio->audio_ring_device)) { // The requested device will work
                audio->active_audio_ring_device = audio->audio_ring_device;
            } else { // The requested device will NOT work
                audio->audio_ring_device = audio->active_audio_ring_device = audio->audio_default_ring_device;
            }
        }
        
        if ( /* If no output device was chosen specifically */ audio->audio_output_device == -1) {
            audio->audio_output_device = audio->active_audio_output_device = audio->audio_default_output_device;
        } else {
            if ( check_output_device(audio->audio_output_device)) { // The requested device will work
                audio->active_audio_output_device = audio->audio_output_device;
            } else { // The requested device will NOT work
                audio->audio_output_device = audio->active_audio_output_device = audio->audio_default_output_device;
            }
        }
        
        
        /* Devices are selected, fire up streams */
        
        
        /* Notify Application of device change */
        if (audio->audio_device_change != 0) {
            if (audio->active_audio_input_device != in_dev ||
                audio->active_audio_output_device != out_dev ||
                audio->active_audio_ring_device != ring_dev) {
                audio->audio_device_change(audio->active_audio_input_device,
                   audio->active_audio_output_device,
                   audio->active_audio_ring_device,
                   audio->json_audio_device_list);
            }
        }
        
        // Creates an output stream
        for (i = 0; i < 2; i++) {
            pasp_out.device = audio->active_audio_output_device;
            pasp_out.channelCount = 1;
            pasp_out.hostApiSpecificStreamInfo = 0;
            pasp_out.sampleFormat = paInt16;
            pasp_out.suggestedLatency = Pa_GetDeviceInfo(pasp_out.device)->defaultLowOutputLatency;
            err = Pa_OpenStream(&outstream, 0, &pasp_out, 8000, 160, paDitherOff|paClipOff, &audio_out, audio);
            if (err == paNoError) break;
            Sleep(20);
        }
        if (err != paNoError) {
            printf("audio.cpp > restart_audio(): Pa_OpenStream() failed to open OUTPUT stream.\n");
            return;
        }
        err = Pa_StartStream(outstream);
        if (err != paNoError) {
            printf("audio.cpp > restart_audio(): Pa_StartStream() failed.\n");
            return;
        } else {
            audio->outstream = outstream;
//            printf("audio.cpp > restart_audio(): OUTPUT STREAM STARTED!!\n");

            /* Reset all AEC buffers */
//            audio->aec_write_position = 0;
//            memset(audio->aec_amp_diff_list, 0, 32000);
//            memset(audio->audio_played, 0, 32000);
            
        }
        
        
        if (audio->active_audio_ring_device != audio->active_audio_output_device) {
            if (audio->audio_ringing == 1) {
                // Creates a RINGING stream
                audio->frames_rang = 0;
                audio->ringing_silence = 0;
                audio->ring_toggle_timer = time(0) + 3;
                for (i = 0; i < 2; i++) {
                    pasp_ring.device = audio->active_audio_ring_device;
                    pasp_ring.channelCount = 1;
                    pasp_ring.hostApiSpecificStreamInfo = 0;
                    pasp_ring.sampleFormat = paInt16;
                    pasp_ring.suggestedLatency = Pa_GetDeviceInfo(pasp_ring.device)->defaultLowOutputLatency;
                    err = Pa_OpenStream(&ring_stream, 0, &pasp_ring, 8000, 160, paDitherOff|paClipOff, &ring_callback, audio);
                    if (err == paNoError) break;
                    Sleep(20);
                }
                if (err != paNoError) {
                    printf("audio.cpp > restart_audio(): Pa_OpenStream() failed to open RING stream.\n");
                    return;
                }
                err = Pa_StartStream(ring_stream);
                if (err != paNoError) {
                    printf("audio.cpp > restart_audio(): Pa_StartStream() failed on RING stream.\n");
                    return;
                } else {
                    audio->frames_rang = 0;
                    audio->ringing_silence = 0;
                    audio->ring_toggle_timer = time(0) + 3;
//                    printf("audio.cpp > restart_audio(): RING STREAM STARTED!!\n");
                    audio->ring_stream = ring_stream;
                }
            }
        }
        
        
        
        
        
        // Create an input stream
        for (i = 0; i < 2; i++) {
            pasp_in.device = audio->active_audio_input_device;
            pasp_in.channelCount = 1;
            pasp_in.hostApiSpecificStreamInfo = 0;
            pasp_in.sampleFormat = paInt16;
            pasp_in.suggestedLatency = Pa_GetDeviceInfo(pasp_in.device)->defaultLowInputLatency;
//            err = Pa_OpenStream(&instream, &pasp_in, 0, 8000, 160, paDitherOff|paClipOff, &audio_in, audio);
            err = Pa_OpenStream(&instream, &pasp_in, 0, 8000, 160, paDitherOff|paClipOff, 0, audio);
            if (err == paNoError) break;
            Sleep(20);
        }
        if (err != paNoError) {
            printf("audio.cpp > restart_audio(): Pa_OpenStream() failed to open INPUT stream.\n");
            return;
        }
        err = Pa_StartStream(instream);
        if (err != paNoError) {
            printf("audio.cpp > restart_audio(): Pa_StartStream() failed on RING stream.\n");
            return;
        } else {
//            printf("audio.cpp > restart_audio(): INPUT STREAM STARTED!!\n");
            audio->instream = instream;
        }
    }
    
    db_lock_mutex(&audio->buffer_mutex);
    flush_all_audio_buffers(audio);
    db_unlock_mutex(&audio->buffer_mutex);
    audio->audio_timeout = time(0) + 20;
    return;
}
    
    
    
    
    
    
    
/* Audio thread entry point */
#ifdef __APPLE__
void *audio_thread( void *param ) {
#else
DWORD WINAPI audio_thread( void *param ) {
#endif

    struct _audio_buffer **ppaudio_buffer = (struct _audio_buffer **)param;
    struct _audio_buffer *audio = 0;
    int active_streams = 0;
    time_t active_stream_checker = 0, device_check_timer = 0;
    int restart_due_to_error = 0;
    
    if (!ppaudio_buffer) {
        audio_alive = 0;
        audio_run_state = 0;
        return 0;
    }
    
    audio = *ppaudio_buffer;
    audio->sound_list[0].length = (sizeof(ringtone) / 2);
    audio->sound_list[0].pointer = (short *)ringtone;
    audio->sound_list[1].length = (sizeof(dialtone) / 2);
    audio->sound_list[1].pointer = (short *)dialtone;
    audio->sound_list[2].length = (sizeof(intercomring) / 2);
    audio->sound_list[2].pointer = (short *)intercomring;
    audio->sound_list[3].length = (sizeof(intercom_in) / 2);
    audio->sound_list[3].pointer = (short *)intercom_in; // This is sampled at 44.1kHz.  Need to adapt for that!
    audio->sound_list[4].length = (sizeof(sound_sample) / 2);
    audio->sound_list[4].pointer = (short *)sound_sample;
    audio->sound_list[5].length = (sizeof(answerpark) / 2);
    audio->sound_list[5].pointer = (short *)answerpark;

    audio->intercom_beep_offset = 0;
    audio->intercom_beep_status = 0;
    
    memset(audio->connection_list, 0, sizeof(struct _incoming_connection) * UDP_MAX_CONNECTIONS);
    
    db_create_mutex(&audio->buffer_mutex);
    audio->clock_sync = db_create_sem((char *)"audio_clock_sync");
    
    audio->audio_timeout = 0;
    
    audio->scan_for_devices = 0;
    
    audio->input_status = paComplete;
    audio->output_status = paComplete;
    audio->json_audio_device_list = 0;
    audio->ring_status = paComplete;
    audio->ring_stream = 0;
    
    
    
    restart_audio(audio);
    audio->active_stream_count = 0;
    
    
    
    
    
    
    
    if (audio_run_state && audio->audio_ready) {
        /* Fire callback to let everyone know the audio is ready. */
        audio->audio_ready( audio->audio_ready_data );
    }
    
    
    
    
    
    
    active_stream_checker = time(0) + 5;
    device_check_timer = time(0) + 2;
    
    /* Audio thread loop run loop audio loop */
    
    while (audio_run_state) {
        /*
         This is a timer that periodically counts audio (data) streams being handled by network.cpp
         ************************************************************************************************************/
        if (time(0) > active_stream_checker) {
            db_lock_mutex(&audio->buffer_mutex);
            active_streams = get_stream_count(audio);
            audio->active_stream_count = active_streams;
            db_unlock_mutex(&audio->buffer_mutex);
            //printf("audio.cpp > audio_thread(): %d active streams.\n", active_streams);
            active_stream_checker = time(0) + 5;
        }
        /************************************************************************************************************/

        
        
        if (time(0) > device_check_timer) {
            PaError err = Pa_Initialize();
            if (err == paNoError) {
                if (audio_refresh_device_list(audio)) {
                    if (audio->audio_device_change) {
                        audio->audio_device_change(
                           audio->active_audio_input_device,
                           audio->active_audio_output_device,
                           audio->active_audio_ring_device,
                           audio->json_audio_device_list);
                    }
                }
                Pa_Terminate();
            }
            device_check_timer = time(0) + 2;
        }
        
        
        if (audio->ringing) { // The application has told us to ring.
            if (audio->active_stream_count == 0) { // We have no network streams
                // Here we should really be ringing...
                if (audio->audio_ringing == 0) {

                    
                    audio->audio_ringing = 1;
                    audio->ring_toggle_timer = time(0) + 3;
                    audio->ringing_silence = 0;
                    audio->ring_sample_ptr = 0;
                    audio->frames_rang = 0;

                    if ( /* The ring device is NOT the OUTPUT device */
                        audio->active_audio_ring_device != audio->active_audio_output_device) {
                        printf("Ring device is NOT equal to OUTPUT device, restarting audio...\n");
                        restart_audio(audio);

                    } else /* The RING device IS the OUTPUT device */ {
                        if (!audio->instream || !audio->outstream) {// If the streams aren't open
                            printf("Setting audio->audio_ringing to 1 and restarting audio.\n");
                            restart_audio(audio);
                        }
                    }
                    
//                    if (!audio->instream || !audio->outstream) {// If the streams aren't open
//                        printf("Setting audio->audio_ringing to 1 and restarting audio.\n");
//                        restart_audio(audio);
//                    } else {
//                        // There are streams
//                        if (audio->active_audio_ring_device != audio->active_audio_output_device) {
//                            printf("Ring device is NOT equal to OUTPUT device, restarting audio...\n");
//                            restart_audio(audio);
//                        }
//                    }
                    

                    
                    
                }
                
            } else { // We HAVE active network streams
                if (audio->audio_ringing == 1) {
                    if (audio->ring_stream != 0) {
                        /* We have a SEPARATE RINGING stream from our OUTPUT stream */
                        if (Pa_IsStreamActive(audio->ring_stream)) {
                            printf("Closing ring_stream 1\n");
                            Pa_CloseStream(audio->ring_stream);
                        }
                        audio->ring_stream = 0;
                    }
                    audio->audio_ringing = 0;
                }
            }
        } else {
            if (audio->audio_ringing == 1) {
                if (audio->ring_stream) {
                    /* We have a SEPARATE RINGING stream from our OUTPUT stream */
                    if (Pa_IsStreamActive(audio->ring_stream)) {
                        printf("Closing ring_stream 2\n");
                        Pa_CloseStream(audio->ring_stream);
                    }
                    audio->ring_stream = 0;
                }
                audio->audio_ringing = 0;
            }
        }
        
        

        if (audio->audio_input_device != audio->active_audio_input_device ||
            audio->audio_output_device != audio->active_audio_output_device ||
            audio->audio_ring_device != audio->active_audio_ring_device) {
            restart_audio(audio);
        }
        

        if (audio->instream) {
            if (!Pa_IsStreamActive(audio->instream)) {
                printf("instream error...\n");
                restart_due_to_error = 1;
            }
        }
        
        if (audio->outstream && !restart_due_to_error) {
            if (!Pa_IsStreamActive(audio->outstream)) {
                printf("outstream error...\n");
                restart_due_to_error = 1;
            }
        }
        
        if (audio->ring_stream && !restart_due_to_error) {
            if (!Pa_IsStreamActive(audio->ring_stream)) {
                printf("audio->ring_stream is NOT showing active, restarting audio..\n");
                restart_due_to_error = 1;
            }
        }
        if (restart_due_to_error) {
            restart_due_to_error = 0;
            printf("Restarting audio due to error.\n");
            restart_audio(audio);
        }

        
        
        if (time(0) < audio->audio_timeout) { // If the audio engine should get ready
            if (!audio->outstream || !audio->instream) {
                printf("Restarting audio due to time being set!\n");
                restart_audio(audio);
            }
        } else { // Time is GREATER than audio timeout
            
            if (audio->ring_stream != 0 || audio->instream != 0 || audio->outstream != 0 ) {
                if (!audio->ringing && audio->active_stream_count == 0) {
                    if (Pa_IsStreamActive(audio->instream)) {
                        Pa_CloseStream(audio->instream);
                    }
                    if (Pa_IsStreamActive(audio->outstream)) {
                        Pa_CloseStream(audio->outstream);
                    }
                    if (Pa_IsStreamActive(audio->ring_stream)) {
                        Pa_CloseStream(audio->ring_stream);
                    }
                    audio->instream = 0;
                    audio->outstream = 0;
                    audio->ring_stream = 0;
                    free_audio(audio);
                }
            }
        }
        Sleep(20);
    }
    

    db_destroy_mutex(&audio->buffer_mutex);
#ifdef __APPLE__
    return &audio_thread_return_value;
#else
	return audio_thread_return_value;
#endif

}

int get_stream_count( struct _audio_buffer *audio ) {
    int connection_count = 0;
    size_t i = 0;
    time_t call_time = time(0);
    for (; i < UDP_MAX_CONNECTIONS; i++) {
        if ( call_time - audio->connection_list[i].inuse <= RTP_AUDIO_TIMEOUT ) {
            connection_count++;
        }
    }
    return connection_count;
}


int set_ringer_volume( int percentage ) {
    ring_volume = ((double)percentage/100.0f);
    return percentage;
}

int get_ringer_volume( void ) {
    int retval = (int)(ring_volume * 100);
    return retval;
}


int set_volume( int percentage ) {
    volume = ((float)percentage/100.0f);
    return percentage;
}

int set_gain( int percentage ) {
    gain = ((float)percentage/100.0f);
    return percentage;
}

int get_volume( void ) {
    return (int)(volume * 100.0f);
}

int get_gain( void ) {
    return (int)(gain * 100.0f);
}

/* Returns the unique identifier for this stream */
AUDIO_STREAM_ID start_audio_stream( struct _audio_buffer *audio, char *sip_call_id, char *ipaddress, unsigned short port, AUDIO_STREAM_ID myssrc) {
    int i = 0;
    struct _incoming_connection *last_empty_connection = 0;
    time_t current_time = time(0);
    if (!ipaddress) return 0;
    db_lock_mutex(&audio->buffer_mutex);
    for (i = 0; i < UDP_MAX_CONNECTIONS; i++) {
        if ( (current_time - audio->connection_list[i].inuse) > RTP_AUDIO_TIMEOUT ) {
            last_empty_connection = &audio->connection_list[i];
        }
    }
    if (last_empty_connection) {
        memset(last_empty_connection->incoming_samples, 0, (sizeof(struct _sample) * AUDIO_BUFFER_SIZE) );
        memset(last_empty_connection->outbound_samples, 0, (sizeof(struct _sample) * AUDIO_BUFFER_SIZE) );
        memset(last_empty_connection, 0, sizeof(struct _incoming_connection));
        last_empty_connection->volume = 1;
        last_empty_connection->media_time_stamp = 20000;
        last_empty_connection->media_index = 1;
        last_empty_connection->started_at = time(0);
        sprintf(last_empty_connection->sip_call_id, "%.*s", (( strlen(sip_call_id)>499)?499:(int)strlen(sip_call_id)), sip_call_id);
		memset(&last_empty_connection->client_address, 0, sizeof(struct sockaddr_in));
        if (db_inet_pton(AF_INET, ipaddress, &last_empty_connection->client_address.sin_addr) ) {
            last_empty_connection->last_sequence = 0;
            last_empty_connection->last_time_stamp = 0;
			last_empty_connection->client_address.sin_family = AF_INET;
            last_empty_connection->client_address.sin_port = htons(port);
            last_empty_connection->inuse = current_time;
            last_empty_connection->audio_stream_id = myssrc;
            last_empty_connection->their_audio_stream_id = 0;
            audio->active_stream_count++;
        } else {
            printf("\t*** audio stream NOT started for IP:port \"%s:%u\"!\n", ipaddress, port);
        }
    } else {
        printf("\t*** audio stream NOT started because we have no connections available!\n");
    }
    db_unlock_mutex(&audio->buffer_mutex);
//    printf("Done with start_audio_stream()!\n");
    return myssrc;
}

/* Returns 1 if the stream was successfully stopped */
int stop_audio_stream(struct _audio_buffer *audio,  char *sipcallid ) {
    int retval = 0, i = 0;
    time_t current_time = time(0);
    db_lock_mutex(&audio->buffer_mutex);
    for (i = 0; i < UDP_MAX_CONNECTIONS; i++) {
        if ( (current_time - audio->connection_list[i].inuse) <= RTP_AUDIO_TIMEOUT ) {
            if (!strcmp(audio->connection_list[i].sip_call_id, sipcallid) ) {
               //printf("stop_audio_stream(): Call-ID=\"%s\"\n", sipcallid);
                audio->active_stream_count--;
                audio->connection_list[i].inuse = 0;
                retval = 1;
            }
        }
    }
    db_unlock_mutex(&audio->buffer_mutex);
    return retval;
}

void reset_audio_connection( struct _incoming_connection *connection_buffer ) {
    memset(connection_buffer, 0, sizeof(struct _incoming_connection));
    connection_buffer->volume = 1;
    connection_buffer->their_audio_stream_id = 0;
    connection_buffer->audio_stream_id = 0;
    connection_buffer->sip_call_id[0] = 0;
    connection_buffer->connectionnumber = 0;
    connection_buffer->media_time_stamp = 20000;
    connection_buffer->media_index = 1;
    memset(&connection_buffer->client_address, 0, sizeof(struct sockaddr_in));
}

AUDIO_STREAM_LIST *get_active_streams(struct _audio_buffer *audio, size_t *listsize) {
    AUDIO_STREAM_LIST *asl = (AUDIO_STREAM_LIST *)MEMORY::calloc_(UDP_MAX_CONNECTIONS, sizeof(AUDIO_STREAM_LIST), (char *)"audio.c > get_active_streams()");
    int i = 0, j = 0;
    time_t current_time = time(0);
    if (!listsize || !audio) return 0;
    *listsize = 0;
    db_lock_mutex(&audio->buffer_mutex);
    for (; i < UDP_MAX_CONNECTIONS; i++) {
        if ( (current_time - audio->connection_list[i].inuse) <= RTP_AUDIO_TIMEOUT  ) {
            asl[j].client.sin_addr.s_addr = audio->connection_list[i].client_address.sin_addr.s_addr;
            asl[j].client.sin_port = audio->connection_list[i].client_address.sin_port;
            asl[j].port = ntohs(asl[j].client.sin_port);
            asl[j].ipaddress[0] = 0;
            strcpy(asl[j].sip_call_id, audio->connection_list[i].sip_call_id);
            db_inet_ntop(AF_INET, &asl[j].client.sin_addr, asl[j].ipaddress, 100);
            asl[j].id = audio->connection_list[i].audio_stream_id;
            asl[j].senderid = audio->connection_list[i].their_audio_stream_id;
            asl[j].time_active = audio->connection_list[i].started_at;
            asl[j].connection_number = j;
            j++;
        }
    }
    db_unlock_mutex(&audio->buffer_mutex);
    if (j > 0) {
        *listsize = j;
        return asl;
    }
    MEMORY::free_(asl);
    return 0;
}

int audio_set_input_device( struct _audio_buffer *audio, char *device_name ) {
    int i = 0;
    if (!device_name) {
        audio->audio_input_device = audio->audio_default_input_device;
        return audio->audio_input_device;
    }
    for (; i < audio->device_count; i++) {
        if (audio->audio_devices[i].is_input) {
            if (!strcmp(audio->audio_devices[i].device_name, device_name)) {
                printf("audio.c > audio_set_input_device(): Set input device to \"%s\" (%d)\n", audio->audio_devices[i].device_name, i);
                audio->audio_input_device = i;
                return i;
            }
        }
    }
    audio->audio_input_device = audio->audio_default_input_device;
    return audio->audio_input_device;
}

int audio_set_output_device(  struct _audio_buffer *audio, char *device_name) {
    int i = 0;
    if (!device_name) {
        audio->audio_output_device = audio->audio_default_output_device;
        return audio->audio_output_device;
    }
    for (; i < audio->device_count; i++) {
        if (audio->audio_devices[i].is_output) {
            if (!strcmp(audio->audio_devices[i].device_name, device_name)) {
                printf("audio.c > audio_set_output_device(): Set output device to \"%s\" (%d)\n", audio->audio_devices[i].device_name, i);
                audio->audio_output_device = i;
                return i;
            }
        }
    }
    audio->audio_output_device = audio->audio_default_output_device;
    return audio->audio_output_device;
}

int audio_set_ring_device(struct _audio_buffer *audio, char *device_name) {
	int i = 0;
	if (!device_name) {
		audio->audio_ring_device = audio->audio_default_ring_device;
		return audio->audio_ring_device;
	}
	for (; i < audio->device_count; i++) {
		if (audio->audio_devices[i].is_output) {
			if (!strcmp(audio->audio_devices[i].device_name, device_name)) {
				printf("audio.c > audio_set_output_device(): Set ring device to \"%s\" (%d)\n", audio->audio_devices[i].device_name, i);
				audio->audio_ring_device = i;
				return i;
			}
		}
	}
	audio->audio_ring_device = audio->audio_default_ring_device;
	return audio->audio_ring_device;
}



void flush_all_audio_buffers( struct _audio_buffer *audio ) {
    int i = 0;
//    printf("flush_all_audio_buffers()\n");
    memset(audio->samples_to_play, 0, (sizeof(struct _sample) * AUDIO_BUFFER_SIZE) );
    for (i = 0; i < UDP_MAX_CONNECTIONS; i++) {
        memset(audio->connection_list[i].incoming_samples, 0, (sizeof(struct _sample) * AUDIO_BUFFER_SIZE) );
        memset(audio->connection_list[i].outbound_samples, 0, (sizeof(struct _sample) * AUDIO_BUFFER_SIZE) );
        audio->connection_list[i].volume = 1;
        audio->connection_list[i].readblock = 0;
        audio->connection_list[i].writeblock = 0;
        audio->connection_list[i].writeblock_out = 0;
        audio->connection_list[i].readblock_out = 0;
    }
    audio->writeblock = 0;
    audio->readblock = 0;
}

















