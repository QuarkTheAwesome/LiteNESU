#ifndef LOADER_H
#define LOADER_H

//My prototypes

void _main();
void _osscreeninit();
static int curl_write_data_callback(void *buffer, int size, int nmemb, void *userp);
void _mainThread(int argc, void *argv);
void startDebugger();

void flipBuffersQuick();
void fillScreenQuick(char r,char g,char b,char a);
void drawPixelQuick(int x, int y, char r, char g, char b, char a);

//WiiU prototypes at bottom of file

//libwiiu:types.h

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef uint32_t size_t;

typedef _Bool bool;
#define true 1
#define false 0
#define null 0

#define NULL (void*)0


//From here on out we're copy-pasting liteNES code
//COMMON.H
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

// Binary Operations
bool common_bit_set(long long value, byte position);

// Byte Bit Operations
void common_set_bitb(byte *variable, byte position);
void common_unset_bitb(byte *variable, byte position);
void common_toggle_bitb(byte *variable, byte position);
void common_modify_bitb(byte *variable, byte position, bool set);

// Word Bit Operations
void common_set_bitw(word *variable, byte position);
void common_unset_bitw(word *variable, byte position);
void common_toggle_bitw(word *variable, byte position);
void common_modify_bitw(word *variable, byte position, bool set);

// Double Word Bit Operations
void common_set_bitd(dword *variable, byte position);
void common_unset_bitd(dword *variable, byte position);
void common_toggle_bitd(dword *variable, byte position);
void common_modify_bitd(dword *variable, byte position, bool set);

// Quad Word Bit Operations
void common_set_bitq(qword *variable, byte position);
void common_unset_bitq(qword *variable, byte position);
void common_toggle_bitq(qword *variable, byte position);
void common_modify_bitq(qword *variable, byte position, bool set);
//CPU.H
byte cpu_ram_read(word address);
void cpu_ram_write(word address, byte data);

void cpu_init();
void cpu_reset();
void cpu_interrupt();
void cpu_run(long cycles);

// CPU cycles that passed since power up
unsigned long long cpu_clock();
//CPU-INTERNAL.H
typedef enum {
    carry_flag     = 0x01,
    zero_flag      = 0x02,
    interrupt_flag = 0x04,
    decimal_flag   = 0x08,
    break_flag     = 0x10,
    unused_flag    = 0x20,
    overflow_flag  = 0x40,
    negative_flag  = 0x80
} cpu_p_flag;

typedef enum {
    carry_bp      = 0,
    zero_bp       = 1,
    interrupt_bp  = 2,
    decimal_bp    = 3,
    break_bp      = 4,
    unused_bp     = 5,
    overflow_bp   = 6,
    negative_bp   = 7
} cpu_p_bp;

typedef struct {
    word PC; // Program Counter,
    byte SP; // Stack Pointer,
    byte A, X, Y; // Registers
    byte P; // Flag Register
} CPU_STATE;

CPU_STATE cpu;

byte CPU_RAM[0x8000];

byte op_code;             // Current instruction code
int op_value, op_address; // Arguments for current instruction
int op_cycles;            // Additional instruction cycles used (e.g. when paging occurs)

unsigned long long cpu_cycles;  // Total CPU Cycles Since Power Up (wraps)

void (*cpu_op_address_mode[256])();       // Array of address modes
void (*cpu_op_handler[256])();            // Array of instruction function pointers
bool cpu_op_in_base_instruction_set[256]; // true if instruction is in base 6502 instruction set
char *cpu_op_name[256];                   // Instruction names
int cpu_op_cycles[256];                   // CPU cycles used by instructions

byte cpu_ram_read(word address);
void cpu_ram_write(word address, byte data);

// Interrupt Addresses
word cpu_nmi_interrupt_address();
word cpu_reset_interrupt_address();
word cpu_irq_interrupt_address();

// Updates Zero and Negative flags in P
void cpu_update_zn_flags(byte value);

// If OP_TRACE, print current instruction with all registers into the console
void cpu_trace_instruction();

// CPU Adressing Modes
void cpu_address_implied();
void cpu_address_immediate();
void cpu_address_zero_page();
void cpu_address_zero_page_x();
void cpu_address_zero_page_y();
void cpu_address_absolute();
void cpu_address_absolute_x();
void cpu_address_absolute_y();
void cpu_address_relative();
void cpu_address_indirect();
void cpu_address_indirect_x();
void cpu_address_indirect_y();

