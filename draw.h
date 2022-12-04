#pragma once
#include <stdint.h>
#include <cstdlib>

typedef unsigned int size_t;

namespace ev3media {
	typedef uint8_t color;

	class point {
	public:
		int x;
		int y;
	};

	class rect {
	public:
		int x;
		int y;
		int w;
		int h;
	};

	class res {
	public:
		uint32_t w;
		uint32_t h;
	};

	class renderrable {
	public:
		virtual void render(rect r, uint8_t* dst, res dst_res, uint8_t dst_bpp) = 0;
	};

	class bitmap : public renderrable {
	public:
		bitmap();
		bitmap(res r);
		bitmap(const uint8_t* pixels, res r);
		~bitmap();

		void render(rect r, uint8_t* dst, res dst_res, uint8_t dst_bpp);
		void loadEPIC(const char* filename);

		uint8_t* get_data();
		res get_size();
		void re_create(res new_res);
	private:
		uint8_t* data;
		res resolution;
		size_t size;
		readEPIC(FILE* f);
	};

	class renderer {
	public:
		renderer();
		~renderer();

		void render(renderrable* obj, rect r);

		void fill_rect(rect r, color col);
		void draw_rect(rect r, color col);
		void draw_line(point p1, point p2, color col, uint32_t t = 1);
		void set_pixel(point pos, color col);

		void draw_tringle(point p1, point p2, point p3, color col);
		void fill_tringle(point p1, point p2, point p3, color col);

		void clear(color col);

		void read_pixels(uint8_t* dst, rect r);

		void set_render_target(bitmap* target);
		bitmap* get_render_target();

		void set_viewport(res viewport);
	private:
		bitmap* current_target;

		uint8_t* current_target_bytes;
		uint8_t current_target_bpp;
		size_t current_target_size;
		res current_target_res;

		bool screen_available;

		uint8_t* screen_buffer;
		uint8_t screen_bpp;
		size_t screen_size;
		res screen_res;
	};
}
