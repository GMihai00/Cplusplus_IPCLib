#include <iostream>

#include "net/web_client.hpp"
#include "net/secure_web_client.hpp"
#include "utile/finally.hpp"

template <typename T>
int test_web_server_send(T& web_client, const std::string& url, const std::string& method)
{
	nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"},
	{"Accept-Encoding", "gzip, deflate, br"}
	});

	if (!web_client.connect(url))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

	try
	{
	 	std::cout << "Request: " << req.to_string() << std::endl;
		auto response = web_client.send(std::move(req) , 5000);


		if (!response.second)
		{
			std::cerr << "Failed to get response err: " << response.second.message();
			return 5;
		}

		std::cout << "version: " << response.first->get_version() <<
			" status: " << response.first->get_status() <<
			" reason: " << response.first->get_reason() << std::endl;
		std::cout << "Recieved header data: " << response.first->get_header().dump() << std::endl;
		auto body = response.first->get_body_raw();
		std::cout << "Recieved body data: " << std::string(body.begin(), body.end()) << std::endl;
	}
	catch (const std::exception& err)
	{
		std::cerr << "Request failed, err: " << err.what();
		return 5;
	}

	return 0;
}

template <typename T>
int test_web_server_send_async(T& web_client, const std::string& url, const std::string& method)
{

	nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"},
	/*{"Accept-Encoding", "gzip, deflate, br"}*/
	{"Accept-Encoding", "br"}
		});

	if (!web_client.connect(url))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

	net::async_get_callback req_callback;

	bool can_stop = false;

	req_callback = [&can_stop, &method, &web_client, &req_callback](std::shared_ptr<net::ihttp_message> response, utile::web_error err_msg) {
		if (!response)
		{
			std::cerr << "Failed: " << err_msg.message();
			exit(5);
		}

		std::cout << "Recieved response: " << response->to_string() << std::endl;

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

template <typename T>
int test_web_server_send_in_loop(T& web_client)
{
	std::string url = "127.0.0.1";
	std::string method = "/test";

	if (!web_client.connect(url, 54321))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"},
		{"Transfer-Encoding", "chunked"}
		});

	std::string body_data = "2\r\nab\r\n0\r\n\r\n";

	net::http_request req(net::request_type::GET,
		method, net::content_type::any,
		additional_header_data,
		std::vector<uint8_t>(body_data.begin(), body_data.end()));

	bool can_stop = false;

	net::async_get_callback req_callback;

	req_callback = [&can_stop, &method, &web_client, &req_callback](std::shared_ptr<net::ihttp_message> response, utile::web_error err_msg) {
		if (!response)
		{
			std::cerr << "Failed: " << err_msg.message();
			//exit(5);
			can_stop = true;
			return;
		}
		nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"}
			});

		std::cout << "Recieved response: " << response->to_string() << "\n";

		net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);
		
		web_client.send_async(std::move(req), req_callback);
	};

	web_client.send_async(std::move(req), req_callback);

	while (!can_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return 0;
}

int main() try
{	
	bool test_web = true;

	//net::web_client web_client{};
	net::secure_web_client web_client{ { R"(..\..\..\external\boost_asio\example\cpp11\ssl\ca.pem)"} };

	if (test_web)
	{
		if (auto ret = test_web_server_send_async(web_client, "www.dataaccess.com", "/webservicesserver/numberconversion.wso?WSDL"); ret != 0)
		{
			std::cerr << "Send async failed";
			return ret;
		}

		web_client.disconnect();

		return test_web_server_send(web_client, "universities.hipolabs.com", "/search?country=United+States");
	}
	else
	{
		return test_web_server_send_in_loop(web_client);
	}
	
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 5;
}