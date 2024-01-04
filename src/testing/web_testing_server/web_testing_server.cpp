#include <iostream>

#include "net/web_server.hpp"

// naming not yet enabled... Need to use IP ADRESS INSTEAD

constexpr auto HOST = "127.0.0.1";

int main() try
{
	// boost::asio::ip::udp::socket; for udp sockets
	
	net::web_server server(HOST, 54321);

	// add callbacks	
	net::async_req_handle_callback test_callback = [](std::shared_ptr<net::http_request> req) {

		auto req_data = req->to_string(req->is_body_encoded());

		std::cout << "/////////////////////////////////////////\n";
		std::cout << "Recieved request: " << req->to_string(req->is_body_encoded()) << "\n";
		std::cout << "/////////////////////////////////////////\n";

		try
		{
			auto json_data = req->get_json_body();

			unsigned long long a = json_data["a"].get<unsigned long long>();
			unsigned long long b = json_data["b"].get<unsigned long long>();
			
			auto c = a + b;
			a = b;
			b = c;

			json_data["a"] = a;
			json_data["b"] = b;

			std::string dumped_json = json_data.dump();

			auto body_data = std::vector<uint8_t>(dumped_json.begin(), dumped_json.end());


			return net::http_response(200, "OK", nullptr, body_data);
		}
		catch(const std::exception& err)
		{
			return net::http_response(400, std::string(err.what()));
		}
	};

	net::async_req_regex_handle_callback test_regex_callback = [](std::shared_ptr<net::http_request>, const std::smatch& matches) {

		if (matches.size() < 3)
		{
			return net::http_response(400, "Bad request");
		}

		if (!matches[2].matched)
		{
			return net::http_response(400, "Bad request");
		}

		int id = 0;

		try
		{
			id = std::stoi(matches[2]);
		}
		catch (...)
		{
			return net::http_response(400, "Bad request");
		}

		nlohmann::json smth_to_send = nlohmann::json({ {"id", id}});
		std::string data = smth_to_send.dump();

		auto body_data = std::vector<uint8_t>(data.begin(), data.end());

		return net::http_response(200, "OK", nullptr, body_data);
	};

	std::regex test_pattern(R"(^(/test/id=(\d+))$)");

	server.add_mapping(net::request_type::GET, "/test", test_callback);

	server.add_regex_mapping(net::request_type::GET, test_pattern, test_regex_callback);

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
