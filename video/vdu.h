#ifndef VDU_H
#define VDU_H

#include "agon.h"
#include "cursor.h"
#include "graphics.h"
#include "vdu_audio.h"
#include "vdu_sys.h"

// Handle VDU commands
//
void VDUStreamProcessor::vdu(uint8_t c) {
	switch(c) {
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
		case 0x07:	// Bell
			play_note(0, 100, 750, 125);
			break;
		case 0x08:  // Cursor Left
			cursorLeft();
			break;
		case 0x09:  // Cursor Right
			cursorRight();
			break;
		case 0x0A:  // Cursor Down
			cursorDown();
			break;
		case 0x0B:  // Cursor Up
			cursorUp();
			break;
		case 0x0C:  // CLS
			cls(false);
			break;
		case 0x0D:  // CR
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
		case 0x12:  // GCOL
			vdu_gcol();
			break;
		case 0x13:	// Define Logical Colour
			vdu_palette();
			break;
		case 0x16:  // Mode
			vdu_mode();
			break;
		case 0x17:  // VDU 23
			vdu_sys();
			break;
		case 0x18:	// Define a graphics viewport
			vdu_graphicsViewport();
			break;
		case 0x19:  // PLOT
			vdu_plot();
			break;
		case 0x1A:	// Reset text and graphics viewports
			vdu_resetViewports();
			break;
		case 0x1C:	// Define a text viewport
			vdu_textViewport();
			break;
		case 0x1D:	// VDU_29
			vdu_origin();
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
		case 0x7F:  // Backspace
			plotBackspace();
			break;
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
	auto mode = readByte_t(); if (mode == -1) return;

	auto x = readWord_t(); if (x == -1) return; else x = (short)x;
	auto y = readWord_t(); if (y == -1) return; else y = (short)y;

	pushPoint(x, y);
	setGraphicsOptions();

	debug_log("vdu_plot: mode %d, (%d,%d) -> (%d,%d)\n\r", mode, x, y, p1.X, p1.Y);
  	switch (mode) {
		case 0x04: 			// Move
			moveTo();
			break;
		case 0x05: 			// Line
			plotLine();
			break;
		case 0x40 ... 0x47:	// Point
			plotPoint();
			break;
		case 0x50 ... 0x57: // Triangle
			plotTriangle(mode - 0x50);
			break;
		case 0x90 ... 0x97: // Circle
			plotCircle(mode - 0x90);
			break;
	}
	waitPlotCompletion();
}

// VDU 26 Reset graphics and text viewports
//
void VDUStreamProcessor::vdu_resetViewports() {
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
	auto x1 = readByte_t() * fontW;				// Left
	auto y2 = (readByte_t() + 1) * fontH - 1;	// Bottom
	auto x2 = (readByte_t() + 1) * fontW - 1;	// Right
	auto y1 = readByte_t() * fontH;				// Top

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