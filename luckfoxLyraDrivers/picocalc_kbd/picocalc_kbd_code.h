#ifndef PICOCALC_KBD_CODES_H_
#define PICOCALC_KBD_CODES_H_

/*
The PicoCalc Keyboard firmwarev1.2 Note:
* The keyboard firmware itself handles the logic of the modifier keys and CapsLock;
* Alt is use to activate some of the machine's own functions,such as backlight,keyboard ligtht;
* These functions will stop the scancode ouput except the modiy_key itself;
* Shift + UP/DOWN/Enter trigger PageUp/PageDown/Insert It's Not print/mention on anywhere except the firmware sourcecode.
 */


#define NUM_KEYCODES	256

static unsigned short keycodes[NUM_KEYCODES] = {
	

	[0xA1] = KEY_LEFTALT,
	[0xA2] = KEY_LEFTSHIFT,
	[0xA3] = KEY_RIGHTSHIFT,
	[0xA5] = KEY_LEFTCTRL,

	['a'] = KEY_A,
	['b'] = KEY_B,
	['c'] = KEY_C,
	['d'] = KEY_D,
	['e'] = KEY_E,
	['f'] = KEY_F,
	['g'] = KEY_G,
	['h'] = KEY_H,
	['i'] = KEY_I,
	['j'] = KEY_J,
	['k'] = KEY_K,
	['l'] = KEY_L,
	['m'] = KEY_M,
	['n'] = KEY_N,
	['o'] = KEY_O,
	['p'] = KEY_P,
	['q'] = KEY_Q,
	['r'] = KEY_R,
	['s'] = KEY_S,
	['t'] = KEY_T,
	['u'] = KEY_U,
	['v'] = KEY_V,
	['w'] = KEY_W,
	['x'] = KEY_X,
	['y'] = KEY_Y,
	['z'] = KEY_Z,

	['A'] = KEY_A,
	['B'] = KEY_B,
	['C'] = KEY_C,
	['D'] = KEY_D,
	['E'] = KEY_E,
	['F'] = KEY_F,
	['G'] = KEY_G,
	['H'] = KEY_H,
	['I'] = KEY_I,
	['J'] = KEY_J,
	['K'] = KEY_K,
	['L'] = KEY_L,
	['M'] = KEY_M,
	['N'] = KEY_N,
	['O'] = KEY_O,
	['P'] = KEY_P,
	['Q'] = KEY_Q,
	['R'] = KEY_R,
	['S'] = KEY_S,
	['T'] = KEY_T,
	['U'] = KEY_U,
	['V'] = KEY_V,
	['W'] = KEY_W,
	['X'] = KEY_X,
	['Y'] = KEY_Y,
	['Z'] = KEY_Z,

	[' '] = KEY_SPACE,
	

	['\b'] = KEY_BACKSPACE,
	['\n'] = KEY_ENTER,
	/*
	 * As per the kernel, a keyboard needs to indicate, in advance, which key values it can report.
	 * In order to that, it should have unique scancodes pointing those scancode-keycode pairs.
	 * With the configuration set for now, the keyboard never outputs lower case letters, numbers, and equal to sign.
	 * We can use these as bogus scancodes, and map the keys we want the keyboard to say its pressed when modifier keys are used.
	 * This can change however, in case future software versions of the keyboard micrcontroller itself changes to output other stuff.
	 */
	['0'] = KEY_0,
	['1'] = KEY_1,
	['2'] = KEY_2,
	['3'] = KEY_3,
	['4'] = KEY_4,
	['5'] = KEY_5,
	['6'] = KEY_6,
	['7'] = KEY_7,
	['8'] = KEY_8,
	['9'] = KEY_9,

	[')'] = KEY_0,
	['('] = KEY_9,
	['*'] = KEY_8,
	['&'] = KEY_7,
	['^'] = KEY_6,
	['%'] = KEY_5,
	['$'] = KEY_4,
	['#'] = KEY_3,
	['@'] = KEY_2,
	['!'] = KEY_1,

	[0x81] = KEY_F1,
	[0x82] = KEY_F2,
	[0x83] = KEY_F3,
	[0x84] = KEY_F4,
	[0x85] = KEY_F5,
	[0x86] = KEY_F6,
	[0x87] = KEY_F7,
	[0x88] = KEY_F8,
	[0x89] = KEY_F9,
	[0x90] = KEY_F10,


	[0x08] = KEY_BACKSPACE,
	[0xD4] = KEY_DELETE,
	[0xD5] = KEY_END,
	[0xC1] = KEY_CAPSLOCK,
	[0x09] = KEY_TAB,
	[0xD2] = KEY_HOME,
	[0xB1] = KEY_ESC,
//	[0xd0] = KEY_BREAK,
	[0xd0] = KEY_PAUSE,
	['='] = KEY_EQUAL,
	['+'] = KEY_EQUAL,
	['-'] = KEY_MINUS,
	['_'] = KEY_MINUS,
	['\\'] = KEY_BACKSLASH,
	['|'] = KEY_BACKSLASH,
	
	[0xD1] = KEY_ENTER,//因为 shift + Enter 会报告为 Inser，特殊处理


	['.'] = KEY_DOT,
	['>'] = KEY_DOT,

	[';'] = KEY_SEMICOLON,
	[':'] = KEY_SEMICOLON,

	[','] = KEY_COMMA,
	['<'] = KEY_COMMA,

	['`'] = KEY_GRAVE,
	['~'] = KEY_GRAVE,

	['"'] = KEY_APOSTROPHE,
	['\''] = KEY_APOSTROPHE,

	['?'] = KEY_SLASH,
	['/'] = KEY_SLASH,

	[']'] = KEY_RIGHTBRACE,
	['}'] = KEY_RIGHTBRACE,

	['['] = KEY_LEFTBRACE,
	['{'] = KEY_LEFTBRACE,

	[0xb7] = KEY_RIGHT,
	[0xb5] = KEY_UP,
	[0xb6] = KEY_DOWN,
	[0xb4] = KEY_LEFT,
	[0xd6] = KEY_PAGEUP,
	[0xd7] = KEY_PAGEDOWN,
};


#endif
