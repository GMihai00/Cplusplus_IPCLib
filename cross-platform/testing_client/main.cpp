#include <iostream>
#include <boost/thread/thread.hpp>
#include "net/web_client.hpp"

namespace consts
{
	const std::string URL = "127.0.0.1";
	const std::string PORT = "54321";
}

template <typename T>
void test_web_server_send_in_loop(T& web_client)
{
	std::string url = consts::URL;
	std::string method = "/test";

	if (!web_client.connect(url, consts::PORT))
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

void test_local_client_server_send_in_loop()
{
	net::web_client web_client{};

	test_web_server_send_in_loop(web_client);
}

int main(int argc, char* argv[]) try
{	
	constexpr auto NR_CLIENTS = 5;
	
	boost::thread_group m_worker_threads;

	for (int i = 0; i < NR_CLIENTS; i++)
	{
		m_worker_threads.create_thread(boost::bind(&test_local_client_server_send_in_loop));
	}

	bool can_stop = false;
	while (!can_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return 0;
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 1;
}
