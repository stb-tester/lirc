/******************************************************************
** line_buffer ****************************************************
*******************************************************************
*
* Line buffered input on top of e. g., read(2).
*
* Copyright (c) 2015 Alec Leamas
*
* */

/**
 * @file line_buffer.cpp
 * This file implements the line buffer class.
 */

#ifndef LIB_LINE_BUFFER_H_
#define LIB_LINE_BUFFER_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string>


/** After appending, data can be retrieved as lines. */
class LineBuffer {
	private:
		std::string buff;

	public:
		/** Insert data in buffer. */
		void append(const char* line, size_t size);

		/** Check if get_next_line() returns a non-empty string. */
		bool has_lines();

		/** Peek the complete buffer contents. */
		const char* c_str();

		/** Return and remove first line in buffer, possibly "". */
		std::string get_next_line();

		LineBuffer();
};

#endif  // LIB_LINE_BUFFER_H_
