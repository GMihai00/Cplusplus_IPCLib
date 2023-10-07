#pragma once

namespace net
{
	//https://kinsta.com/knowledgebase/what-is-an-http-request/
	//https://www.youtube.com/watch?v=pHFWGN-upGM
	enum class request_type
	{
		GET,
		POST,
		PUT,
		HEAD
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
	};


	inline std::string content_type_to_string(const content_type con_type)
	{
		switch (con_type)
		{
		case content_type::text_or_html:
			return "text/html";
		case content_type::text_or_plain:
			return "text/plain";
		case content_type::application_or_json:
			return "application/json";
		case content_type::application_or_xml:
			return "application/xml";
		case content_type::image_or_jpeg:
			return "image/jpeg";
		case content_type::image_or_png:
			return "image/png";
		case content_type::audio_or_mpeg:
			return "iaudio/mpeg";
		case content_type::video_or_mp4:
			return "video/mp4";
		default:
			break;
		}

		throw std::runtime_error("Failed to find coresponding context type");
	}

	//Content-Length: just read the data

	//Transfer-Encoding: Chunked 0\r\n\r\n should do the trick here but NEEDS DECODING. basicaly read until \r\n if you are on number size, convert to int 
	// then read until \r\n, see amount of data sent, if not matching with the lenght just halt everything

	//Connection: closed read data until you can't any more on one go, needs to be done sync I think not sure
}