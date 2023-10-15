#include <iostream>

#include "net/web_client.hpp"
#include "utile/finally.hpp"

int main()
{	
	net::web_client web_client{};

	std::string url = "universities.hipolabs.com";
	std::string method = "/search?country=United+States";

	//std::string url = "api.publicapis.org";
	//std::string method = "/entries";

	if (!web_client.connect(url))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"}
		});

	net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

	try
	{
		auto response = web_client.send(req, 2000);

		if (response == nullptr)
		{
			std::cerr << "Failed to get response";
			return 5;
		}

		std::cout << "Request: " << req.to_string() << std::endl;

		std::cout << "version: " << response->get_version() <<
			" status: " << response->get_status() << 
			" reason: " << response->get_reason() << std::endl;
		std::cout << "Recieved header data: " << response->get_header().dump() << std::endl;
		auto raw_body_data = response->get_body_raw();
		std::cout << "Recieved body data: " << std::string(raw_body_data.begin(), raw_body_data.end()) << std::endl;
		std::cout << "Recieved body data: " << response->get_json_body().dump();
	}
	catch (const std::exception& err)
	{
		std::cerr << "Request failed, err: " << err.what() << " timeout: " << web_client.last_request_timedout();
		return 5;
	}

	// problem in dtor it seems
	return 0;
}