#include <alsa/asoundlib.h>
#include <stdint.h>
#include <memory>
#include <thread>

namespace ev3media {

    class sound {
    public:
        sound();
        sound(int fd);
	sound(const uint8_t* wav_data, size_t wav_len);
        sound(const uint8_t* data, size_t size, int format, int rate, int channels);
        ~sound();

        bool is_available();
	//uint32_t get_len_ms();
    private:
        std::shared_ptr<uint8_t> sound_data;
        size_t sound_size;
        bool available;

        void load(const uint8_t* wav_data, size_t wav_size);
        void convert(const uint8_t* data, size_t size, int format, int rate, int channels);
    };

    class audio_player {
    public:
        audio_player();
        audio_player(uint32_t new_max_channels);
        ~audio_player();

        bool is_available();

        void set_max_channels(uint32_t new_max_channels);

        void play(sound* snd, uint32_t channel);
        
        void pause_channel(uint32_t c, bool paused);
        void pause_all(bool paused);

        void clear_channel(uint32_t channel);
        void clear_all();

        void volume_channel(uint32_t channel, float volume);
        void volume_all(float volume);

        void set_channels(int new_value);

        void free();
    private:
        struct channel_t {
            std::shared_ptr<uint8_t> data;
            uint8_t volume;
            bool paused;
            size_t pos;
            size_t len;
        };

        std::thread async_handler_thread;

        bool available;
        bool* thread_running;

        channel_t* channels;
        uint32_t channels_count;

        snd_pcm_t* pcm_handle;
        uint32_t sample_rate;
        uint32_t buffer_size;
        uint32_t period_size;

        void init(uint32_t max_channels);
        void init_alsa();

        static void async_handler(audio_player* instance, bool** running_ptr);
    };

}