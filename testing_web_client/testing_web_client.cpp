#include <iostream>

#include "net/web_client.hpp"

int main()
{	
	net::web_client web_client{};


	if (!web_client.connect("api.publicapis.org"))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"}
		});

	net::http_request req(net::request_type::GET, "/entries", net::content_type::any, additional_header_data);

	try
	{
		auto response = web_client.send(req);

		if (response == nullptr)
		{
			std::cerr << "Failed to get response";
			return 5;
		}

		std::cout << "Request: " << req.to_string() << std::endl;

		std::cout << "version: " << response->get_version() <<
			" status: " << response->get_status() << 
			" reason: " << response->get_reason() << std::endl;
		std::cout << "Recieved header data" << response->get_header().dump() << std::endl;
		std::cout << "Recieved body data:" << response->get_json_body().dump();
	}
	catch (const std::exception& err)
	{
		std::cerr << "Request failed, err: " << err.what();
		return 5;
	}

	return 0;
}