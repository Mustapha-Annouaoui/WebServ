// -------------------------------------------------------------------------------
//   Created by Jaouad Chakir (jchakir@student.1337.ma) on 2/21/23 at 1:37 PM.
//   Copyright (c) 2023  1337.ma(@1337FIL) . All rights reserved.
// -------------------------------------------------------------------------------

#include "../../include/webserv/http.hpp"

static int __strcmp_(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2)
	{ ++s1; ++s2; }
	return *s1 - *s2;
}

static char *__buffer_duplicate_(const char *_buffer, size_t len)
{
	char *buffer, *save_buffer;
	
	buffer = new char[len + 1];
	save_buffer = buffer;
	while (*_buffer)
		*buffer++ = *_buffer++;
	*buffer = '\0';
	return save_buffer;
}

static int __parse_content_length_(const char *str)
{
	const char 	*checker = str;
	long int	res = 0;
	
	while (*checker)
	{
		if (*checker < '0' || *checker > '9')
			return -1;
		++checker;
	}
	while (*str && res < INT_MAX)
	{
		res = res * 10 + *str - '0' ;
		++str;
	}
	if (*str == '\0')
		return res;
	return -1;
}

static bool __is_space_(char c)
{
	if (c == ' ' || c == '\t')
		return true;
	return false;
}
static void __skipp_spaces_(char * &buffer)
{
	while (*buffer == ' ' || *buffer == '\t')
		++buffer;
	if (*buffer == '\0')
		throw 400;
}

static char *__get_request_method_(char * &buffer)
{
	char *save_buffer = buffer;
	
	 if (__strcmp_(buffer, "GET") == ' ')
		 buffer += 3;
	 else if (__strcmp_(buffer, "POST") == ' ')
		 buffer += 4;
	 else if (__strcmp_(buffer, "DELETE") == ' ')
		 buffer += 6;
	 else
		 throw 501;
	 
	 *buffer = '\0';
	 ++buffer;
	 while (*buffer == ' ')
		 ++buffer;
	 return save_buffer ;
}

static char *__get_requested_path_(char * &buffer)
{
	char *save_buffer = buffer;
	
	while (*buffer && *buffer != ' ')
		++buffer;
	if (*buffer == '\0')
		throw 400;
	*buffer = '\0';
	++buffer;
	while (*buffer == ' ')
		++buffer;
	return save_buffer;
}

static char *__get_query_from_path_(char *path)
{
	while (*path && *path != '?')
		++path;
	if (*path == '\0')
		return path;
	*path = '\0';
	return path + 1;
}

static const char *__get_extension_from_path_(const char *path)
{
	while (*path)
		++path;
	while (*path != '/' && *path != '.')
		--path;
	if (*path == '.' && *(path-1) != '/')
		return path;
	return "";
}

static void __check_request_protocol_(char * &buffer)
{
	if (__strcmp_(buffer, "HTTP/1.1") == '\r')
		buffer += 9;
	else if (__strcmp_(buffer, "HTTP/1.1") == ' ')
	{
		buffer += 9;
		while (*buffer == ' ')
			++buffer;
		if (*buffer != '\r')
			throw 400;
		++buffer;
	}
	else
		throw 400;
	if (*buffer != '\n')
		throw 400;
	++buffer;
}

static char *__get_header_key_(char * &buffer)
{
	char *save_buffer = buffer;
	
	while (*buffer && *buffer != ':' && !__is_space_(*buffer) && *buffer != '\r')
		++buffer;
	if (*buffer == '\0' || __is_space_(*buffer) || *buffer == '\r')
		throw 400;
	*buffer = '\0';
	++buffer;
	return save_buffer;
}

static char *__get_header_value_(char * &buffer)
{
	char *save_buffer, *end_of_value;

	__skipp_spaces_(buffer);
	save_buffer = buffer;
	end_of_value = buffer;
	while (*buffer && *buffer != '\r')
	{
		end_of_value = buffer;
		if (__is_space_(*buffer))
			__skipp_spaces_(buffer);
		else
			++buffer;
	}
	if (*buffer == '\0' || *(buffer+1) != '\n')
		throw 400;
	if (end_of_value == save_buffer)
		throw 400;
	if (__is_space_(*end_of_value))
		*end_of_value = '\0';
	else
		*(++end_of_value) = '\0';
	buffer += 2;
	return save_buffer;
}

static void __check_basic_bad_request_errors(Request *request)
{
	std::string::size_type pos;
	Headers::iterator	header;
	
	header = request->headers.find("Host");
	if (header == request->headers.end())
		throw 400;
	pos = header->second.rfind(':');
	if (pos != std::string::npos)
		header->second = header->second.substr(0, pos);
	request->is_chunked = false;
	header = request->headers.find("Transfer-Encoding") ;
	if (header != request->headers.end())
	{
		if (header->second != "chunked")
			throw 501;
		request->is_chunked = true;
	}
	header =  request->headers.find("Content-Length");
	if (header == request->headers.end())
		request->content_length = -1;
	else
		request->content_length = __parse_content_length_(header->second.c_str());
}

Request* HTTP::request_parser(const std::string &_buffer)
{
	Request		*request;
	char		*key, *path, *buffer, *save_buffer;
	
	request = new Request;
	buffer = __buffer_duplicate_(_buffer.c_str(), _buffer.length());
	save_buffer = buffer;
	request->method = __get_request_method_(buffer) ;
	path = __get_requested_path_(buffer);
	if (path[0] != '/')
		throw 400;
	request->query = __get_query_from_path_(path);
	request->path = path;
	request->extension = __get_extension_from_path_(path);
	__check_request_protocol_(buffer);
	if (*buffer == '\r')
		throw 400;
	while (*buffer && *buffer != '\r')
	{
		key = buffer;
		if ( ! request->headers[__get_header_key_(buffer)].empty())
		{
			if (__strcmp_(key, "Host") == 0)
				throw 400;
		}
		request->headers[key] = __get_header_value_(buffer);
	}
	if (*buffer == '\0' || *(buffer+1) != '\n')
		throw 400;
	__check_basic_bad_request_errors(request);
	request->body = buffer + 2;
	
	delete save_buffer;

#ifdef DEBUG_JCHAKIR
	std::cout << "-------------------------- Request Data ---------------------\n" ;
	std::cout << "                  =========  Headers   ==========\n" ;
	for (Headers::const_iterator begin = request->headers.begin(), end = request->headers.end(); begin != end; ++begin)
		std::cout << begin->first << ": " << begin->second << '\n';
	std::cout << "                  =========  Path   ==========\n" ;
	std::cout << request->path << '\n';
	std::cout << "                  =========  Extension   ==========\n" ;
	std::cout << request->extension << '\n';
	std::cout << "                  =========  Query   ==========\n" ;
	std::cout << request->query << '\n';
	std::cout << "                  =========  Body   ==========\n" ;
	std::cout << request->body << '\n' ;
	std::cout << "-------------------------- ====== ---------------------\n" ;
#endif
	
	return request;
}
