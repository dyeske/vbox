/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard driver translation tables (PC scan code mappings for known
 * keyboard maps)
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

/* This file contains two tables - one which contains entries identifying
 * the keyboard maps, each of which contains a name along with the mappings
 * of certain fixed keys, and the other of which contains the complete set
 * of mappings for each of the entries in the first table.  These tables will
 * be completed when relevant user feedback is received, based on entries
 * autogenerated by VirtualBox in its release log file. */

#ifndef ___VBox_keyboard_tables_h
# error This file must be included from within keyboard-tables.h
#endif /* ___VBox_keyboard_tables_h */

/**
 * This table contains a set of known keycode mappings for a set of known
 * keyboard types.  The most important type will be the almost ubiquious PC
 * keyboard, but as far as I know some VNC servers and some Sunrays for
 * example use different mappings.  I only used a minimal set of key mappings
 * in this table (perhaps slightly too minimal...) because I wanted to be sure
 * that the keys chosen will be present on all keyboards, even very reduced
 * laptop keyboards. 
 */
static const struct {
    const char *comment;
    unsigned lctrl;
    unsigned lshift;
    unsigned capslock;
    unsigned tab;
    unsigned esc;
    unsigned enter;
    unsigned up;
    unsigned down;
    unsigned left;
    unsigned right;
    unsigned f1;
    unsigned f2;
    unsigned f3;
    unsigned f4;
    unsigned f5;
    unsigned f6;
    unsigned f7;
    unsigned f8;
} main_keyboard_type_list[] = {
    { "XFree86", 0x25, 0x32, 0x42, 0x17, 0x9, 0x24, 0x62, 0x68,
      0x64, 0x66, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a },
    { "evdev", 0x25, 0x32, 0x42, 0x17, 0x9, 0x24, 0x6f, 0x74,
      0x71, 0x72, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a },
    { NULL, 0 } /* Sentinal */
};

unsigned main_keyboard_type_scans[][256] = {
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
      0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
      0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
      0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x145, 0x46, 0x47,
      0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x0, 0x138, 0x56, 0x57,
      0x58, 0x147, 0x148, 0x149, 0x14b, 0x0, 0x14d, 0x14f, 0x150, 0x151, 0x152, 0x153, 0x11c, 0x11d, 0x45, 0x137,
      0x135, 0x138, 0x0, 0x15b, 0x15c, 0x15d, 0x13c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x138, 0x0, 0x0, 0x0,
      0x0, 0x79, 0x0, 0x7b, 0x0, 0x7d, 0x7e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x110, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x119, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x120, 0x0, 0x122, 0x0, 0x124, 0x15f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x12e, 0x0,
      0x130, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x70, 0x0, 0x0, 0x73, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x165, 0x166, 0x167, 0x168, 0x169, 0x16a, 0x16b, 0x16c, 0x16d, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    },
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
      0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
      0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
      0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x145, 0x46, 0x47,
      0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x138, 0x29, 0x56, 0x57,
      0x58, 0x73, 0x0, 0x0, 0x79, 0x70, 0x7b, 0x0, 0x11c, 0x11d, 0x135, 0x137, 0x138, 0x0, 0x147, 0x148,
      0x149, 0x14b, 0x14d, 0x14f, 0x150, 0x151, 0x152, 0x153, 0x0, 0x120, 0x12e, 0x130, 0x15e, 0x0, 0x0, 0x45,
      0x0, 0x53, 0xf2, 0xf1, 0x7d, 0x15b, 0x15c, 0x15d, 0x168, 0x105, 0x106, 0x107, 0x10c, 0x118, 0x65, 0x10a,
      0x110, 0x117, 0x175, 0x0, 0x0, 0x0, 0x15f, 0x163, 0x0, 0x119, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x120, 0x0, 0x122, 0x16c, 0x124, 0x15f, 0x16a, 0x169, 0x0, 0x0, 0x0, 0x119, 0x122, 0x110, 0x12e, 0x0,
      0x130, 0x0, 0x0, 0x0, 0x132, 0x167, 0x140, 0x0, 0x0, 0x10b, 0x18b, 0x0, 0x0, 0x0, 0x105, 0x0, 
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x138, 0x0, 0x0, 0x0, 0x0, 
      0x122, 0x122, 0x0, 0x0, 0x0, 0x0, 0x140, 0x122, 0x169, 0x0, 0x137, 0x0, 0x0, 0x0, 0x0, 0x0, 
      0x0, 0x165, 0x0, 0x0, 0x0, 0x165, 0x166, 0x167, 0x168, 0x169, 0x16a, 0x16b, 0x16c, 0x16d, 0x0, 0x143, 
      0x141, 0x0, 0x157, 0x105, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    },
    { 0 } /* Sentinal */
};
