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

#include "osd_canvas.h"
#include "osd_font.h"

static const char* const k_osd_cursor[] =
{
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.....XXXXX",
    "X..X..X",
    "X.X X..X",
    "XX  X..X",
    "X    X..X",
    "     X..X",
    "      XX"
};

static const int k_osd_cursor_rows = (int)(sizeof(k_osd_cursor) / sizeof(k_osd_cursor[0]));

OsdCanvas::OsdCanvas()
{
    InitPointer(m_pixels);
    m_width = 0;
    m_height = 0;
    memset(m_palette, 0, sizeof(m_palette));
}

OsdCanvas::~OsdCanvas()
{
    SafeDeleteArray(m_pixels);
}

void OsdCanvas::Init(int width, int height)
{
    SafeDeleteArray(m_pixels);
    m_width = width;
    m_height = height;
    m_pixels = new u8[width * height];
    memset(m_pixels, 0, (size_t)width * height);
}

int OsdCanvas::GetWidth() const
{
    return m_width;
}

int OsdCanvas::GetHeight() const
{
    return m_height;
}

void OsdCanvas::SetColor(u8 index, u8 red, u8 green, u8 blue)
{
    if (index < OSD_CANVAS_COLORS)
        m_palette[index] = 0xFF000000U | ((u32)red << 16) | ((u32)green << 8) | blue;
}

void OsdCanvas::Clear(u8 color)
{
    memset(m_pixels, color, (size_t)m_width * m_height);
}

void OsdCanvas::FillRect(int x, int y, int width, int height, u8 color)
{
    int x0 = MAX(x, 0);
    int y0 = MAX(y, 0);
    int x1 = MIN(x + width, m_width);
    int y1 = MIN(y + height, m_height);

    for (int row = y0; row < y1; row++)
    {
        if (x1 > x0)
            memset(m_pixels + (row * m_width) + x0, color, x1 - x0);
    }
}

void OsdCanvas::FrameRect(int x, int y, int width, int height, u8 color)
{
    FillRect(x, y, width, 1, color);
    FillRect(x, y + height - 1, width, 1, color);
    FillRect(x, y, 1, height, color);
    FillRect(x + width - 1, y, 1, height, color);
}

void OsdCanvas::DrawText(int x, int y, const char* text, u8 color, bool tall, int advance)
{
    for (int i = 0; IsValidPointer(text) && (text[i] != 0); i++)
    {
        u8 character = (u8)text[i];
        const u8* glyph = tall ? &k_osd_font_16[character * 16] : &k_osd_font_8[character * 8];
        DrawGlyph(x + (i * advance), y, glyph, tall ? 16 : 8, color);
    }
}

void OsdCanvas::DrawSmallText(int x, int y, const char* text, u8 color)
{
    for (int i = 0; IsValidPointer(text) && (text[i] != 0); i++)
    {
        u8 character = (u8)text[i];

        if (character < 128)
            DrawGlyph(x + (i * 5), y, &k_osd_font_small[character * 7], 7, color);
    }
}

void OsdCanvas::DrawCursor(int x, int y, u8 fill, u8 outline)
{
    for (int row = 0; row < k_osd_cursor_rows; row++)
    {
        for (int column = 0; k_osd_cursor[row][column] != 0; column++)
        {
            char c = k_osd_cursor[row][column];

            if (c == 'X')
                Plot(x + column, y + row, outline);
            else if (c == '.')
                Plot(x + column, y + row, fill);
        }
    }
}

void OsdCanvas::Present(u8* frame_buffer) const
{
    u32* destination = (u32*)frame_buffer;
    int count = m_width * m_height;

    for (int i = 0; i < count; i++)
        destination[i] = m_palette[m_pixels[i] & (OSD_CANVAS_COLORS - 1)];
}

int OsdCanvas::GetTextWidth(const char* text, int advance)
{
    return IsValidPointer(text) ? (int)strlen(text) * advance : 0;
}

int OsdCanvas::GetTextHeight(bool tall)
{
    return tall ? 16 : 8;
}

int OsdCanvas::GetSmallTextWidth(const char* text)
{
    int length = IsValidPointer(text) ? (int)strlen(text) : 0;
    return length > 0 ? (length * 5) - 1 : 0;
}

void OsdCanvas::DrawGlyph(int x, int y, const u8* glyph, int rows, u8 color)
{
    for (int row = 0; row < rows; row++)
    {
        u8 bits = glyph[row];

        for (int column = 0; (column < 8) && (bits != 0); column++)
        {
            if ((bits & (0x80 >> column)) != 0)
                Plot(x + column, y + row, color);
        }
    }
}

void OsdCanvas::Plot(int x, int y, u8 color)
{
    if ((x >= 0) && (y >= 0) && (x < m_width) && (y < m_height))
        m_pixels[(y * m_width) + x] = color;
}
