#include "init.h"
#include "loader.h"

extern char __rx_start, __rx_end;
extern char __rw_start, __rw_end;

static char rom[1048576];

static int curl_download_file(void *curl, char *url);

#define CURL_GLOBAL_SSL (1<<0)
#define CURL_GLOBAL_WIN32 (1<<1)
#define CURL_GLOBAL_ALL (CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32)
#define CURL_GLOBAL_NOTHING 0
#define CURL_GLOBAL_DEFAULT CURL_GLOBAL_ALL
#define OSTHREAD_SIZE	0x1000

void _main() {
	_osscreeninit();
	//OSScreen Rendering Stuff (for speedy loads)
	unsigned int vpadHandle;
	OSDynLoad_Acquire("vpad.rpl", &vpadHandle);
	OSDynLoad_FindExport(vpadHandle, 0, "VPADRead", &VPADRead);
	OSDynLoad_FindExport(coreinit_handle, 0, "DCFlushRange", &DCFlushRange);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenFlipBuffersEx", &OSScreenFlipBuffersEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenGetBufferSizeEx", &OSScreenGetBufferSizeEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenClearBufferEx", &OSScreenClearBufferEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenPutPixelEx", &OSScreenPutPixelEx);
	
	void*(*OSAllocFromSystem)(uint32_t size, int align);
	void(*OSFreeToSystem)(void *ptr);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	
	int (*OSCreateThread)(void *thread, void *entry, int argc, void *args, uint32_t stack,
		uint32_t stack_size, int priority, uint16_t attr);
	int (*OSResumeThread)(void *thread);
	int (*OSIsThreadTerminated)(void *thread);

	OSDynLoad_FindExport(coreinit_handle, 0, "OSCreateThread", &OSCreateThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSResumeThread", &OSResumeThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSIsThreadTerminated", &OSIsThreadTerminated);

	/* Allocate a stack for the thread */
	/* IMPORTANT: libcurl uses around 0x1000 internally so make sure to allocate more*
		for the stack to avoid crashes *0x200 isn't enough */
	void *stack = OSAllocFromSystem(0x5000, 0x10);

	/* Create the thread */
	void *thread = OSAllocFromSystem(OSTHREAD_SIZE, 8);
	int ret = OSCreateThread(thread, _mainThread, 0, (void*) 0, (uint32_t)stack + 0x1800, 0x1800, 0, 0x1A);
	if (!ret) OSFatal("Failed to create thread");
	
	/* Schedule it for execution */
	OSResumeThread(thread);
	
	while(!OSIsThreadTerminated(thread)) ; //Keep this thread alive  to do actual emulation
	
		
	if (fce_load_rom(rom) == -1) {
		OSFatal("Couldn't load rom");
	}
	fce_init();
	fce_run();

	OSFatal("FCE_Run stopped!");
}

void _mainThread(int argc, void *argv)
{
	char* leaddr = (char*)0;
	char* str, url[64];
	for (str = (char*)0x1A000000; str < (char*)0x20000000; str++) {
		/* Search for /payload which we use to find the URL */
		if(*(uint32_t*)str == 0x2F706179 && *(uint32_t*)(str + 4) == 0x6C6F6164) {
			leaddr = str;
			while(*leaddr) leaddr--;
			leaddr++;
			/* If string starts with http its likely to be correct */
			if(*(uint32_t*)leaddr == 0x68747470)
				break;
			leaddr = (char*)0;
		}
	}
	if(leaddr == (char*)0)
		OSFatal("Unable to find usable URL");
	/* Generate the boot.elf address */
	memcpy(url, leaddr, 64);
	char *ptr = url;
	while(*ptr) ptr++;
	while(*ptr != 0x2F) ptr--;
	memcpy(ptr + 1, "play.nes", 9);
	uint32_t nn_ac_handle, nsysnet_handle, libcurl_handle, nn_startupid;
	OSDynLoad_Acquire("nn_ac.rpl", &nn_ac_handle);
	OSDynLoad_Acquire("nsysnet", &nsysnet_handle);
	OSDynLoad_Acquire("nlibcurl", &libcurl_handle);

	int  (*ACInitialize)(void);
	int  (*ACGetStartupId)(uint32_t *id);
	int  (*ACConnectWithConfigId)(uint32_t id);
	int  (*socket_lib_init)(void);
	int  (*curl_global_init)(int opts);
	void*(*curl_easy_init)(void);
	void (*curl_easy_cleanup)(void *handle);

	OSDynLoad_FindExport(nn_ac_handle, 0, "ACInitialize", &ACInitialize);
	OSDynLoad_FindExport(nn_ac_handle, 0, "ACGetStartupId", &ACGetStartupId);   
	OSDynLoad_FindExport(nn_ac_handle, 0, "ACConnectWithConfigId",&ACConnectWithConfigId);
	OSDynLoad_FindExport(nsysnet_handle, 0, "socket_lib_init", &socket_lib_init);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_global_init", &curl_global_init);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_easy_init", &curl_easy_init);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_easy_cleanup", &curl_easy_cleanup);
	ACInitialize();
	ACGetStartupId(&nn_startupid);
	ACConnectWithConfigId(nn_startupid);
	socket_lib_init();
	curl_global_init(CURL_GLOBAL_ALL); 
	
	void*(*OSAllocFromSystem)(uint32_t size, int align);
	void(*OSFreeToSystem)(void *ptr);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	void *curl_handle = curl_easy_init();
	if(!curl_handle) OSFatal("cURL not initialized");
	curl_download_file(curl_handle, url);
	curl_easy_cleanup(curl_handle);
	
	void (*OSDynLoad_Release)(uint32_t handle);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSDynLoad_Release", &OSDynLoad_Release);
	OSDynLoad_Release(libcurl_handle);
}
unsigned int offset = 0x01;
static int curl_write_data_callback(void *buffer, int size, int nmemb, void *userp) {
	int insize = size * nmemb;
	memcpy(rom + offset, buffer, insize);
	offset += insize;
	return insize;
}

//Copied from the ELF loader.

#define CURLOPT_URL 10002
#define CURLOPT_WRITEFUNCTION 20011
#define CURLINFO_RESPONSE_CODE 0x200002

static int curl_download_file(void *curl, char *url) {
	uint32_t libcurl_handle;
	OSDynLoad_Acquire("nlibcurl", &libcurl_handle);
	void (*curl_easy_setopt) (void *handle, uint32_t param, void *op);
	int  (*curl_easy_perform)(void *handle);
	void (*curl_easy_getinfo)(void *handle, uint32_t param, void *info);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_easy_setopt", &curl_easy_setopt);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_easy_perform", &curl_easy_perform);
	OSDynLoad_FindExport(libcurl_handle, 0, "curl_easy_getinfo", &curl_easy_getinfo);
	curl_easy_setopt(curl, CURLOPT_URL, url); //Setup parameters for file
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_data_callback);

	int ret = curl_easy_perform(curl); //Actually download the file
	if (ret) OSFatal("curl_easy_perform returned an error");

	int resp = 404;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp);
	if(resp != 200) OSFatal("curl_easy_getinfo returned an HTTP error");
}

void _osscreeninit()
{
	void(*OSScreenInit)();
	unsigned int(*OSScreenGetBufferSizeEx)(unsigned int bufferNum);
	unsigned int(*OSScreenSetBufferEx)(unsigned int bufferNum, void * addr);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenInit", &OSScreenInit);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenGetBufferSizeEx", &OSScreenGetBufferSizeEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenSetBufferEx", &OSScreenSetBufferEx);
	//Call the Screen initilzation function.
	OSScreenInit();
	//Grab the buffer size for each screen (TV and gamepad)
	int buf0_size = OSScreenGetBufferSizeEx(0);
	int buf1_size = OSScreenGetBufferSizeEx(1);
	//Set the buffer area.
	OSScreenSetBufferEx(0, (void *)0xF4000000);
	OSScreenSetBufferEx(1, (void *)0xF4000000 + buf0_size);
}


//From here on out we're copypasting LiteNES code

//PSG.C

static byte prev_write;
static int p = 10;

inline byte psg_io_read(word address)
{
	// Joystick 1
	if (address == 0x4016) {
		if (p++ < 9) {
			return nes_key_state(p);
		}
	}
	return 0;
}

