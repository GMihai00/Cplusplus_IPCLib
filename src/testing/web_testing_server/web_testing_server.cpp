#include <iostream>

#include "net/web_server.hpp"

// naming not yet enabled... Need to use IP ADRESS INSTEAD

constexpr auto HOST = "127.0.0.1";

int main() try
{
	net::web_server server(HOST, 54321);

	// add callbacks	
	net::async_req_handle_callback test_callback = [](std::shared_ptr<net::http_request>) {
		return net::http_response(200, "OK");
	};

	server.add_mapping("/test", test_callback);

	if (auto ret = server.start(); !ret)
	{
		std::cerr << ret.message();
		return 1;
	}

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		/*std::cout << "Running\n";*/
	}

	return 0;
}
catch (const std::exception& err)
{
	std::cerr << err.what();
	return 1;
}