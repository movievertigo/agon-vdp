#ifndef VDU_H
#define VDU_H

#include <HardwareSerial.h>

#include "agon.h"
#include "cursor.h"
#include "graphics.h"
#include "vdu_audio.h"
#include "vdu_sys.h"

extern bool consoleMode;
extern bool printerOn;
extern HardwareSerial DBGSerial;

// Handle VDU commands
//
void VDUStreamProcessor::vdu(uint8_t c) {
	// We want to send raw chars back to the debugger
	// this allows binary (faster) data transfer in ZDI mode
	// to inspect memory and register values
	//
	if (consoleMode) {
		DBGSerial.write(c);
	}

	if (printerOn) {
		switch(c) {
			case 0x03:
				printerOn = false;
				break;
			// case 0x09:	// translate "cursor right" to a space, as terminals treat character 9 as tab
			// 	vdu_print(32);
			// 	break;
			case 8 ... 13:
			case 0x20 ... 0xFF: {
				vdu_print(c);
			}
		}
	}

	if (!commandsEnabled) {
		switch (c) {
			case 0x01: {
				// capture character and send to "printer" if enabled
				auto b = readByte_t();	if (b == -1) return;
				vdu_print(b);
			}	break;
			case 0x06:	// resume VDU command system
				commandsEnabled = true;
				break;
		}
		return;
	}

	switch(c) {
		case 0x01: {
			// capture character and send to "printer" if enabled
			auto b = readByte_t();	if (b == -1) return;
			vdu_print(b);
		}	break;
		case 0x02:
			printerOn = true;
			break;
		case 0x04:
			// enable text cursor
			setCharacterOverwrite(true);
			setActiveCursor(getTextCursor());
			setActiveViewport(VIEWPORT_TEXT);
			break;
		case 0x05:
			// enable graphics cursor
			setCharacterOverwrite(false);
			setActiveCursor(getGraphicsCursor());
			setActiveViewport(VIEWPORT_GRAPHICS);
			break;
		case 0x06:
			// Resume VDU system
			break;
		case 0x07:	// Bell
			playNote(0, 100, 750, 125);
			break;
		case 0x08:	// Cursor Left
			cursorLeft();
			break;
		case 0x09:	// Cursor Right
			cursorRight();
			break;
		case 0x0A:	// Cursor Down
			cursorDown();
			break;
		case 0x0B:	// Cursor Up
			cursorUp();
			break;
		case 0x0C:	// CLS
			cls(false);
			break;
		case 0x0D:	// CR
			cursorCR();
			break;
		case 0x0E:	// Paged mode ON
			setPagedMode(true);
			break;
		case 0x0F:	// Paged mode OFF
			setPagedMode(false);
			break;
		case 0x10:	// CLG
			clg();
			break;
		case 0x11:	// COLOUR
			vdu_colour();
			break;
		case 0x12:	// GCOL
			vdu_gcol();
			break;
		case 0x13:	// Define Logical Colour
			vdu_palette();
			break;
		case 0x14:	// Reset colours
			restorePalette();
			break;
		case 0x15:
			commandsEnabled = false;
			break;
		case 0x16:	// Mode
			vdu_mode();
			break;
		case 0x17:	// VDU 23
			vdu_sys();
			break;
		case 0x18:	// Define a graphics viewport
			vdu_graphicsViewport();
			break;
		case 0x19:	// PLOT
			vdu_plot();
			break;
		case 0x1A:	// Reset text and graphics viewports
			vdu_resetViewports();
			break;
		case 0x1B: { // VDU 27
			auto b = readByte_t();	if (b == -1) return;
			plotCharacter(b);
			vdu_print(c);
		}	break;
		case 0x1C:	// Define a text viewport
			vdu_textViewport();
			break;
		case 0x1D:	// VDU_29
			vdu_origin();
			break;
		case 0x1E:	// Move cursor to top left of the viewport
			cursorHome();
			break;
		case 0x1F:	// TAB(X,Y)
			vdu_cursorTab();
			break;
		case 0x20 ... 0x7E:
		case 0x80 ... 0xFF:
			plotCharacter(c);
			break;
		case 0x7F:	// Backspace
			plotBackspace();
			break;
	}
}