static const byte cpu_zn_flag_table[256] =
{
  zero_flag,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
  negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,negative_flag,
};

//FCE.H
int fce_load_rom(char *rom);
void fce_init();
void fce_run();
void fce_update_screen();
void fce_setup_funcs();


// Palette

typedef struct {
	int r;
	int g;
	int b;
} pal;

static const pal palette[64] = {
	{ 0x80, 0x80, 0x80 },
	{ 0x00, 0x00, 0xBB },
	{ 0x37, 0x00, 0xBF },
	{ 0x84, 0x00, 0xA6 },
	{ 0xBB, 0x00, 0x6A },
	{ 0xB7, 0x00, 0x1E },
	{ 0xB3, 0x00, 0x00 },
	{ 0x91, 0x26, 0x00 },
	{ 0x7B, 0x2B, 0x00 },
	{ 0x00, 0x3E, 0x00 },
	{ 0x00, 0x48, 0x0D },
	{ 0x00, 0x3C, 0x22 },
	{ 0x00, 0x2F, 0x66 },
	{ 0x00, 0x00, 0x00 },
	{ 0x05, 0x05, 0x05 },
	{ 0x05, 0x05, 0x05 },
	{ 0xC8, 0xC8, 0xC8 },
	{ 0x00, 0x59, 0xFF },
	{ 0x44, 0x3C, 0xFF },
	{ 0xB7, 0x33, 0xCC },
	{ 0xFF, 0x33, 0xAA },
	{ 0xFF, 0x37, 0x5E },
	{ 0xFF, 0x37, 0x1A },
	{ 0xD5, 0x4B, 0x00 },
	{ 0xC4, 0x62, 0x00 },
	{ 0x3C, 0x7B, 0x00 },
	{ 0x1E, 0x84, 0x15 },
	{ 0x00, 0x95, 0x66 },
	{ 0x00, 0x84, 0xC4 },
	{ 0x11, 0x11, 0x11 },
	{ 0x09, 0x09, 0x09 },
	{ 0x09, 0x09, 0x09 },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x00, 0x95, 0xFF },
	{ 0x6F, 0x84, 0xFF },
	{ 0xD5, 0x6F, 0xFF },
	{ 0xFF, 0x77, 0xCC },
	{ 0xFF, 0x6F, 0x99 },
	{ 0xFF, 0x7B, 0x59 },
	{ 0xFF, 0x91, 0x5F },
	{ 0xFF, 0xA2, 0x33 },
	{ 0xA6, 0xBF, 0x00 },
	{ 0x51, 0xD9, 0x6A },
	{ 0x4D, 0xD5, 0xAE },
	{ 0x00, 0xD9, 0xFF },
	{ 0x66, 0x66, 0x66 },
	{ 0x0D, 0x0D, 0x0D },
	{ 0x0D, 0x0D, 0x0D },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x84, 0xBF, 0xFF },
	{ 0xBB, 0xBB, 0xFF },
	{ 0xD0, 0xBB, 0xFF },
	{ 0xFF, 0xBF, 0xEA },
	{ 0xFF, 0xBF, 0xCC },
	{ 0xFF, 0xC4, 0xB7 },
	{ 0xFF, 0xCC, 0xAE },
	{ 0xFF, 0xD9, 0xA2 },
	{ 0xCC, 0xE1, 0x99 },
	{ 0xAE, 0xEE, 0xB7 },
	{ 0xAA, 0xF7, 0xEE },
	{ 0xB3, 0xEE, 0xFF },
	{ 0xDD, 0xDD, 0xDD },
	{ 0x11, 0x11, 0x11 },
	{ 0x11, 0x11, 0x11 }
};

//HAL.H

struct Pixel {
    int x, y; // (x, y) coordinate
    int c; // RGB value of colors can be found in fce.h
};
typedef struct Pixel Pixel;

/* A buffer of pixels */
struct PixelBuf {
	Pixel buf[264 * 264];
	int size;
};
typedef struct PixelBuf PixelBuf;

extern PixelBuf bg, bbg, fg;

// clear a pixel buffer
#define pixbuf_clean(bf) \
	do { \
		(bf).size = 0; \
	} while (0)