inline void psg_io_write(word address, byte data)
{
	if (address == 0x4016) {
		if ((data & 1) == 0 && prev_write == 1) {
			// strobe
			p = 0;
		}
	}
	prev_write = data & 1;
}

//PPU.C

byte ppu_sprite_palette[4][4];
bool ppu_2007_first_read;
byte ppu_addr_latch;

void* memcpy(void* dst, const void* src, uint32_t size)
{
	uint32_t i;
	for (i = 0; i < size; i++)
		((uint8_t*) dst)[i] = ((const uint8_t*) src)[i];
	return dst;
}

// PPUCTRL Functions

inline word ppu_base_nametable_address()                            { return ppu_base_nametable_addresses[ppu.PPUCTRL & 0x3];  }
inline byte ppu_vram_address_increment()                            { return common_bit_set(ppu.PPUCTRL, 2) ? 32 : 1;          }
inline word ppu_sprite_pattern_table_address()                      { return common_bit_set(ppu.PPUCTRL, 3) ? 0x1000 : 0x0000; }
inline word ppu_background_pattern_table_address()                  { return common_bit_set(ppu.PPUCTRL, 4) ? 0x1000 : 0x0000; }
inline byte ppu_sprite_height()                                     { return common_bit_set(ppu.PPUCTRL, 5) ? 16 : 8;          }
inline bool ppu_generates_nmi()                                     { return common_bit_set(ppu.PPUCTRL, 7);                   }



// PPUMASK Functions

inline bool ppu_renders_grayscale()                                 { return common_bit_set(ppu.PPUMASK, 0); }
inline bool ppu_shows_background_in_leftmost_8px()                  { return common_bit_set(ppu.PPUMASK, 1); }
inline bool ppu_shows_sprites_in_leftmost_8px()                     { return common_bit_set(ppu.PPUMASK, 2); }
inline bool ppu_shows_background()                                  { return common_bit_set(ppu.PPUMASK, 3); }
inline bool ppu_shows_sprites()                                     { return common_bit_set(ppu.PPUMASK, 4); }
inline bool ppu_intensifies_reds()                                  { return common_bit_set(ppu.PPUMASK, 5); }
inline bool ppu_intensifies_greens()                                { return common_bit_set(ppu.PPUMASK, 6); }
inline bool ppu_intensifies_blues()                                 { return common_bit_set(ppu.PPUMASK, 7); }

inline void ppu_set_renders_grayscale(bool yesno)                   { common_modify_bitb(&ppu.PPUMASK, 0, yesno); }
inline void ppu_set_shows_background_in_leftmost_8px(bool yesno)    { common_modify_bitb(&ppu.PPUMASK, 1, yesno); }
inline void ppu_set_shows_sprites_in_leftmost_8px(bool yesno)       { common_modify_bitb(&ppu.PPUMASK, 2, yesno); }
inline void ppu_set_shows_background(bool yesno)                    { common_modify_bitb(&ppu.PPUMASK, 3, yesno); }
inline void ppu_set_shows_sprites(bool yesno)                       { common_modify_bitb(&ppu.PPUMASK, 4, yesno); }
inline void ppu_set_intensifies_reds(bool yesno)                    { common_modify_bitb(&ppu.PPUMASK, 5, yesno); }
inline void ppu_set_intensifies_greens(bool yesno)                  { common_modify_bitb(&ppu.PPUMASK, 6, yesno); }
inline void ppu_set_intensifies_blues(bool yesno)                   { common_modify_bitb(&ppu.PPUMASK, 7, yesno); }



// PPUSTATUS Functions

inline bool ppu_sprite_overflow()                                   { return common_bit_set(ppu.PPUSTATUS, 5); }
inline bool ppu_sprite_0_hit()                                      { return common_bit_set(ppu.PPUSTATUS, 6); }
inline bool ppu_in_vblank()                                         { return common_bit_set(ppu.PPUSTATUS, 7); }

inline void ppu_set_sprite_overflow(bool yesno)                     { common_modify_bitb(&ppu.PPUSTATUS, 5, yesno); }
inline void ppu_set_sprite_0_hit(bool yesno)                        { common_modify_bitb(&ppu.PPUSTATUS, 6, yesno); }
inline void ppu_set_in_vblank(bool yesno)                           { common_modify_bitb(&ppu.PPUSTATUS, 7, yesno); }



// RAM

inline word ppu_get_real_ram_address(word address)
{
	if (address < 0x2000) {
		return address;
	}
	else if (address < 0x3F00) {
		if (address < 0x3000) {
			return address;
		}
		else {
			return address;// - 0x1000;
		}
	}
	else if (address < 0x4000) {
		address = 0x3F00 | (address & 0x1F);
		if (address == 0x3F10 || address == 0x3F14 || address == 0x3F18 || address == 0x3F1C)
			return address - 0x10;
		else
			return address;
	}
	return 0xFFFF;
}

inline byte ppu_ram_read(word address)
{
	return PPU_RAM[ppu_get_real_ram_address(address)];
}

inline void ppu_ram_write(word address, byte data)
{
	PPU_RAM[ppu_get_real_ram_address(address)] = data;
}


// 3F01 = 0F (00001111)
// 3F02 = 2A (00101010)
// 3F03 = 09 (00001001)
// 3F04 = 07 (00000111)
// 3F05 = 0F (00001111)
// 3F06 = 30 (00110000)
// 3F07 = 27 (00100111)
// 3F08 = 15 (00010101)
// 3F09 = 0F (00001111)
// 3F0A = 30 (00110000)
// 3F0B = 02 (00000010)
// 3F0C = 21 (00100001)
// 3F0D = 0F (00001111)
// 3F0E = 30 (00110000)
// 3F0F = 00 (00000000)
// 3F11 = 0F (00001111)
// 3F12 = 16 (00010110)
// 3F13 = 12 (00010010)
// 3F14 = 37 (00110111)
// 3F15 = 0F (00001111)
// 3F16 = 12 (00010010)
// 3F17 = 16 (00010110)
// 3F18 = 37 (00110111)
// 3F19 = 0F (00001111)
// 3F1A = 17 (00010111)
// 3F1B = 11 (00010001)
// 3F1C = 35 (00110101)
// 3F1D = 0F (00001111)
// 3F1E = 17 (00010111)
// 3F1F = 11 (00010001)
// 3F20 = 2B (00101011)


// Rendering

void ppu_draw_background_scanline(bool mirror)
{
	int tile_x;
	for (tile_x = ppu_shows_background_in_leftmost_8px() ? 0 : 1; tile_x < 32; tile_x++) {
		// Skipping off-screen pixels
		if (((tile_x << 3) - ppu.PPUSCROLL_X + (mirror ? 256 : 0)) > 256)
			continue;

		int tile_y = ppu.scanline >> 3;
		int tile_index = ppu_ram_read(ppu_base_nametable_address() + tile_x + (tile_y << 5) + (mirror ? 0x400 : 0));
		word tile_address = ppu_background_pattern_table_address() + 16 * tile_index;

		int y_in_tile = ppu.scanline & 0x7;
		byte l = ppu_ram_read(tile_address + y_in_tile);
		byte h = ppu_ram_read(tile_address + y_in_tile + 8);

		int x;
		for (x = 0; x < 8; x++) {
			byte color = ppu_l_h_addition_table[l][h][x];

			// Color 0 is transparent
			if (color != 0) {
				
				word attribute_address = (ppu_base_nametable_address() + (mirror ? 0x400 : 0) + 0x3C0 + (tile_x >> 2) + (ppu.scanline >> 5) * 8);
				bool top = (ppu.scanline % 32) < 16;
				bool left = (tile_x % 32 < 16);

				byte palette_attribute = ppu_ram_read(attribute_address);

				if (!top) {
					palette_attribute >>= 4;
				}
				if (!left) {
					palette_attribute >>= 2;
				}
				palette_attribute &= 3;

				word palette_address = 0x3F00 + (palette_attribute << 2);
				int idx = ppu_ram_read(palette_address + color);

				ppu_screen_background[(tile_x << 3) + x][ppu.scanline] = color;
				
				pixbuf_add(bg, (tile_x << 3) + x - ppu.PPUSCROLL_X + (mirror ? 256 : 0), ppu.scanline + 1, idx);
			}
		}
	}
}