// VDU "print" command
// will output to "printer", if enabled
//
void VDUStreamProcessor::vdu_print(uint8_t c) {
	if (printerOn && !consoleMode) {
		// if consoleMode is enabled we're echoing everything back anyway
		DBGSerial.write(c);
	}
}

// VDU 17 Handle COLOUR
//
void VDUStreamProcessor::vdu_colour() {
	auto colour = readByte_t();

	setTextColour(colour);
}

// VDU 18 Handle GCOL
//
void VDUStreamProcessor::vdu_gcol() {
	auto mode = readByte_t();
	auto colour = readByte_t();

	setGraphicsColour(mode, colour);
}

// VDU 19 Handle palette
//
void VDUStreamProcessor::vdu_palette() {
	auto l = readByte_t(); if (l == -1) return; // Logical colour
	auto p = readByte_t(); if (p == -1) return; // Physical colour
	auto r = readByte_t(); if (r == -1) return; // The red component
	auto g = readByte_t(); if (g == -1) return; // The green component
	auto b = readByte_t(); if (b == -1) return; // The blue component

	setPalette(l, p, r, g, b);
}

// VDU 22 Handle MODE
//
void VDUStreamProcessor::vdu_mode() {
	auto mode = readByte_t();
	debug_log("vdu_mode: %d\n\r", mode);
	if (mode >= 0) {
		set_mode(mode);
		sendModeInformation();
		if (mouseEnabled) {
			sendMouseData();
		}
	}
}

