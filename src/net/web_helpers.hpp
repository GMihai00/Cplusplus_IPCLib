#pragma once

#include "ihttp_message.hpp"

namespace net
{
	class ihttp_message;
	//https://www.youtube.com/watch?v=pHFWGN-upGM
	//https://faculty.cs.byu.edu/~rodham/cs462/lecture-notes/day-09-web-programming/diagrams-HTTP.pdf

	//Content-Length: just read the data

	//Transfer-Encoding: Chunked 0\r\n\r\n should do the trick here but NEEDS DECODING. basicaly read until \r\n if you are on number size, convert to int 
	// then read until \r\n, see amount of data sent, if not matching with the lenght just halt everything

	//Connection: closed read data until you can't any more on one go, needs to be done sync I think not sure

	// need to follow up redirects... Location: ... in response header
	enum class request_type
	{
		GET,
		POST,
		PUT,
		HEAD,
		PATCH,
		OPTIONS,
		DEL
	};

	enum class content_type
	{
		text_or_html, // Specifies that the content is HTML markup.
		text_or_plain, // Indicates that the content is plain text.
		application_or_json, //Specifies JSON data.
		application_or_xml, //Indicates XML data.
		image_or_jpeg, //Specifies a JPEG image.
		image_or_png, //Indicates a PNG image.
		audio_or_mpeg, //Specifies an MP3 audio file.
		video_or_mp4, //Indicates an MP4 video file.
		any
	};

	request_type string_to_request_type(const std::string& req_type);

	std::string request_type_to_string(const request_type req_type);

	std::string content_type_to_string(const content_type con_type);

	std::optional<std::pair<std::string, std::string>> get_redirect_location(const std::shared_ptr<ihttp_message>& message);

}