void ppu_draw_sprite_scanline()
{
	int scanline_sprite_count = 0;
	int n;
	for (n = 0; n < 0x100; n += 4) {
		byte sprite_x = PPU_SPRRAM[n + 3];
		byte sprite_y = PPU_SPRRAM[n];

		// Skip if sprite not on scanline
		if (sprite_y > ppu.scanline || sprite_y + ppu_sprite_height() < ppu.scanline)
		   continue;

		scanline_sprite_count++;

		// PPU can't render > 8 sprites
		if (scanline_sprite_count > 8) {
			ppu_set_sprite_overflow(true);
			// break;
		}

		bool vflip = PPU_SPRRAM[n + 2] & 0x80;
		bool hflip = PPU_SPRRAM[n + 2] & 0x40;

		word tile_address = ppu_sprite_pattern_table_address() + 16 * PPU_SPRRAM[n + 1];
		int y_in_tile = ppu.scanline & 0x7;
		byte l = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile));
		byte h = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile) + 8);

		byte palette_attribute = PPU_SPRRAM[n + 2] & 0x3;
		word palette_address = 0x3F10 + (palette_attribute << 2);
		int x;
		for (x = 0; x < 8; x++) {
			int color = hflip ? ppu_l_h_addition_flip_table[l][h][x] : ppu_l_h_addition_table[l][h][x];

			// Color 0 is transparent
			if (color != 0) {
				int screen_x = sprite_x + x;
				int idx = ppu_ram_read(palette_address + color);
				
				if (PPU_SPRRAM[n + 2] & 0x20) {
					pixbuf_add(bbg, screen_x, sprite_y + y_in_tile + 1, idx);
				}
				else {
					pixbuf_add(fg, screen_x, sprite_y + y_in_tile + 1, idx);
				}

				// Checking sprite 0 hit
				if (ppu_shows_background() && !ppu_sprite_hit_occured && n == 0 && ppu_screen_background[screen_x][sprite_y + y_in_tile] == color) {
					ppu_set_sprite_0_hit(true);
					ppu_sprite_hit_occured = true;
				}
			}
		}
	}
}



// PPU Lifecycle

void ppu_run(int cycles)
{
	while (cycles-- > 0) {
		ppu_cycle();
	}
}

void ppu_cycle()
{
	if (!ppu.ready && cpu_clock() > 29658)
		ppu.ready = true;

	ppu.scanline++;
	if (ppu_shows_background()) {
		ppu_draw_background_scanline(false);
		ppu_draw_background_scanline(true);
	}
	
	if (ppu_shows_sprites()) ppu_draw_sprite_scanline();

	if (ppu.scanline == 241) {
		ppu_set_in_vblank(true);
		ppu_set_sprite_0_hit(false);
		cpu_interrupt();
	}
	else if (ppu.scanline == 262) {
		ppu.scanline = -1;
		ppu_sprite_hit_occured = false;
		ppu_set_in_vblank(false);
		fce_update_screen();
	}
}

inline void ppu_copy(word address, byte *source, int length)
{
	memcpy(&PPU_RAM[address], source, length);
}

inline byte ppu_io_read(word address)
{
	ppu.PPUADDR &= 0x3FFF;
	switch (address & 7) {
		case 2:
		{
			byte value = ppu.PPUSTATUS;
			ppu_set_in_vblank(false);
			ppu_set_sprite_0_hit(false);
			ppu.scroll_received_x = 0;
			ppu.PPUSCROLL = 0;
			ppu.addr_received_high_byte = 0;
			ppu_latch = value;
			ppu_addr_latch = 0;
			ppu_2007_first_read = true;
			return value;
		}
		case 4: return ppu_latch = PPU_SPRRAM[ppu.OAMADDR];
		case 7:
		{
			byte data;
			
			if (ppu.PPUADDR < 0x3F00) {
				data = ppu_latch = ppu_ram_read(ppu.PPUADDR);
			}
			else {
				data = ppu_ram_read(ppu.PPUADDR);
				ppu_latch = 0;
			}
			
			if (ppu_2007_first_read) {
				ppu_2007_first_read = false;
			}
			else {
				ppu.PPUADDR += ppu_vram_address_increment();
			}
			return data;
		}
		default:
			return 0xFF;
	}
}

inline void ppu_io_write(word address, byte data)
{
	address &= 7;
	ppu_latch = data;
	ppu.PPUADDR &= 0x3FFF;
	switch(address) {
		case 0: if (ppu.ready) ppu.PPUCTRL = data; break;
		case 1: if (ppu.ready) ppu.PPUMASK = data; break;
		case 3: ppu.OAMADDR = data; break;
		case 4: PPU_SPRRAM[ppu.OAMADDR++] = data; break;
		case 5:
		{
			if (ppu.scroll_received_x)
				ppu.PPUSCROLL_Y = data;
			else
				ppu.PPUSCROLL_X = data;

			ppu.scroll_received_x ^= 1;
			break;
		}
		case 6:
		{
			if (!ppu.ready)
				return;

			if (ppu.addr_received_high_byte)
				ppu.PPUADDR = (ppu_addr_latch << 8) + data;
			else
				ppu_addr_latch = data;

			ppu.addr_received_high_byte ^= 1;
			ppu_2007_first_read = true;
			break;
		}
		case 7:
		{
			if (ppu.PPUADDR > 0x1FFF || ppu.PPUADDR < 0x4000) {
				ppu_ram_write(ppu.PPUADDR ^ ppu.mirroring_xor, data);
				ppu_ram_write(ppu.PPUADDR, data);
			}
			else {
				ppu_ram_write(ppu.PPUADDR, data);
			}
		}
	}
	ppu_latch = data;
}

void ppu_init()
{
	ppu.PPUCTRL = ppu.PPUMASK = ppu.PPUSTATUS = ppu.OAMADDR = ppu.PPUSCROLL_X = ppu.PPUSCROLL_Y = ppu.PPUADDR = 0;
	ppu.PPUSTATUS |= 0xA0;
	ppu.PPUDATA = 0;
	ppu_2007_first_read = true;

	// Initializing low-high byte-pairs for pattern tables
	int h, l, x;
	for (h = 0; h < 0x100; h++) {
		for (l = 0; l < 0x100; l++) {
			for (x = 0; x < 8; x++) {
				ppu_l_h_addition_table[l][h][x] = (((h >> (7 - x)) & 1) << 1) | ((l >> (7 - x)) & 1);
				ppu_l_h_addition_flip_table[l][h][x] = (((h >> x) & 1) << 1) | ((l >> x) & 1);
			}
		}
	}
}

void ppu_sprram_write(byte data)
{
	PPU_SPRRAM[ppu.OAMADDR++] = data;
}

void ppu_set_background_color(byte color)
{
	nes_set_bg_color(color);
}

void ppu_set_mirroring(byte mirroring)
{
	ppu.mirroring = mirroring;
	ppu.mirroring_xor = 0x400 << mirroring;
}

//MMC.C

#define MMC_MAX_PAGE_COUNT 256

byte mmc_prg_pages[MMC_MAX_PAGE_COUNT][0x4000];
byte mmc_chr_pages[MMC_MAX_PAGE_COUNT][0x2000];
int mmc_prg_pages_number, mmc_chr_pages_number;

byte memory[0x10000];

inline byte mmc_read(word address)
{
	return memory[address];
}

inline void mmc_write(word address, byte data)
{
	switch (mmc_id) {
		case 0x3: {
			ppu_copy(0x0000, &mmc_chr_pages[data & 3][0], 0x2000);
		}
		break;
	}
	memory[address] = data;
}

inline void mmc_copy(word address, byte *source, int length)
{
	memcpy(&memory[address], source, length);
}

inline void mmc_append_chr_rom_page(byte *source)
{
	memcpy(&mmc_chr_pages[mmc_chr_pages_number++][0], source, 0x2000);
}

//MEMORY.C


byte memory_readb(word address)
{
	switch (address >> 13) {
		case 0: return cpu_ram_read(address & 0x07FF);
		case 1: return ppu_io_read(address);
		case 2: return psg_io_read(address);
		case 3: return cpu_ram_read(address & 0x1FFF);
		default: return mmc_read(address);
	}
}

void memory_writeb(word address, byte data)
{
	// DMA transfer
	int i;
	if (address == 0x4014) {
		for (i = 0; i < 256; i++) {
			ppu_sprram_write(cpu_ram_read((0x100 * data) + i));
		}
		return;
	}
	switch (address >> 13) {
		case 0: return cpu_ram_write(address & 0x07FF, data);
		case 1: return ppu_io_write(address, data);
		case 2: return psg_io_write(address, data);
		case 3: return cpu_ram_write(address & 0x1FFF, data);
		default: return mmc_write(address, data);
	}
}

