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

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <string>

inline void append_path_component(std::string& path, const char* component)
{
    if (path.length() > 0)
    {
        char last = path[path.length() - 1];
        if (last != '/' && last != '\\')
        {
#if defined(_WIN32)
            path += "\\";
#else
            path += "/";
#endif
        }
    }

    path += component;
}

#endif /* PATH_UTILS_H */