// VDU 24 Graphics viewport
// Example: VDU 24,640;256;1152;896;
//
void VDUStreamProcessor::vdu_graphicsViewport() {
	auto x1 = readWord_t();			// Left
	auto y2 = readWord_t();			// Bottom
	auto x2 = readWord_t();			// Right
	auto y1 = readWord_t();			// Top

	if (setGraphicsViewport(x1, y1, x2, y2)) {
		debug_log("vdu_graphicsViewport: OK %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	} else {
		debug_log("vdu_graphicsViewport: Invalid Viewport %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	}
}

// VDU 25 Handle PLOT
//
void VDUStreamProcessor::vdu_plot() {
	uint8_t buffer[5];
	auto remaining = readIntoBuffer(buffer, 5);
	if (remaining > 0 || ttxtMode) return; // Timeout or teletext mode active

	auto& command = buffer[0];
	auto mode = command & 0x07;
	auto operation = command & 0xF8;

	auto& x = *(short*)(buffer+1);
	auto& y = *(short*)(buffer+3);

	if (mode < 4) {
		pushPointRelative(x, y);
	} else {
		pushPoint(x, y);
	}
	setGraphicsOptions(mode);

	debug_log("vdu_plot: operation: %X, mode %d, (%d,%d) -> (%d,%d)\n\r", operation, mode, x, y, p1.X, p1.Y);

	// if (mode != 0 && mode != 4) {
	if (mode & 0x03) {
		switch (operation) {
			case 0x00:	// line
				plotLine();
				break;
			case 0x08:	// line, omitting last point
				plotLine(false, true);
				break;
			case 0x10:	// dot-dash line
			case 0x18:	// dot-dash line, omitting first point
			case 0x30:	// dot-dash line, omitting first, pattern continued
			case 0x38:	// dot-dash line, omitting both, pattern continued
				debug_log("plot dot-dash line not implemented\n\r");
				break;
			case 0x20:	// solid line, first point omitted
				plotLine(true, false);
				break;
			case 0x28:	// solid line, first and last points omitted
				plotLine(true, true);
				break;
			case 0x40:	// point
				plotPoint();
				break;
			case 0x48:	// line fill left/right to non-bg
				fillHorizontalLine(true, false, gbg);
				break;
			case 0x50:	// triangle fill
				setGraphicsFill(mode);
				plotTriangle();
				break;
			case 0x58:	// line fill right to bg
				fillHorizontalLine(false, true, gbg);
				break;
			case 0x60:	// rectangle fill
				setGraphicsFill(mode);
				plotRectangle();
				break;
			case 0x68:	// line fill left/left to fg
				fillHorizontalLine(true, true, gfg);
				break;
			case 0x70:	// parallelogram fill
				setGraphicsFill(mode);
				plotParallelogram();
				break;
			case 0x78:	// line fill right to non-fg
				fillHorizontalLine(false, false, gfg);
				break;
			case 0x80:	// flood to non-bg
			case 0x88:	// flood to fg
				debug_log("plot flood fill not implemented\n\r");
				break;
			case 0x90:	// circle outline
				plotCircle();
				break;
			case 0x98:	// circle fill
				setGraphicsFill(mode);
				plotCircle(true);
				break;
			case 0xA0:	// circular arc
			case 0xA8:	// circular segment
			case 0xB0:	// circular sector
				// fab-gl has no arc or segment operations, only simple ellipse (squashable circle)
				debug_log("plot circular arc/segment/sector not implemented\n\r");
				break;
			case 0xB8:	// copy/move
				plotCopyMove(mode);
				break;
			case 0xC0:	// ellipse outline
			case 0xC8:	// ellipse fill
				// fab-gl's ellipse isn't compatible with BBC BASIC
				debug_log("plot ellipse not implemented\n\r");
				break;
			case 0xE8:	// Bitmap plot
				plotBitmap(mode);
				break;
		}

	}
	moveTo();
}

// VDU 26 Reset graphics and text viewports
//
void VDUStreamProcessor::vdu_resetViewports() {
	if (ttxtMode) {
		ttxt_instance.set_window(0,24,39,0);
	}
	viewportReset();
	// reset cursors too (according to BBC BASIC manual)
	cursorHome();
	pushPoint(0, 0);
	debug_log("vdu_resetViewport\n\r");
}

// VDU 28: text viewport
// Example: VDU 28,20,23,34,4
//
void VDUStreamProcessor::vdu_textViewport() {
	auto cx1 = readByte_t();
	auto cy2 = readByte_t();
	auto cx2 = readByte_t();
	auto cy1 = readByte_t();
	auto x1 = cx1 * fontW;				// Left
	auto y2 = (cy2 + 1) * fontH - 1;	// Bottom
	auto x2 = (cx2 + 1) * fontW - 1;	// Right
	auto y1 = cy1 * fontH;				// Top

	if (ttxtMode) {
		if (cx2 > 39) cx2 = 39;
		if (cy2 > 24) cy2 = 24;
		if (cx2 >= cx1 && cy2 >= cy1)
		ttxt_instance.set_window(cx1,cy2,cx2,cy1);
	}
	if (setTextViewport(x1, y1, x2, y2)) {
		ensureCursorInViewport(textViewport);
		debug_log("vdu_textViewport: OK %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	} else {
		debug_log("vdu_textViewport: Invalid Viewport %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	}
}

// Handle VDU 29
//
void VDUStreamProcessor::vdu_origin() {
	auto x = readWord_t();
	if (x >= 0) {
		auto y = readWord_t();
		if (y >= 0) {
			setOrigin(x, y);
			debug_log("vdu_origin: %d,%d\n\r", x, y);
		}
	}
}

// VDU 30 TAB(x,y)
//
void VDUStreamProcessor::vdu_cursorTab() {
	auto x = readByte_t();
	if (x >= 0) {
		auto y = readByte_t();
		if (y >= 0) {
			cursorTab(x, y);
		}
	}
}

#endif // VDU_H