word memory_readw(word address)
{
	return memory_readb(address) + (memory_readb(address + 1) << 8);
}

void memory_writew(word address, word data)
{
	memory_writeb(address, data & 0xFF);
	memory_writeb(address + 1, data >> 8);
}

//FCE.C //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// RAM stuff
void*(*memset)(void *dest, uint32_t value, uint32_t bytes);

PixelBuf bg, bbg, fg;

typedef struct {
	char signature[4];
	byte prg_block_count;
	byte chr_block_count;
	word rom_type;
	byte reserved[8];
} ines_header;

static ines_header fce_rom_header;

// FCE Lifecycle

void fce_setup_funcs() {
	unsigned int coreinit_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
}
int memcmp(void *ptr1, void *ptr2, uint32_t length)
{
	uint8_t *check1 = (uint8_t*) ptr1;
	uint8_t *check2 = (uint8_t*) ptr2;
	uint32_t i;
	for (i = 0; i < length; i++)
	{
		if (check1[i] != check2[i]) return 1;
	}

	return 0;
}
void
romread(char *rom, void *buf, int size)
{
	static int off = 0;
	memcpy(buf, rom + off, size);
	off += size;
}

int fce_load_rom(char *rom)
{
	romread(rom, &fce_rom_header, sizeof(fce_rom_header));
	if (memcmp(fce_rom_header.signature, "NES\x1A", 4)) {
		return -1;
	}

	mmc_id = ((fce_rom_header.rom_type & 0xF0) >> 4);

	int prg_size = fce_rom_header.prg_block_count * 0x4000;
	static byte buf[1048576];
	romread(rom, buf, prg_size);

	if (mmc_id == 0 || mmc_id == 3) {
		// if there is only one PRG block, we must repeat it twice
		if (fce_rom_header.prg_block_count == 1) {
			mmc_copy(0x8000, buf, 0x4000);
			mmc_copy(0xC000, buf, 0x4000);
		}
		else {
			mmc_copy(0x8000, buf, 0x8000);
		}
	}
	else {
		return -1;
	}

	// Copying CHR pages into MMC and PPU
	int i;
	for (i = 0; i < fce_rom_header.chr_block_count; i++) {
		romread(rom, buf, 0x2000);
		mmc_append_chr_rom_page(buf);

		if (i == 0) {
			ppu_copy(0x0000, buf, 0x2000);
		}
	}

	return 0;
}

void fce_init()
{
	nes_hal_init();
	cpu_init();
	ppu_init();
	ppu_set_mirroring(fce_rom_header.rom_type & 1);
	cpu_reset();
}

void wait_for_frame();

void fce_run()
{
	while(1)
	{
		//_printstr("loop");
		wait_for_frame();
			int scanlines = 262;
			while (scanlines-- > 0)
			{
				ppu_run(1);
				cpu_run(1364 / 12); // 1 scanline
			}
	}
}

// Rendering


void fce_update_screen()
{
	int idx = ppu_ram_read(0x3F00);
	nes_set_bg_color(idx);
	
	if (ppu_shows_sprites())
		nes_flush_buf(&bbg);

	if (ppu_shows_background())
		nes_flush_buf(&bg);

	if (ppu_shows_sprites())
		nes_flush_buf(&fg);

	nes_flip_display();

	pixbuf_clean(bbg);
	pixbuf_clean(bg);
	pixbuf_clean(fg);
}

//CPU-ADDRESSING.C

// CPU Addressing Modes

void cpu_address_implied()
{
}

void cpu_address_immediate()
{
	op_value = memory_readb(cpu.PC);
	cpu.PC++;

}

void cpu_address_zero_page()
{
	op_address = memory_readb(cpu.PC);
	op_value = CPU_RAM[op_address];
	cpu.PC++;
}

void cpu_address_zero_page_x()
{
	op_address = (memory_readb(cpu.PC) + cpu.X) & 0xFF;
	op_value = CPU_RAM[op_address];
	cpu.PC++;
}

void cpu_address_zero_page_y()
{
	op_address = (memory_readb(cpu.PC) + cpu.Y) & 0xFF;
	op_value = CPU_RAM[op_address];
	cpu.PC++;
}

void cpu_address_absolute()
{
	op_address = memory_readw(cpu.PC);
	op_value = memory_readb(op_address);
	cpu.PC += 2;
}

void cpu_address_absolute_x()
{
	op_address = memory_readw(cpu.PC) + cpu.X;
	op_value = memory_readb(op_address);
	cpu.PC += 2;

	if ((op_address >> 8) != (cpu.PC >> 8)) {
		op_cycles++;
	}
}

void cpu_address_absolute_y()
{
	op_address = (memory_readw(cpu.PC) + cpu.Y) & 0xFFFF;
	op_value = memory_readb(op_address);
	cpu.PC += 2;

	if ((op_address >> 8) != (cpu.PC >> 8)) {
		op_cycles++;
	}
}

void cpu_address_relative()
{
	op_address = memory_readb(cpu.PC);
	cpu.PC++;
	if (op_address & 0x80)
		op_address -= 0x100;
	op_address += cpu.PC;

	if ((op_address >> 8) != (cpu.PC >> 8)) {
		op_cycles++;
	}
}

void cpu_address_indirect()
{
	word arg_addr = memory_readw(cpu.PC);

	// The famous 6502 bug when instead of reading from $C0FF/$C100 it reads from $C0FF/$C000
	if ((arg_addr & 0xFF) == 0xFF) {
		// Buggy code
		op_address = (memory_readb(arg_addr & 0xFF00) << 8) + memory_readb(arg_addr);
	}
	else {
		// Normal code
		op_address = memory_readw(arg_addr);
	}
	cpu.PC += 2;
}

void cpu_address_indirect_x()
{
	byte arg_addr = memory_readb(cpu.PC);
	op_address = (memory_readb((arg_addr + cpu.X + 1) & 0xFF) << 8) | memory_readb((arg_addr + cpu.X) & 0xFF);
	op_value = memory_readb(op_address);
	cpu.PC++;
}

void cpu_address_indirect_y()
{
	byte arg_addr = memory_readb(cpu.PC);
	op_address = (((memory_readb((arg_addr + 1) & 0xFF) << 8) | memory_readb(arg_addr)) + cpu.Y) & 0xFFFF;
	op_value = memory_readb(op_address);
	cpu.PC++;

	if ((op_address >> 8) != (cpu.PC >> 8)) {
		op_cycles++;
	}
}

//CPU.C

// CPU Memory

inline byte cpu_ram_read(word address)
{
	return CPU_RAM[address & 0x7FF];
}

void cpu_ram_write(word address, byte data)
{
	CPU_RAM[address & 0x7FF] = data;
}



// Interrupt Addresses

inline word cpu_nmi_interrupt_address()   { return memory_readw(0xFFFA); }
inline word cpu_reset_interrupt_address() { return memory_readw(0xFFFC); }
inline word cpu_irq_interrupt_address()   { return memory_readw(0xFFFE); }



// Stack Routines

void cpu_stack_pushb(byte data) { memory_writeb(0x100 + cpu.SP--, data);       }
void cpu_stack_pushw(word data) { memory_writew(0xFF + cpu.SP, data); cpu.SP -= 2; }
byte cpu_stack_popb()           { return memory_readb(0x100 + ++cpu.SP);       }
word cpu_stack_popw()           { cpu.SP += 2; return memory_readw(0xFF + cpu.SP); }



// CPU Instructions

void ____FE____() { /* Instruction for future Extension */ }

#define cpu_flag_set(flag) common_bit_set(cpu.P, flag)
#define cpu_modify_flag(flag, value) common_modify_bitb(&cpu.P, flag, value)
#define cpu_set_flag(flag) common_set_bitb(&cpu.P, flag)
#define cpu_unset_flag(flag) common_unset_bitb(&cpu.P, flag)

#define cpu_update_zn_flags(value) cpu.P = (cpu.P & ~(zero_flag | negative_flag)) | cpu_zn_flag_table[value]

#define cpu_branch(flag) if (flag) cpu.PC = op_address;
#define cpu_compare(reg) int result = reg - op_value; \
						 cpu_modify_flag(carry_bp, result >= 0); \
						 cpu_modify_flag(zero_bp, result == 0); \
						 cpu_modify_flag(negative_bp, (result >> 7) & 1);



