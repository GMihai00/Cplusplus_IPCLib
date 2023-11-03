#include <iostream>

#include "net/web_client.hpp"
#include "utile/finally.hpp"

int main()
{	
	 //boost::asio::io_service io_service;
  //  boost::asio::ip::tcp::socket socket(io_service);

  //   Wrap the socket in a shared_ptr
  //  std::shared_ptr<boost::asio::ip::tcp::socket> socketPtr = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));

	net::web_client web_client{};

	/*std::string url = "universities.hipolabs.com";
	std::string method = "/search?country=United+States";*/

	//std::string url = "api.publicapis.org";
	//std::string method = "/entries";

	std::string url = "127.0.0.1";
	std::string method = "/test";

	if (!web_client.connect(url, 54321))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	//nlohmann::json additional_header_data = nlohmann::json({
	//	{"Accept", "*/*"},
	//	{"Connection", "keep-alive"},
	//	{"Accept-Encoding", "gzip, deflate, br"}
	//	});

	nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"},
		{"Transfer-Encoding", "chunked"}
		});

	std::string body_data = "2\r\nab\r\n0\r\n\r\n";

	//net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

	net::http_request req(net::request_type::GET, 
		method, net::content_type::any, 
		additional_header_data,
		std::vector<uint8_t>(body_data.begin(), body_data.end()));
	// THIS SEEMS TO WORK FINE
	//try
	//{
	// 	std::cout << "Request: " << req.to_string() << std::endl;
	//	auto response = web_client.send(std::move(req) /*, 5000*/);


	//	if (!response.second)
	//	{
	//		std::cerr << "Failed to get response err: " << response.second.message();
	//		return 5;
	//	}

	//	std::cout << "version: " << response.first->get_version() <<
	//		" status: " << response.first->get_status() <<
	//		" reason: " << response.first->get_reason() << std::endl;
	//	std::cout << "Recieved header data: " << response.first->get_header().dump() << std::endl;
	//	std::cout << "Recieved body data: " << response.first->get_json_body().dump();
	//}
	//catch (const std::exception& err)
	//{
	//	std::cerr << "Request failed, err: " << err.what();
	//	return 5;
	//}

	// to send connection close and chuncked message to server
	// connection close not really practicable as request by client
	bool can_stop = false;

	net::async_get_callback req_callback;

	req_callback = [&can_stop, &method, &web_client, &req_callback](std::shared_ptr<net::ihttp_message> response, utile::web_error err_msg) {
		if (!response)
		{
			std::cerr << "Failed: " << err_msg.message();
			exit(5);
		}

		//nlohmann::json additional_header_data = nlohmann::json({
		//{"Accept", "*/*"},
		//{"Connection", "keep-alive"},
		//{"Accept-Encoding", "gzip, deflate, br"}
		//	});

		//std::cout << "Recieved response: " << response->to_string() << "\n";

		//net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);
		//
		//web_client.send_async(std::move(req), req_callback);
		can_stop = true;
	};

	web_client.send_async(std::move(req), req_callback);

	while (!can_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "Waiting for answear\n";
	}

	
	return 0;
}