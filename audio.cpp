#include "audio.h"
#include <sys/mman.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define GET_FROM_DATA(data, pos, type) *(type*)&data[pos]
#define P_ERR(err, func) if((err = func) < 0) {std::cout << snd_strerror(err) << std::endl; return;}

namespace ev3media {
	// sound

	sound::sound() {
		available = false;
	}

	sound::sound(int fd) {
		available = false;
		if(fd >= 0) {
			size_t wav_len = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);

			uint8_t* wav_data = (uint8_t*)mmap(nullptr, wav_len, PROT_READ, MAP_PRIVATE, fd, 0);
			load((const uint8_t*)(wav_data), wav_len);
			munmap(wav_data, 0);
		}
	}

	sound::sound(const uint8_t* wav_data, size_t wav_len) {
		available = false;
		load(wav_data, wav_len);
	}

	sound::sound(const uint8_t* data, size_t size, int format, int rate, int channels) {
		available = false;
		convert(data, size, format, rate, channels);
	}

	void sound::load(const uint8_t* wav_data, size_t wav_len) {
		if(memcmp(wav_data, "RIFF", 4) != 0 || wav_len < 44) {
			return;
		}
		if(*(uint16_t*)(&wav_data[20]) != 1) {
			return;
		}

		uint32_t src_samplerate = GET_FROM_DATA(wav_data, 24, uint32_t);
		uint16_t src_format = GET_FROM_DATA(wav_data, 34, uint16_t);
		uint16_t src_channels = GET_FROM_DATA(wav_data, 22, uint16_t);
		uint16_t src_frame_size = GET_FROM_DATA(wav_data, 32, uint16_t);

		convert(&wav_data[44], *(uint32_t*)(&wav_data[40]) / (src_channels * src_format / 8), 
			src_format, src_samplerate, src_channels);
	}

    //FIX ME
	void sound::convert(const uint8_t* data, size_t size, int format, int rate, int channels) {
		if(format == 16 && rate == 22050 && channels == 1) {
			sound_data = std::shared_ptr<uint8_t>(new uint8_t[size]);
			sound_size = size;	
			memcpy(sound_data.get(), data, size);
			available = true;
			return;
		}
	}

	bool sound::is_available() {
		return available;
	}
	
	
    size_t sound::get_data_size() {
        return sound_size;
    }
    
    std::shared_ptr<uint8_t> sound::get_data() {
        return sound_data;
    }

	// audio_player

	void audio_player::init(uint32_t max_channels) {
		sample_rate = 22050;
		init_alsa();
		
		channels_count = max_channels;
		channels = new channel_t[max_channels];
		
		async_handler_thread = std::thread(async_handler, this, &thread_running);
		async_handler_thread.detach();
	}

   	void audio_player::init_alsa() {
      		available = false;
        	snd_pcm_hw_params_t* hw_params;
		    int error = 0;

        	P_ERR(error, snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0))
        	P_ERR(error, snd_pcm_hw_params_malloc(&hw_params))
        	P_ERR(error, snd_pcm_hw_params_any(pcm_handle, hw_params))
        	P_ERR(error, snd_pcm_hw_params_set_rate_resample(pcm_handle, hw_params, 1))
        	P_ERR(error, snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED))
        	P_ERR(error, snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE))
        	P_ERR(error, snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 1))
        	P_ERR(error, snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0))
        	P_ERR(error, snd_pcm_hw_params_set_period_size(pcm_handle, hw_params, 4096/*4096 * 4*/, 0))
        	P_ERR(error, snd_pcm_hw_params(pcm_handle, hw_params))

        	snd_pcm_uframes_t pcm_buffer_size = 0;
        	snd_pcm_hw_params_get_buffer_size(hw_params, &pcm_buffer_size);

        	snd_pcm_uframes_t pcm_period_size = 0;
        	snd_pcm_hw_params_get_period_size(hw_params, &pcm_period_size, 0);

		    snd_pcm_hw_params_free(hw_params);

        	period_size = pcm_period_size;
        	buffer_size = pcm_buffer_size;
        	available = true;
	}

	audio_player::audio_player() {
		init(10);
	}

	audio_player::audio_player(uint32_t max_channels) {
		init(max_channels);
	}
	
    void audio_player::play(sound* snd, uint32_t channel) {
        if(snd != nullptr) {
            if(snd->is_available()) {
                channel_t new_data;
                memset(&new_data, 0, sizeof(channel_t));

                new_data.len = snd->get_data_size();
                new_data.data = snd->get_data();
                new_data.playing = true;
                new_data.volume = 100;
                channels[channel] = new_data;
            }
        }
    }
        
    void audio_player::pause_channel(uint32_t c, bool paused) {
        if(c < channels_count)
            channels[c].playing = !paused;
    }
    
    void audio_player::pause_all(bool paused) {
        for(uint32_t i = 0; i < channels_count; i++) {
            channels[i].playing = !paused;
        }
    }

    void audio_player::clear_channel(uint32_t channel) {
        memset(&channels[channel], 0, sizeof(channel_t));
    }
    
    void audio_player::clear_all() {
        memset(channels, 0, sizeof(channel_t) * channels_count);
    }

    void audio_player::volume_channel(uint32_t channel, float volume) {
        if(channel < channels_count)
            channels[channel].volume = (uint8_t)(volume * 100);
    }
    
    void audio_player::volume_all(float volume) {
        uint8_t formatted_volume = (uint8_t)(volume * 100);
        for(uint32_t i = 0; i < channels_count; i++) {
            channels[i].volume = formatted_volume;
        }
    }
    
    bool audio_player::is_available() {
        return available;
    }
	

	void audio_player::audio_player::free() {
		*thread_running = false;
		if(available && pcm_handle != nullptr) {
			snd_pcm_close(pcm_handle);
		}
        
		delete[] channels;
	}

	audio_player::~audio_player() {
		free();
	}

	static void mix_audio(uint8_t* dst, const uint8_t* src, uint32_t len, float volume) {
		for(uint32_t i = 0; i < len; i++) {
			dst[i] = src[i];
		}
	}

	void audio_player::async_handler(audio_player* instance, bool** running_ptr) {
	    bool running = true;
	    *running_ptr = &running;

	    uint32_t buffer_size = instance->period_size * 2;
    	uint8_t* buffer = new uint8_t[buffer_size];

    	snd_pcm_t* pcm = instance->pcm_handle;
	    int err = 0;

    	while(running) {
    		memset(buffer, 0, buffer_size);
    		for(int i = 0; i < instance->channels_count; i++) {
                channel_t* channel = &instance->channels[i];
                
                if(channel->pos < channel->len && channel->playing) {
                    uint32_t len = channel->len - channel->pos;
                    
                    if(len > buffer_size) {
	                    len = buffer_size;
	                }

                    mix_audio(buffer, &channel->data.get()[channel->pos], len, channel->volume);
                    channel->pos += len;
                }
            }

    		if(running) {
        		if((err = snd_pcm_writei(pcm, buffer, instance->period_size)) < 0) {
         	   		if(snd_pcm_recover(pcm, err, 1) < 0) {
           	     			running = false;
	                }
                }
                if(snd_pcm_avail(pcm) + instance->period_size < instance->buffer_size) {
                    		usleep(instance->period_size * 1000000 / instance->sample_rate);
                }
        	}
	    }
    	delete[] buffer;
	}
}