// CPU Instructions

// NOP

void cpu_op_nop() {}

// Addition

void cpu_op_adc()
{
	int result = cpu.A + op_value + (cpu_flag_set(carry_bp) ? 1 : 0);
	cpu_modify_flag(carry_bp, !!(result & 0x100));
	cpu_modify_flag(overflow_bp, !!(~(cpu.A ^ op_value) & (cpu.A ^ result) & 0x80));
	cpu.A = result & 0xFF;
	cpu_update_zn_flags(cpu.A);
}

// Subtraction

void cpu_op_sbc()
{
	int result = cpu.A - op_value - (cpu_flag_set(carry_bp) ? 0 : 1);
	cpu_modify_flag(carry_bp, !(result & 0x100));
	cpu_modify_flag(overflow_bp, !!((cpu.A ^ op_value) & (cpu.A ^ result) & 0x80));
	cpu.A = result & 0xFF;
	cpu_update_zn_flags(cpu.A);
}

// Bit Manipulation Operations

void cpu_op_and() { cpu_update_zn_flags(cpu.A &= op_value); }
void cpu_op_bit() { cpu_modify_flag(zero_bp, !(cpu.A & op_value)); cpu.P = (cpu.P & 0x3F) | (0xC0 & op_value); }
void cpu_op_eor() { cpu_update_zn_flags(cpu.A ^= op_value); }
void cpu_op_ora() { cpu_update_zn_flags(cpu.A |= op_value); }
void cpu_op_asla()
{
	cpu_modify_flag(carry_bp, cpu.A & 0x80);
	cpu.A <<= 1;
	cpu_update_zn_flags(cpu.A);
}
void cpu_op_asl()
{
	cpu_modify_flag(carry_bp, op_value & 0x80);
	op_value <<= 1;
	op_value &= 0xFF;
	cpu_update_zn_flags(op_value);
	memory_writeb(op_address, op_value);
}
void cpu_op_lsra()
{
	int value = cpu.A >> 1;
	cpu_modify_flag(carry_bp, cpu.A & 0x01);
	cpu.A = value & 0xFF;
	cpu_update_zn_flags(value);
}
void cpu_op_lsr()
{
	cpu_modify_flag(carry_bp, op_value & 0x01);
	op_value >>= 1;
	op_value &= 0xFF;
	memory_writeb(op_address, op_value);
	cpu_update_zn_flags(op_value);
}

void cpu_op_rola()
{
	int value = cpu.A << 1;
	value |= cpu_flag_set(carry_bp) ? 1 : 0;
	cpu_modify_flag(carry_bp, value > 0xFF);
	cpu.A = value & 0xFF;
	cpu_update_zn_flags(cpu.A);
}
void cpu_op_rol()
{
	op_value <<= 1;
	op_value |= cpu_flag_set(carry_bp) ? 1 : 0;
	cpu_modify_flag(carry_bp, op_value > 0xFF);
	op_value &= 0xFF;
	memory_writeb(op_address, op_value);
	cpu_update_zn_flags(op_value);
}
void cpu_op_rora()
{
	unsigned char carry = cpu_flag_set(carry_bp);
	cpu_modify_flag(carry_bp, cpu.A & 0x01);
	cpu.A = (cpu.A >> 1) | (carry << 7);
	cpu_modify_flag(zero_bp, cpu.A == 0);
	cpu_modify_flag(negative_bp, !!carry);
}
void cpu_op_ror()
{
	unsigned char carry = cpu_flag_set(carry_bp);
	cpu_modify_flag(carry_bp, op_value & 0x01);
	op_value = ((op_value >> 1) | (carry << 7)) & 0xFF;
	cpu_modify_flag(zero_bp, op_value == 0);
	cpu_modify_flag(negative_bp, !!carry);
	memory_writeb(op_address, op_value);
}

// Loading

void cpu_op_lda() { cpu_update_zn_flags(cpu.A = op_value); }
void cpu_op_ldx() { cpu_update_zn_flags(cpu.X = op_value); }
void cpu_op_ldy() { cpu_update_zn_flags(cpu.Y = op_value); }

// Storing

void cpu_op_sta() { memory_writeb(op_address, cpu.A); }
void cpu_op_stx() { memory_writeb(op_address, cpu.X); }
void cpu_op_sty() { memory_writeb(op_address, cpu.Y); }

// Transfering

void cpu_op_tax() { cpu_update_zn_flags(cpu.X = cpu.A);  }
void cpu_op_txa() { cpu_update_zn_flags(cpu.A = cpu.X);  }
void cpu_op_tay() { cpu_update_zn_flags(cpu.Y = cpu.A);  }
void cpu_op_tya() { cpu_update_zn_flags(cpu.A = cpu.Y);  }
void cpu_op_tsx() { cpu_update_zn_flags(cpu.X = cpu.SP); }
void cpu_op_txs() { cpu.SP = cpu.X; }

// Branching Positive

void cpu_op_bcs() { cpu_branch(cpu_flag_set(carry_bp));     }
void cpu_op_beq() { cpu_branch(cpu_flag_set(zero_bp));      }
void cpu_op_bmi() { cpu_branch(cpu_flag_set(negative_bp));  }
void cpu_op_bvs() { cpu_branch(cpu_flag_set(overflow_bp));  }

// Branching Negative

void cpu_op_bne() { cpu_branch(!cpu_flag_set(zero_bp));     }
void cpu_op_bcc() { cpu_branch(!cpu_flag_set(carry_bp));    }
void cpu_op_bpl() { cpu_branch(!cpu_flag_set(negative_bp)); }
void cpu_op_bvc() { cpu_branch(!cpu_flag_set(overflow_bp)); }

// Jumping

void cpu_op_jmp() { cpu.PC = op_address; }

// Subroutines

void cpu_op_jsr() { cpu_stack_pushw(cpu.PC - 1); cpu.PC = op_address; }
void cpu_op_rts() { cpu.PC = cpu_stack_popw() + 1; }

// Interruptions

void cpu_op_brk() { cpu_stack_pushw(cpu.PC - 1); cpu_stack_pushb(cpu.P); cpu.P |= unused_flag | break_flag; cpu.PC = cpu_nmi_interrupt_address(); }
void cpu_op_rti() { cpu.P = cpu_stack_popb() | unused_flag; cpu.PC = cpu_stack_popw(); }

// Flags

void cpu_op_clc() { cpu_unset_flag(carry_bp);     }
void cpu_op_cld() { cpu_unset_flag(decimal_bp);   }
void cpu_op_cli() { cpu_unset_flag(interrupt_bp); }
void cpu_op_clv() { cpu_unset_flag(overflow_bp);  }
void cpu_op_sec() { cpu_set_flag(carry_bp);       }
void cpu_op_sed() { cpu_set_flag(decimal_bp);     }
void cpu_op_sei() { cpu_set_flag(interrupt_bp);   }

// Comparison

void cpu_op_cmp() { cpu_compare(cpu.A); }
void cpu_op_cpx() { cpu_compare(cpu.X); }
void cpu_op_cpy() { cpu_compare(cpu.Y); }

// Increment

void cpu_op_inc() { byte result = op_value + 1; memory_writeb(op_address, result); cpu_update_zn_flags(result); }
void cpu_op_inx() { cpu_update_zn_flags(++cpu.X); }
void cpu_op_iny() { cpu_update_zn_flags(++cpu.Y); }

// Decrement

void cpu_op_dec() { byte result = op_value - 1; memory_writeb(op_address, result); cpu_update_zn_flags(result); }
void cpu_op_dex() { cpu_update_zn_flags(--cpu.X); }
void cpu_op_dey() { cpu_update_zn_flags(--cpu.Y); }

// Stack

void cpu_op_php() { cpu_stack_pushb(cpu.P | 0x30); }
void cpu_op_pha() { cpu_stack_pushb(cpu.A); }
void cpu_op_pla() { cpu.A = cpu_stack_popb(); cpu_update_zn_flags(cpu.A); }
void cpu_op_plp() { cpu.P = (cpu_stack_popb() & 0xEF) | 0x20; }



// Extended Instruction Set

