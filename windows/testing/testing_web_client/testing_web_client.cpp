#include <iostream>
#include <fstream>
#include <boost/thread/thread.hpp>

#include "net/web_client.hpp"
#include "net/secure_web_client.hpp"
#include "utile/finally.hpp"

void save_to_file(const std::string& data, const std::string& file_name)
{
	std::ofstream file(file_name);

	if (!file.is_open())
	{
		std::cerr << "Failed to save output to file\n";
		return;
	}

	file << data;

	file.close();
}

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
int test_web_server_send_follow_redirects(T& web_client, const std::string& url, const std::string& method)
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
		auto response = web_client.send(std::move(req), 0, true);


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
	{"Accept-Encoding", "gzip, deflate, br"}
	/*{"Accept-Encoding", "br"}*/
		});

	if (!web_client.connect(url))
	{
		std::cerr << "Failed to connect to server";
		return 5;
	}

	net::http_request req(net::request_type::GET, method, net::content_type::any, additional_header_data);

	net::async_get_callback req_callback;

	bool can_stop = false;

	req_callback = [&can_stop, &method, &url, &web_client, &req_callback](std::shared_ptr<net::ihttp_message> response, utile::web_error err_msg) {
		if (!response)
		{
			std::cerr << "Failed: " << err_msg.message();
			exit(5);
		}

		auto is_encoded = response->is_body_encoded();

		auto response_data = response->to_string(is_encoded);
		std::cout << "Recieved response: " << response_data << std::endl;

		save_to_file(response_data, url + "_response.log");

		if (is_encoded)
		{
			auto response_data_encoded = response->to_string(false);

			save_to_file(response_data_encoded, url + "_encoded_response.log");
		}

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
void test_web_server_send_in_loop(T& web_client)
{
	std::string url = "127.0.0.1";
	std::string method = "/test";

	if (!web_client.connect(url, 54321))
	{
		std::cerr << "Failed to connect to server";
		return;
	}

	nlohmann::json additional_header_data = nlohmann::json({
		{"Accept", "*/*"},
		{"Connection", "keep-alive"},
		{"Accept-Encoding", "gzip, deflate, br"}
		});

	std::string body_data = R"({"a": 1, "b": 1 })";

	net::http_request req(net::request_type::GET,
		method, net::content_type::any,
		additional_header_data,
		std::vector<uint8_t>(body_data.begin(), body_data.end()));

	req.gzip_compress_body();

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

		std::cout << "/////////////////////////////////////////\n";
		std::cout << "Recieved response: " << response->to_string() << "\n";
		std::cout << "/////////////////////////////////////////\n";

		try
		{
			auto json_data = response->get_json_body();

			unsigned long long a = json_data["a"].get<unsigned long long>();
			unsigned long long b = json_data["b"].get<unsigned long long>();

			auto c = a + b;
			a = b;
			b = c;

			json_data["a"] = a;
			json_data["b"] = b;

			std::string body_data = json_data.dump();

			net::http_request req(net::request_type::GET,
				method, net::content_type::any,
				additional_header_data,
				std::vector<uint8_t>(body_data.begin(), body_data.end()));

			web_client.send_async(std::move(req), req_callback, 5000);
		}
		catch (const std::exception& err)
		{
			std::cerr << "Invalid json body recieved: " << err.what();
		}
	};

	web_client.send_async(std::move(req), req_callback, 5000);

	while (!can_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

}

int test_http_connection_to_public_web_service()
{
	int ret = 0;

	net::web_client web_client{};

	return test_web_server_send_async(web_client, "universities.hipolabs.com", "/search?country=United+States");
}

int test_https_connection_to_public_web_service()
{
	int ret = 0;

	net::secure_web_client web_client{ { R"(..\..\..\external\boost_asio\example\cpp11\ssl\ca.pem)"} };

	if (ret = test_web_server_send_async(web_client, "www.dataaccess.com", "/webservicesserver/numberconversion.wso?WSDL"); ret != 0)
	{
		std::cerr << "Send async failed\n";
	}

	return ret;
}

void test_local_client_server_send_in_loop()
{
	net::web_client web_client{};

	test_web_server_send_in_loop(web_client);
}


int main() try
{	
	if (auto ret = test_http_connection_to_public_web_service(); ret != 0)
	{
		return ret;
	}

	if (auto ret = test_https_connection_to_public_web_service(); ret != 0)
	{
		return ret;
	}

	constexpr auto NR_CLIENTS = 5;

	boost::thread_group m_worker_threads;

	for (int i = 0; i < NR_CLIENTS; i++)
	{
		m_worker_threads.create_thread(boost::bind(&test_local_client_server_send_in_loop));
	}

	std::this_thread::sleep_for(std::chrono::seconds(10));
	// INFINIT LOOP IF WANTED
	/*bool can_stop = false;
	while (!can_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}*/
	return 0;
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 5;
}