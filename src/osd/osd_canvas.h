/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef OSD_CANVAS_H
#define OSD_CANVAS_H

#include "../common/common.h"

#define OSD_CANVAS_COLORS 16

class OsdCanvas
{
public:
    OsdCanvas();
    ~OsdCanvas();
    void Init(int width, int height);
    int GetWidth() const;
    int GetHeight() const;
    void SetColor(u8 index, u8 red, u8 green, u8 blue);
    void Clear(u8 color);
    void FillRect(int x, int y, int width, int height, u8 color);
    void FrameRect(int x, int y, int width, int height, u8 color);
    void DrawText(int x, int y, const char* text, u8 color, bool tall, int advance = 8);
    void DrawSmallText(int x, int y, const char* text, u8 color);
    void DrawCursor(int x, int y, u8 fill, u8 outline);
    void Present(u8* frame_buffer) const;
    static int GetTextWidth(const char* text, int advance = 8);
    static int GetTextHeight(bool tall);
    static int GetSmallTextWidth(const char* text);

private:
    void DrawGlyph(int x, int y, const u8* glyph, int rows, u8 color);
    void Plot(int x, int y, u8 color);

private:
    u8* m_pixels;
    int m_width;
    int m_height;
    u32 m_palette[OSD_CANVAS_COLORS];
};

#endif /* OSD_CANVAS_H */