void cpu_op_aso() { cpu_op_asl(); cpu_op_ora(); }
void cpu_op_axa() { memory_writeb(op_address, cpu.A & cpu.X & (op_address >> 8)); }
void cpu_op_axs() { memory_writeb(op_address, cpu.A & cpu.X); }
void cpu_op_dcm()
{
	op_value--;
	op_value &= 0xFF;
	memory_writeb(op_address, op_value);
	cpu_op_cmp();
}
void cpu_op_ins()
{
	op_value = (op_value + 1) & 0xFF;
	memory_writeb(op_address, op_value);
	cpu_op_sbc();
}
void cpu_op_lax() { cpu_update_zn_flags(cpu.A = cpu.X = op_value); }
void cpu_op_lse() { cpu_op_lsr(); cpu_op_eor(); }
void cpu_op_rla() { cpu_op_rol(); cpu_op_and(); }
void cpu_op_rra() { cpu_op_ror(); cpu_op_adc(); }





// Base 6502 instruction set

#define CPU_OP_BIS(o, c, f, n, a) cpu_op_cycles[0x##o] = c; \
								  cpu_op_handler[0x##o] = cpu_op_##f; \
								  cpu_op_name[0x##o] = n; \
								  cpu_op_address_mode[0x##o] = cpu_address_##a; \
								  cpu_op_in_base_instruction_set[0x##o] = true;

// Not implemented instructions

#define CPU_OP_NII(o, a) cpu_op_cycles[0x##o] = 1; \
						 cpu_op_handler[0x##o] = ____FE____; \
						 cpu_op_name[0x##o] = "NOP"; \
						 cpu_op_address_mode[0x##o] = cpu_address_##a; \
						 cpu_op_in_base_instruction_set[0x##o] = false;

// Extended instruction set found in other CPUs and implemented for compatibility

#define CPU_OP_EIS(o, c, f, n, a) cpu_op_cycles[0x##o] = c; \
								  cpu_op_handler[0x##o] = cpu_op_##f; \
								  cpu_op_name[0x##o] = n; \
								  cpu_op_address_mode[0x##o] = cpu_address_##a; \
								  cpu_op_in_base_instruction_set[0x##o] = false;



// CPU Lifecycle

void cpu_init()
{
	CPU_OP_BIS(00, 7, brk, "BRK", implied)
	CPU_OP_BIS(01, 6, ora, "ORA", indirect_x)
	CPU_OP_BIS(05, 3, ora, "ORA", zero_page)
	CPU_OP_BIS(06, 5, asl, "ASL", zero_page)
	CPU_OP_BIS(08, 3, php, "PHP", implied)
	CPU_OP_BIS(09, 2, ora, "ORA", immediate)
	CPU_OP_BIS(0A, 2, asla,"ASL", implied)
	CPU_OP_BIS(0D, 4, ora, "ORA", absolute)
	CPU_OP_BIS(0E, 6, asl, "ASL", absolute)
	CPU_OP_BIS(10, 2, bpl, "BPL", relative)
	CPU_OP_BIS(11, 5, ora, "ORA", indirect_y)
	CPU_OP_BIS(15, 4, ora, "ORA", zero_page_x)
	CPU_OP_BIS(16, 6, asl, "ASL", zero_page_x)
	CPU_OP_BIS(18, 2, clc, "CLC", implied)
	CPU_OP_BIS(19, 4, ora, "ORA", absolute_y)
	CPU_OP_BIS(1D, 4, ora, "ORA", absolute_x)
	CPU_OP_BIS(1E, 7, asl, "ASL", absolute_x)
	CPU_OP_BIS(20, 6, jsr, "JSR", absolute)
	CPU_OP_BIS(21, 6, and, "AND", indirect_x)
	CPU_OP_BIS(24, 3, bit, "BIT", zero_page)
	CPU_OP_BIS(25, 3, and, "AND", zero_page)
	CPU_OP_BIS(26, 5, rol, "ROL", zero_page)
	CPU_OP_BIS(28, 4, plp, "PLP", implied)
	CPU_OP_BIS(29, 2, and, "AND", immediate)
	CPU_OP_BIS(2A, 2, rola,"ROL", implied)
	CPU_OP_BIS(2C, 4, bit, "BIT", absolute)
	CPU_OP_BIS(2D, 2, and, "AND", absolute)
	CPU_OP_BIS(2E, 6, rol, "ROL", absolute)
	CPU_OP_BIS(30, 2, bmi, "BMI", relative)
	CPU_OP_BIS(31, 5, and, "AND", indirect_y)
	CPU_OP_BIS(35, 4, and, "AND", zero_page_x)
	CPU_OP_BIS(36, 6, rol, "ROL", zero_page_x)
	CPU_OP_BIS(38, 2, sec, "SEC", implied)
	CPU_OP_BIS(39, 4, and, "AND", absolute_y)
	CPU_OP_BIS(3D, 4, and, "AND", absolute_x)
	CPU_OP_BIS(3E, 7, rol, "ROL", absolute_x)
	CPU_OP_BIS(40, 6, rti, "RTI", implied)
	CPU_OP_BIS(41, 6, eor, "EOR", indirect_x)
	CPU_OP_BIS(45, 3, eor, "EOR", zero_page)
	CPU_OP_BIS(46, 5, lsr, "LSR", zero_page)
	CPU_OP_BIS(48, 3, pha, "PHA", implied)
	CPU_OP_BIS(49, 2, eor, "EOR", immediate)
	CPU_OP_BIS(4A, 2, lsra,"LSR", implied)
	CPU_OP_BIS(4C, 3, jmp, "JMP", absolute)
	CPU_OP_BIS(4D, 4, eor, "EOR", absolute)
	CPU_OP_BIS(4E, 6, lsr, "LSR", absolute)
	CPU_OP_BIS(50, 2, bvc, "BVC", relative)
	CPU_OP_BIS(51, 5, eor, "EOR", indirect_y)
	CPU_OP_BIS(55, 4, eor, "EOR", zero_page_x)
	CPU_OP_BIS(56, 6, lsr, "LSR", zero_page_x)
	CPU_OP_BIS(58, 2, cli, "CLI", implied)
	CPU_OP_BIS(59, 4, eor, "EOR", absolute_y)
	CPU_OP_BIS(5D, 4, eor, "EOR", absolute_x)
	CPU_OP_BIS(5E, 7, lsr, "LSR", absolute_x)
	CPU_OP_BIS(60, 6, rts, "RTS", implied)
	CPU_OP_BIS(61, 6, adc, "ADC", indirect_x)
	CPU_OP_BIS(65, 3, adc, "ADC", zero_page)
	CPU_OP_BIS(66, 5, ror, "ROR", zero_page)
	CPU_OP_BIS(68, 4, pla, "PLA", implied)
	CPU_OP_BIS(69, 2, adc, "ADC", immediate)
	CPU_OP_BIS(6A, 2, rora,"ROR", implied)
	CPU_OP_BIS(6C, 5, jmp, "JMP", indirect)
	CPU_OP_BIS(6D, 4, adc, "ADC", absolute)
	CPU_OP_BIS(6E, 6, ror, "ROR", absolute)
	CPU_OP_BIS(70, 2, bvs, "BVS", relative)
	CPU_OP_BIS(71, 5, adc, "ADC", indirect_y)
	CPU_OP_BIS(75, 4, adc, "ADC", zero_page_x)
	CPU_OP_BIS(76, 6, ror, "ROR", zero_page_x)
	CPU_OP_BIS(78, 2, sei, "SEI", implied)
	CPU_OP_BIS(79, 4, adc, "ADC", absolute_y)
	CPU_OP_BIS(7D, 4, adc, "ADC", absolute_x)
	CPU_OP_BIS(7E, 7, ror, "ROR", absolute_x)
	CPU_OP_BIS(81, 6, sta, "STA", indirect_x)
	CPU_OP_BIS(84, 3, sty, "STY", zero_page)
	CPU_OP_BIS(85, 3, sta, "STA", zero_page)
	CPU_OP_BIS(86, 3, stx, "STX", zero_page)
	CPU_OP_BIS(88, 2, dey, "DEY", implied)
	CPU_OP_BIS(8A, 2, txa, "TXA", implied)
	CPU_OP_BIS(8C, 4, sty, "STY", absolute)
	CPU_OP_BIS(8D, 4, sta, "STA", absolute)
	CPU_OP_BIS(8E, 4, stx, "STX", absolute)
	CPU_OP_BIS(90, 2, bcc, "BCC", relative)
	CPU_OP_BIS(91, 6, sta, "STA", indirect_y)
	CPU_OP_BIS(94, 4, sty, "STY", zero_page_x)
	CPU_OP_BIS(95, 4, sta, "STA", zero_page_x)
	CPU_OP_BIS(96, 4, stx, "STX", zero_page_y)
	CPU_OP_BIS(98, 2, tya, "TYA", implied)
	CPU_OP_BIS(99, 5, sta, "STA", absolute_y)
	CPU_OP_BIS(9A, 2, txs, "TXS", implied)
	CPU_OP_BIS(9D, 5, sta, "STA", absolute_x)
	CPU_OP_BIS(A0, 2, ldy, "LDY", immediate)
	CPU_OP_BIS(A1, 6, lda, "LDA", indirect_x)
	CPU_OP_BIS(A2, 2, ldx, "LDX", immediate)
	CPU_OP_BIS(A4, 3, ldy, "LDY", zero_page)
	CPU_OP_BIS(A5, 3, lda, "LDA", zero_page)
	CPU_OP_BIS(A6, 3, ldx, "LDX", zero_page)
	CPU_OP_BIS(A8, 2, tay, "TAY", implied)
	CPU_OP_BIS(A9, 2, lda, "LDA", immediate)
	CPU_OP_BIS(AA, 2, tax, "TAX", implied)
	CPU_OP_BIS(AC, 4, ldy, "LDY", absolute)
	CPU_OP_BIS(AD, 4, lda, "LDA", absolute)
	CPU_OP_BIS(AE, 4, ldx, "LDX", absolute)
	CPU_OP_BIS(B0, 2, bcs, "BCS", relative)
	CPU_OP_BIS(B1, 5, lda, "LDA", indirect_y)
	CPU_OP_BIS(B4, 4, ldy, "LDY", zero_page_x)
	CPU_OP_BIS(B5, 4, lda, "LDA", zero_page_x)
	CPU_OP_BIS(B6, 4, ldx, "LDX", zero_page_y)
	CPU_OP_BIS(B8, 2, clv, "CLV", implied)
	CPU_OP_BIS(B9, 4, lda, "LDA", absolute_y)
	CPU_OP_BIS(BA, 2, tsx, "TSX", implied)
	CPU_OP_BIS(BC, 4, ldy, "LDY", absolute_x)
	CPU_OP_BIS(BD, 4, lda, "LDA", absolute_x)
	CPU_OP_BIS(BE, 4, ldx, "LDX", absolute_y)
	CPU_OP_BIS(C0, 2, cpy, "CPY", immediate)
	CPU_OP_BIS(C1, 6, cmp, "CMP", indirect_x)
	CPU_OP_BIS(C4, 3, cpy, "CPY", zero_page)
	CPU_OP_BIS(C5, 3, cmp, "CMP", zero_page)
	CPU_OP_BIS(C6, 5, dec, "DEC", zero_page)
	CPU_OP_BIS(C8, 2, iny, "INY", implied)
	CPU_OP_BIS(C9, 2, cmp, "CMP", immediate)
	CPU_OP_BIS(CA, 2, dex, "DEX", implied)
	CPU_OP_BIS(CC, 4, cpy, "CPY", absolute)
	CPU_OP_BIS(CD, 4, cmp, "CMP", absolute)
	CPU_OP_BIS(CE, 6, dec, "DEC", absolute)
	CPU_OP_BIS(D0, 2, bne, "BNE", relative)
	CPU_OP_BIS(D1, 5, cmp, "CMP", indirect_y)
	CPU_OP_BIS(D5, 4, cmp, "CMP", zero_page_x)
	CPU_OP_BIS(D6, 6, dec, "DEC", zero_page_x)
	CPU_OP_BIS(D8, 2, cld, "CLD", implied)
	CPU_OP_BIS(D9, 4, cmp, "CMP", absolute_y)
	CPU_OP_BIS(DD, 4, cmp, "CMP", absolute_x)
	CPU_OP_BIS(DE, 7, dec, "DEC", absolute_x)
	CPU_OP_BIS(E0, 2, cpx, "CPX", immediate)
	CPU_OP_BIS(E1, 6, sbc, "SBC", indirect_x)
	CPU_OP_BIS(E4, 3, cpx, "CPX", zero_page)
	CPU_OP_BIS(E5, 3, sbc, "SBC", zero_page)
	CPU_OP_BIS(E6, 5, inc, "INC", zero_page)
	CPU_OP_BIS(E8, 2, inx, "INX", implied)
	CPU_OP_BIS(E9, 2, sbc, "SBC", immediate)
	CPU_OP_BIS(EA, 2, nop, "NOP", implied)
	CPU_OP_BIS(EC, 4, cpx, "CPX", absolute)
	CPU_OP_BIS(ED, 4, sbc, "SBC", absolute)
	CPU_OP_BIS(EE, 6, inc, "INC", absolute)
	CPU_OP_BIS(F0, 2, beq, "BEQ", relative)
	CPU_OP_BIS(F1, 5, sbc, "SBC", indirect_y)
	CPU_OP_BIS(F5, 4, sbc, "SBC", zero_page_x)
	CPU_OP_BIS(F6, 6, inc, "INC", zero_page_x)
	CPU_OP_BIS(F8, 2, sed, "SED", implied)
	CPU_OP_BIS(F9, 4, sbc, "SBC", absolute_y)
	CPU_OP_BIS(FD, 4, sbc, "SBC", absolute_x)
	CPU_OP_BIS(FE, 7, inc, "INC", absolute_x)

	CPU_OP_EIS(03, 8, aso, "SLO", indirect_x)
	CPU_OP_EIS(07, 5, aso, "SLO", zero_page)
	CPU_OP_EIS(0F, 6, aso, "SLO", absolute)
	CPU_OP_EIS(13, 8, aso, "SLO", indirect_y)
	CPU_OP_EIS(17, 6, aso, "SLO", zero_page_x)
	CPU_OP_EIS(1B, 7, aso, "SLO", absolute_y)
	CPU_OP_EIS(1F, 7, aso, "SLO", absolute_x)
	CPU_OP_EIS(23, 8, rla, "RLA", indirect_x)
	CPU_OP_EIS(27, 5, rla, "RLA", zero_page)
	CPU_OP_EIS(2F, 6, rla, "RLA", absolute)
	CPU_OP_EIS(33, 8, rla, "RLA", indirect_y)
	CPU_OP_EIS(37, 6, rla, "RLA", zero_page_x)
	CPU_OP_EIS(3B, 7, rla, "RLA", absolute_y)
	CPU_OP_EIS(3F, 7, rla, "RLA", absolute_x)
	CPU_OP_EIS(43, 8, lse, "SRE", indirect_x)
	CPU_OP_EIS(47, 5, lse, "SRE", zero_page)
	CPU_OP_EIS(4F, 6, lse, "SRE", absolute)
	CPU_OP_EIS(53, 8, lse, "SRE", indirect_y)
	CPU_OP_EIS(57, 6, lse, "SRE", zero_page_x)
	CPU_OP_EIS(5B, 7, lse, "SRE", absolute_y)
	CPU_OP_EIS(5F, 7, lse, "SRE", absolute_x)
	CPU_OP_EIS(63, 8, rra, "RRA", indirect_x)
	CPU_OP_EIS(67, 5, rra, "RRA", zero_page)
	CPU_OP_EIS(6F, 6, rra, "RRA", absolute)
	CPU_OP_EIS(73, 8, rra, "RRA", indirect_y)
	CPU_OP_EIS(77, 6, rra, "RRA", zero_page_x)
	CPU_OP_EIS(7B, 7, rra, "RRA", absolute_y)
	CPU_OP_EIS(7F, 7, rra, "RRA", absolute_x)
	CPU_OP_EIS(83, 6, axs, "SAX", indirect_x)
	CPU_OP_EIS(87, 3, axs, "SAX", zero_page)
	CPU_OP_EIS(8F, 4, axs, "SAX", absolute)
	CPU_OP_EIS(93, 6, axa, "SAX", indirect_y)
	CPU_OP_EIS(97, 4, axs, "SAX", zero_page_y)
	CPU_OP_EIS(9F, 5, axa, "SAX", absolute_y)
	CPU_OP_EIS(A3, 6, lax, "LAX", indirect_x)
	CPU_OP_EIS(A7, 3, lax, "LAX", zero_page)
	CPU_OP_EIS(AF, 4, lax, "LAX", absolute)
	CPU_OP_EIS(B3, 5, lax, "LAX", indirect_y)
	CPU_OP_EIS(B7, 4, lax, "LAX", zero_page_y)
	CPU_OP_EIS(BF, 4, lax, "LAX", absolute_y)
	CPU_OP_EIS(C3, 8, dcm, "DCP", indirect_x)
	CPU_OP_EIS(C7, 5, dcm, "DCP", zero_page)
	CPU_OP_EIS(CF, 6, dcm, "DCP", absolute)
	CPU_OP_EIS(D3, 8, dcm, "DCP", indirect_y)
	CPU_OP_EIS(D7, 6, dcm, "DCP", zero_page_x)
	CPU_OP_EIS(DB, 7, dcm, "DCP", absolute_y)
	CPU_OP_EIS(DF, 7, dcm, "DCP", absolute_x)
	CPU_OP_EIS(E3, 8, ins, "ISB", indirect_x)
	CPU_OP_EIS(E7, 5, ins, "ISB", zero_page)
	CPU_OP_EIS(EB, 2, sbc, "SBC", immediate)
	CPU_OP_EIS(EF, 6, ins, "ISB", absolute)
	CPU_OP_EIS(F3, 8, ins, "ISB", indirect_y)
	CPU_OP_EIS(F7, 6, ins, "ISB", zero_page_x)
	CPU_OP_EIS(FB, 7, ins, "ISB", absolute_y)
	CPU_OP_EIS(FF, 7, ins, "ISB", absolute_x)

	CPU_OP_NII(04, zero_page)
	CPU_OP_NII(0C, absolute)
	CPU_OP_NII(14, zero_page_x)
	CPU_OP_NII(1A, implied)
	CPU_OP_NII(1C, absolute_x)
	CPU_OP_NII(34, zero_page_x)
	CPU_OP_NII(3A, implied)
	CPU_OP_NII(3C, absolute_x)
	CPU_OP_NII(44, zero_page)
	CPU_OP_NII(54, zero_page_x)
	CPU_OP_NII(5A, implied)
	CPU_OP_NII(5C, absolute_x)
	CPU_OP_NII(64, zero_page)
	CPU_OP_NII(74, zero_page_x)
	CPU_OP_NII(7A, implied)
	CPU_OP_NII(7C, absolute_x)
	CPU_OP_NII(80, immediate)
	CPU_OP_NII(D4, zero_page_x)
	CPU_OP_NII(DA, implied)
	CPU_OP_NII(DC, absolute_x)
	CPU_OP_NII(F4, zero_page_x)
	CPU_OP_NII(FA, implied)
	CPU_OP_NII(FC, absolute_x)

	cpu.P = 0x24;
	cpu.SP = 0x00;
	cpu.A = cpu.X = cpu.Y = 0;
}

void cpu_reset()
{
	cpu.PC = cpu_reset_interrupt_address();
	cpu.SP -= 3;
	cpu.P |= interrupt_flag;
}

void cpu_interrupt()
{
	// if (ppu_in_vblank()) {
		if (ppu_generates_nmi()) {
			cpu.P |= interrupt_flag;
			cpu_unset_flag(unused_bp);
			cpu_stack_pushw(cpu.PC);
			cpu_stack_pushb(cpu.P);
			cpu.PC = cpu_nmi_interrupt_address();
		}
	// }
}

inline unsigned long long cpu_clock()
{
	return cpu_cycles;
}

void cpu_run(long cycles)
{
	while (cycles > 0) {
		op_code = memory_readb(cpu.PC++);
		if (cpu_op_address_mode[op_code] == NULL) {
		}
		else {
			cpu_op_address_mode[op_code]();
			cpu_op_handler[op_code]();
		}
		cycles -= cpu_op_cycles[op_code] + op_cycles;
		cpu_cycles -= cpu_op_cycles[op_code] + op_cycles;
		op_cycles = 0;
	}
}

//COMMON.C

bool common_bit_set(long long value, byte position) { return value & (1L << position); }

// I could do this through non-void methods with returns in one copy,
// but this variant is slightly faster, and needs less typing in client code
#define M_common(SUFFIX, TYPE) \
	void common_set_bit##SUFFIX(TYPE *variable, byte position)    { *variable |= 1L << position;    } \
	void common_unset_bit##SUFFIX(TYPE *variable, byte position)  { *variable &= ~(1L << position); } \
	void common_toggle_bit##SUFFIX(TYPE *variable, byte position) { *variable ^= 1L << position;    } \
	void common_modify_bit##SUFFIX(TYPE *variable, byte position, bool set) \
		{ set ? common_set_bit##SUFFIX(variable, position) : common_unset_bit##SUFFIX(variable, position); }

M_common(b, byte)
M_common(w, word)
M_common(d, dword)
M_common(q, qword)



//HAL.C (our actual WiiU interface stuff)

/* Wait until next allegro timer event is fired. */
void wait_for_frame()
{

}

/* Set background color. RGB value of c is defined in fce.h */
pal nextClearColour;
void nes_set_bg_color(int c)
{
	nextClearColour = palette[c];
		fillScreenQuick(nextClearColour.r, nextClearColour.g, nextClearColour.b, 0xFF);
}

/* Flush the pixel buffer/Render it to the screen */
void nes_flush_buf(PixelBuf *buf) {

	int i;
	for (i = 0; i < buf->size; i ++) {
		Pixel *p = &buf->buf[i];
		int x = p->x, y = p->y;
		drawPixelQuick(x, y, palette[p->c].r, palette[p->c].g, palette[p->c].b, 0xFF);
	}
}

void nes_hal_init()
{

}

void nes_flip_display()
{
	flipBuffersQuick();
}

/* Query a button's state.
   Returns 1 if button #b is pressed. 
	0 - Power
	1 - A
	2 - B
	3 - SELECT
	4 - START
	5 - UP
	6 - DOWN
	7 - LEFT
	8 - RIGHT
*/
int nes_key_state(int b)
{
	VPADData vpad_data;
	int error;
	VPADRead(0, &vpad_data, 1, &error);
	if (b == 1) {
		if (vpad_data.btn_hold & BUTTON_A) {
			return 1;
		}
	} else if (b == 2) {
		if (vpad_data.btn_hold & BUTTON_B) {
			return 1;
		}
	} else if (b == 3) {
		if (vpad_data.btn_hold & BUTTON_MINUS) {
			return 1;
		}
	} else if (b == 4) {
		if (vpad_data.btn_hold & BUTTON_PLUS) {
			return 1;
		}
	} else if (b == 5) {
		if (vpad_data.btn_hold & BUTTON_UP) {
			return 1;
		}
	} else if (b == 6) {
		if (vpad_data.btn_hold & BUTTON_DOWN) {
			return 1;
		}
	} else if (b == 7) {
		if (vpad_data.btn_hold & BUTTON_LEFT) {
			return 1;
		}
	} else if (b == 8) {
		if (vpad_data.btn_hold & BUTTON_RIGHT) {
			return 1;
		}
	}
	if (vpad_data.btn_hold & BUTTON_HOME) {
		unsigned int coreinit_handle;
		OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
		void (*_Exit)();
		OSDynLoad_FindExport(coreinit_handle, 0, "_Exit", &_Exit);
		_osscreenexit();
		_Exit();
	}
	return 0;
}

//Faster rendering code, doesn't reload functions each time

void flipBuffersQuick()
{
	//Grab the buffer size for each screen (TV and gamepad)
	int buf0_size = OSScreenGetBufferSizeEx(0);
	int buf1_size = OSScreenGetBufferSizeEx(1);
	//Flush the cache
	DCFlushRange((void *)0xF4000000 + buf0_size, buf1_size);
	DCFlushRange((void *)0xF4000000, buf0_size);
	//Flip the buffer
	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);
}

void fillScreenQuick(char r,char g,char b,char a)
{
	unsigned int num = (r << 24) | (g << 16) | (b << 8) | a;
	OSScreenClearBufferEx(0, num);
	OSScreenClearBufferEx(1, num);
}

void drawPixelQuick(int x, int y, char r, char g, char b, char a)
{
	uint32_t num = (r << 24) | (g << 16) | (b << 8) | a;
	OSScreenPutPixelEx(0,x,y,num);
	OSScreenPutPixelEx(1,x,y,num);
}
 