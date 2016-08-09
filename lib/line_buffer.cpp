/**********************************************************************
** line_buffer.cpp ****************************************************
***********************************************************************
*
* line_buffer - line buffer for use with read(2)
*
* Copyright (c) 2015 Alec Leamas
*
* */

/**
 * @file line_buffer.cpp
 * @brief Line buffered input on top of e. g., read(2).
 */

#include <errno.h>
#include <string.h>

#include "line_buffer.h"


bool LineBuffer::has_lines()
{
	return buff.find('\n') != std::string::npos;
}


void LineBuffer::append(const char* input, size_t size)
{
	buff.append(input, size);
}


const char* LineBuffer::c_str()
{
	return buff.c_str();
}


std::string LineBuffer::get_next_line()
{
	size_t nl = buff.find('\n');
	if (nl == std::string::npos)
		return "";
	std::string line(buff.substr(0, nl + 1));
	buff.erase(0, nl + 1);

	/* remove DOS line endings */
	size_t pos = line.rfind("\r");
	if (pos == line.size() - 1)
		line.erase(pos, 1);
	return line;
}


LineBuffer::LineBuffer()
{
	buff = "";
}
