#include "draw.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/fb.h>
#include <linux/input.h>


namespace ev3media {

	static void render_pixel_array(uint8_t* src, res src_res, rect src_rect, uint8_t* dst, res dst_res, uint8_t dst_bpp) {
		for (int y = 0; y < src_rect.h; y++) {
			for (int x = 0; x < src_rect.w; x++) {
				int rx = x + src_rect.x;
				int ry = y + src_rect.y;

				if (rx >= 0 && rx < dst_res.w && ry >= 0 && ry < dst_res.h) {
					int ix = (x * scr_res.w) / src_rect.w;
					int iy = (y * scr_res.h) / src_rect.h;

					if (ix >= 0 && ix < src_res.w && iy >= 0 && iy < src_res.h) {
						memset(&dst[(rx + ry * dst_res.w) * dst_bpp], src[ix + iy * src_res.w], dst_bpp);
					}
				}
			}
		}
	}

	static bool is_big_endian(void)
	{
		union {
			uint32_t i;
			char c[4];
		} bint = { 0x01020304 };

		return bint.c[0] == 1;
	}

	template<typename T> static T swap_bytes(T bytes) {
		T swaped;
		const T* src = &bytes;
		T* dst = &swaped;
		size_t size = sizeof(T);

		for (size_t i = 0; i < size; i++) {
			dst[i] = src[size - (i + 1)];
		}
		return swaped;
	}


	//bitmap

	bitmap::bitmap() {
		data = nullptr;
		resolution = { 0,0 };
		size = 0;
	}

	bitmap::bitmap(const uint8_t* pixels, res r) {
		resolution = r;
		size = r.w * r.h;
		data = new uint8_t[size];
		memcpy(data, pixels, size);
	}

	bitmap::~bitmap() {
		if (data != nullptr) delete[] data;
	}

	void bitmap::render(rect r, uint8_t* dst, res dst_res, uint8_t dst_bpp) {
		render_pixel_array(data, resolution, r, dst, dst_res, dst_bpp);
	}

	void bitmap::loadEPIC(const char* filename) {
		char signature[6];
		FILE* f = fopen(filename, "rb");

		if (f) {
			fread(&signature, sizeof(char), 6, f);

			if (memcmp(signature, "EV3PIC", 6) == 0) {
				uint32_t width = 0, height = 0;
				fread(&width, sizeof(uint32_t), 1, f);
				fread(&height, sizeof(uint32_t), 1, f);

				if (is_big_endian()) {
					width = swap_bytes<uint32_t>(width);
					height = swap_bytes<uint32_t>(height);
				}

				size_t _size = width * height;
				if (_size == 0) {
					fclose(f);
					return;
				}

				if (data != nullptr) delete[] data;
				data = new uint8_t[_size];
				resolution = { width, height };
				size = _size;

				fread(data, sizeof(uint8_t), _size, f);

				for (size_t i = 0; i < _size; i++) {
					data[i] = (data[i] & 0xF) * 17;
				}

				fclose(f);
			}
			else {
				fclose(f);
			}
		}
	}

	uint8_t* bitmap::get_data() {
		return data;
	}
	res bitmap::get_size() {
		return resolution;
	}

	void bitmap::re_create(res new_res) {
	
	}


	//renderer

	renderer::renderer() {
		current_target = nullptr;
		screen_available = false;
		screen_buffer = nullptr;
		screen_size = 0;
		screen_bpp = 0;
		screen_res = {0,0};

#ifdef _LINUX_FB_H
		int fbf = open("/dev/fb0", O_RDWR);
		if (fbf < 0)
			return;

		fb_fix_screeninfo info;
		if(ioctl(fbf, FBIOGET_FSCREENINFO, &info) < 0)
			return;

		screen_size = info.smem_len;
		screen_buffer = (uint8_t*)mmap(NULL, screen_size, PROT_READ|PROT_WRITE, MAP_SHARED, fbf, 0);

		if(screen_buffer == nullptr)
			return;

		fb_var_screeninfo var_info;

		if(ioctl(fbf, FBIOGET_VSCREENINFO, &var_info) < 0)
			return;

		screen_res = { var_info.xres, var_info.yres };
		screen_bpp = var_info.bits_per_pixel / 8;

		current_target_bytes = screen_buffer;
		current_target_bpp = screen_bpp;
		current_target_size = screen_size;
		current_target_res = screen_res;
		screen_available = true;
#endif
	}

	renderer::~renderer() {
		if (screen_buffer) {
			munmap(screen_buffer, 0);
		}
	}

	void renderer::render(renderrable* obj, rect r) {
		obj->render(r, current_target_bytes, current_target_res, current_target_bpp);
	}

	void renderer::fill_rect(rect r, color col) {
		for(int y = 0; y < r.h; y++) {
			for(int x = 0; x < r.w; x++) {
				int rx = x + r.x;
				int ry = y + r.y;

				if(rx >= 0 && rx < current_target_res.w
					&& ry >= 0 && ry < current_target_res.w)
				{
					memset(&current_target_bytes[(rx + ry*current_target_res.w) * current_target_bpp],
						col, current_target_bpp);
				}
			}
		}
	}

	void renderer::draw_rect(rect r, color col) {
		draw_line({ r.x, r.y }, { r.x + r.w, r.y }, col);
		draw_line({ r.x, r.y }, { r.x, r.y + r.h }, col);
		draw_line({ r.x + r.w, r.y }, { r.x + r.w, r.y + r.h }, col);
		draw_line({ r.x, r.y + r.h }, { r.x + r.w, r.y + r.h }, col);
	}

	void renderer::draw_line(point p1, point p2, color col, uint32_t t) {

	}

	void renderer::set_pixel(point pos, color col) {

	}

	void renderer::draw_tringle(point p1, point p2, point p3, color col) {

	}

	void renderer::fill_tringle(point p1, point p2, point p3, color col) {
		
	}

	void renderer::set_render_target(bitmap* target) {
		current_target = target;
		if (target) {
			current_target_bytes = target->get_data();
			current_target_res = target->get_size();
			current_target_bpp = 1;
			current_target_size = current_target_bpp * current_target_res.w * current_target_res.h;
		}
		else {
			current_target_bytes = screen_buffer;
			current_target_res = screen_res;
			current_target_bpp = screen_bpp;
			current_target_size = screen_size;
		}
	}

	void renderer::read_pixels(uint8_t* dst, rect r) {
		if(current_target != nullptr || screen_available) {
			for(int y = 0; y < r.h; y++) {
				for(int x = 0; x < r.w; x++) {
					int rx = x + r.x;
					int ry = y + r.y;
					int pos = x + y * r.w;
					if(rx >= 0 && rx < current_target_res.w
						&& ry >= 0 && ry < current_target_res.h)
					{
						int sum = 0;
						for(int i = 0; i < current_target_bpp; i++) {
							sum += current_target_bytes[((x + y * current_target_res.w) * current_target_bpp) + i];
						}
						dst[pos] = sum / current_target_bpp;
					}
				}
			}
		}
	}

	void renderer::clear(color col) {
		if(current_target != nullptr || screen_available)
			memset(current_target_bytes, col, current_target_size);
	}

	bitmap* renderer::get_render_target() {
		return current_target;
	}

	void renderer::set_viewport(res viewport) {

	}
}