// add a pending pixel into a buffer
#define pixbuf_add(bf, xa, ya, ca) \
	do { \
		if ((xa) < SCREEN_WIDTH && (ya) < SCREEN_HEIGHT) { \
			(bf).buf[(bf).size].x = (xa); \
			(bf).buf[(bf).size].y = (ya); \
			(bf).buf[(bf).size].c = (ca); \
			(bf).size++; \
		} \
	} while (0)

// fill the screen with background color
void nes_set_bg_color(int c);

// flush pixel buffer to frame buffer
void nes_flush_buf(PixelBuf *buf);

// display and empty the current frame buffer
void nes_flip_display();

// initialization
void nes_hal_init();

// query key-press status
int nes_key_state(int b);

//TODO maybe include loader.h???

//MEMORY.H

// Single byte
byte memory_readb(word address);
void memory_writeb(word address, byte data);

// Two bytes (word), LSB first
word memory_readw(word address);
void memory_writew(word address, word data);

//MMC.H

byte mmc_id;

byte mmc_read(word address);
void mmc_write(word address, byte data);
void mmc_copy(word address, byte *source, int length);
void mmc_append_chr_rom_page(byte *source);

//NES.H

#define FPS 60
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

void play();

//PPU.H

byte PPU_SPRRAM[0x100];
byte PPU_RAM[0x4000];

void* memcpy(void* dst, const void* src, uint32_t size);

void ppu_init();
void ppu_finish();

byte ppu_ram_read(word address);
void ppu_ram_write(word address, byte data);
byte ppu_io_read(word address);
void ppu_io_write(word address, byte data);

bool ppu_generates_nmi();
void ppu_set_generates_nmi(bool yesno);

void ppu_set_mirroring(byte mirroring);

void ppu_run(int cycles);
void ppu_cycle();
int ppu_scanline();
void ppu_set_scanline(int s);
void ppu_copy(word address, byte *source, int length);
void ppu_sprram_write(byte data);

// PPUCTRL
bool ppu_shows_background();
bool ppu_shows_sprites();
bool ppu_in_vblank();
void ppu_set_in_vblank(bool yesno);

//PPU-INTERNAL.H




// PPU Memory and State


typedef struct {
    byte PPUCTRL;   // $2000 write only
    byte PPUMASK;   // $2001 write only
    byte PPUSTATUS; // $2002 read only
    byte OAMADDR;   // $2003 write only
    byte OAMDATA;   // $2004
	word PPUSCROLL;
    byte PPUSCROLL_X, PPUSCROLL_Y; // $2005 write only x2
    word PPUADDR;   // $2006 write only x2
    word PPUDATA;   // $2007

    bool scroll_received_x;
    bool addr_received_high_byte;
    bool ready;

    int mirroring, mirroring_xor;

    int x, scanline;
} PPU_STATE;

PPU_STATE ppu;

byte ppu_latch;
bool ppu_sprite_hit_occured = false;

word ppu_get_real_ram_address(word address);


// PPU Constants

static const word ppu_base_nametable_addresses[4] = { 0x2000, 0x2400, 0x2800, 0x2C00 };



// Screen State and Rendering

// For sprite-0-hit checks
byte ppu_screen_background[264][248];

// Precalculated tile high and low bytes addition for pattern tables
byte ppu_l_h_addition_table[256][256][8];
byte ppu_l_h_addition_flip_table[256][256][8];

// Draws current screen pixels in ppu_background_pixels & ppu_sprite_pixels and clears them
void ppu_render_screen();
void ppu_set_background_color(byte color);



// PPUCTRL Functions

word ppu_base_nametable_address();
byte ppu_vram_address_increment();
word ppu_sprite_pattern_table_address();
word ppu_background_pattern_table_address();
byte ppu_sprite_width();
byte ppu_sprite_height();
bool ppu_generates_nmi();



// PPUMASK Functions

bool ppu_renders_grayscale();
bool ppu_shows_background_in_leftmost_8px();
bool ppu_shows_sprites_in_leftmost_8px();
bool ppu_intensifies_reds();
bool ppu_intensifies_greens();
bool ppu_intensifies_blues();
void ppu_set_renders_grayscale(bool yesno);
void ppu_set_shows_background_in_leftmost_8px(bool yesno);
void ppu_set_shows_sprites_in_leftmost_8px(bool yesno);
void ppu_set_shows_background(bool yesno);
void ppu_set_shows_sprites(bool yesno);
void ppu_set_intensifies_reds(bool yesno);
void ppu_set_intensifies_greens(bool yesno);
void ppu_set_intensifies_blues(bool yesno);



