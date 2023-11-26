#pragma once

#include "web_helpers.hpp"
#include <iostream>

namespace net
{

	nlohmann::json web_location::to_json()
	{
		return nlohmann::json({
			{"method", m_method},
			{"port", m_port},
			{"host", m_host}
			});
	}
	
	std::optional<web_location> from_json(const nlohmann::json& data) try
	{
		web_location rez;

		rez.m_method = data.at("method").get<std::string>();
		rez.m_port = data.at("port").get<std::string>();
		rez.m_host = data.at("host").get<std::string>();

		return rez;
	}
	catch (const std::exception& e) {
		//debug only
		std::cerr << "Error parsing JSON: " << e.what() << std::endl;
		return std::nullopt;
	}

	request_type string_to_request_type(const std::string& req_type)
	{
		if (req_type == "GET") {
			return request_type::GET;
		}
		else if (req_type == "POST") {
			return request_type::POST;
		}
		else if (req_type == "PUT") {
			return request_type::PUT;
		}
		else if (req_type == "HEAD") {
			return request_type::HEAD;
		}
		else if (req_type == "PATCH") {
			return request_type::PATCH;
		}
		else if (req_type == "OPTIONS") {
			return request_type::OPTIONS;
		}
		else if (req_type == "DELETE") {
			return request_type::DEL;
		}

		throw std::runtime_error("Failed to find coresponding request type");
	}

    std::string request_type_to_string(const request_type req_type)
	{
		switch (req_type)
		{
		case request_type::GET:
			return "GET";
		case request_type::POST:
			return "POST";
		case request_type::PUT:
			return "PUT";
		case request_type::HEAD:
			return "HEAD";
		case request_type::PATCH:
			return "PATCH";
		case request_type::OPTIONS:
			return "OPTIONS";
		case request_type::DEL:
			return "DELETE";
		default:
			break;
		}

		throw std::runtime_error("Failed to find coresponding request type");
	}

	std::string content_type_to_string(const content_type con_type)
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
		case content_type::any:
			return "*/*";
		default:
			break;
		}

		throw std::runtime_error("Failed to find coresponding context type");
	}

	std::optional<web_location> get_redirect_location(const std::shared_ptr<ihttp_message>& message)
	{
		auto header_data = message->get_header();

		if (auto it = header_data.find("Location"); it != header_data.end() && it->is_string())
		{
			auto redirect_location = it->get<std::string>();

			web_location rez;

			auto poz = redirect_location.find("://");

			if (poz != std::string::npos)
			{
				rez.m_port = redirect_location.substr(0, poz);

				poz += 3;
			}
			else
			{
				poz = 0;
			}

			auto old_poz = poz;
			poz = redirect_location.find("/", poz);

			if (poz != std::string::npos)
			{
				rez.m_host = redirect_location.substr(old_poz, poz - old_poz);

			}
			else
			{
				poz = 0;
			}

			rez.m_method = redirect_location.substr(poz, redirect_location.size() - poz);

			return rez;

		}

		return std::nullopt;
	}
}