// PPUSTATUS Functions

bool ppu_sprite_overflow();
bool ppu_sprite_0_hit();
bool ppu_in_vblank();

void ppu_set_sprite_overflow(bool yesno);
void ppu_set_sprite_0_hit(bool yesno);
void ppu_set_in_vblank(bool yesno);


//PSG.H

extern unsigned char psg_joy1[8];

byte psg_io_read(word address);
void psg_io_write(word address, byte data);

//libwiiu code
//COREINIT.H

#define OSDynLoad_Acquire ((void (*)(char* rpl, unsigned int *handle))0x0102A3B4)
#define OSDynLoad_FindExport ((void (*)(unsigned int handle, int isdata, char *symbol, void *address))0x0102B828)
#define OSFatal ((void (*)(char* msg))0x01031618)
#define __os_snprintf ((int(*)(char* s, int n, const char * format, ... ))0x0102F160)

/* ioctlv() I/O vector */
struct iovec
{
	void *buffer;
	int len;
	char unknown8[0xc-0x8];
};

typedef struct OSContext
{
	/* OSContext identifier */
    uint32_t tag1;
    uint32_t tag2;

    /* GPRs */
    uint32_t gpr[32];

	/* Special registers */
    uint32_t cr;
    uint32_t lr;
    uint32_t ctr;
    uint32_t xer;

    /* Initial PC and MSR */
    uint32_t srr0;
    uint32_t srr1;
} OSContext;

//VPAD.H

#define BUTTON_A        0x8000
#define BUTTON_B        0x4000
#define BUTTON_X        0x2000
#define BUTTON_Y        0x1000
#define BUTTON_LEFT     0x0800
#define BUTTON_RIGHT    0x0400
#define BUTTON_UP       0x0200
#define BUTTON_DOWN     0x0100
#define BUTTON_ZL       0x0080
#define BUTTON_ZR       0x0040
#define BUTTON_L        0x0020
#define BUTTON_R        0x0010
#define BUTTON_PLUS     0x0008
#define BUTTON_MINUS    0x0004
#define BUTTON_HOME     0x0002
#define BUTTON_SYNC     0x0001
#define BUTTON_STICK_R  0x00020000
#define BUTTON_STICK_L  0x00040000
#define BUTTON_TV       0x00010000

typedef struct
{
    float x,y;
} Vec2D;

typedef struct
{
    uint16_t x, y;               /* Touch coordinates */
    uint16_t touched;            /* 1 = Touched, 0 = Not touched */
    uint16_t validity;           /* 0 = All valid, 1 = X invalid, 2 = Y invalid, 3 = Both invalid? */
} VPADTPData;
 
typedef struct
{
    uint32_t btn_hold;           /* Held buttons */
    uint32_t btn_trigger;        /* Buttons that are pressed at that instant */
    uint32_t btn_release;        /* Released buttons */
    Vec2D lstick, rstick;        /* Each contains 4-byte X and Y components */
    char unknown1c[0x52 - 0x1c]; /* Contains accelerometer and gyroscope data somewhere */
    VPADTPData tpdata;           /* Normal touchscreen data */
    VPADTPData tpdata1;          /* Modified touchscreen data 1 */
    VPADTPData tpdata2;          /* Modified touchscreen data 2 */
    char unknown6a[0xa0 - 0x6a];
    uint8_t volume;
    uint8_t battery;             /* 0 to 6 */
    uint8_t unk_volume;          /* One less than volume */
    char unknowna4[0xac - 0xa4];
} VPADData;

//WiiU function prototypes

void(*DCFlushRange)(void *buffer, unsigned int length);
unsigned int(*OSScreenFlipBuffersEx)(unsigned int bufferNum);
unsigned int(*OSScreenGetBufferSizeEx)(unsigned int bufferNum);
unsigned int(*OSScreenClearBufferEx)(unsigned int bufferNum, unsigned int temp);
unsigned int (*OSScreenPutPixelEx)(unsigned int bufferNum, unsigned int posX, unsigned int posY, uint32_t color);
int(*VPADRead)(int controller, VPADData *buffer, unsigned int num, int *error);

#endif /* LOADER_H */