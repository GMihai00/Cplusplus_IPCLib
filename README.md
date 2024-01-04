# Cplusplus_IPCLib

This project aims to provide an easier to use C++ IPC library. It's build to be easy to debug, fast and versitile. The ideeea of creating this project was due to not finding any easy to use librarys for sending messages over the network while writing my licence. It first started as a quick local IPC that can be found in IPC.vcxproj under net/legacy_fast_ipc that still might have few crashes due to concurency issues that had yet to be solved (Note: I'm not planning to invest time into solving them in the near future). If despite this you still want to use it, I will have a section describing it at the end. The bread and butter of this project is an web-based IPC system. During the development of this project I managed to cover almost all HTTP1.0 messages, providing an client for http and https as well as a server using http (Note: Planning to add a https one as well). While developing the library, I was following what Postman does and tried to include all the features that I found there, apart from that, I added "Transfer-Encoding: chuncked" compatibility as I remarked that Postman doesn't support it( While debugging the library with Postman I saw that I get random data inside my body when sending messages with that flag on.) Right now it supports sync and asyncronious sending/handling messages, gzip, formats bodys to json and follows redirects (can't follow from http to https and backwards due to design flow a.t.m.). By using the library you are able to code servers in no time, in few lines of code, almost like using a high level language like python.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
  - [Web-based](#web-based)
    - [Creating a server](#Creating-a-server)
    - [Creating a client](#Creating-a-client)
  - [Legacy](#local)
- [Contributing](#contributing)
- [License](#license)

## Installation

Requirements: MSVC Compiler, Visual Studio is suggested to be used

1. Clone the repository: `git clone [https://github.com/GMihai00/Cplusplus_IPCLib.git](https://github.com/GMihai00/Cplusplus_IPCLib.git)`
2. Navigate to the project directory: `cd yourproject`
3. Open IPC.sln and build project IPC, this will generate a statically library
4. Library to be included why linking as Additional Dependencies: IPC.lib;libz-static.lib;libcrypto.lib;libssl.lib; (Make sure library directory has been added to Library directories)
5. include headers  depending on your needs (web_server.hpp/secure_web_client.hpp/web_client.hpp for web-based newer IPC client.hpp and server.hpp for older local IPC)

## Usage

### Web-based

### Creating a server 

```cpp
#include <iostream>

#include "net/web_server.hpp"

constexpr auto HOST = "127.0.0.1";
constexpr auto PORT = 54321;

int main() try
{
  net::web_server server(HOST, PORT);

  if (auto ret = server.start(); !ret)
  {
  	std::cerr << ret.message();
  	return 1;
  }

  // infinite loop to prevent main thread exit
  while (true)
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
```

**Adding mappings**

**Note**: Mappings can be added even at runtime, there is no need to add them before starting the server, but it is recommended to do so before starting. 

**Fixed address mapping**
```cpp
...

net::async_req_handle_callback test_callback = [](std::shared_ptr<net::http_request> req) {

	bool ok = false;

	req->get_json_body(); // req->get_body_raw() for raw data

	// handle request

	if (ok)
	{
		std::vector<uint8_t> body_data; 
		
		/* add data to body if needed
		   Ex: std::string data = "smth_to_send_back" 
		   body_data = std::vector<uint8_t>(data.begin(), data.end()); */

		return net::http_response(200, "OK", nullptr, body_data);
	}
	else
	{
		return net::http_response(400, "Bad request");
	}
}

server.add_mapping(net::request_type::GET, "/test", test_callback);

...
```

**Volatile address mapping**
```cpp
...

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

server.add_regex_mapping(net::request_type::GET, test_pattern, test_regex_callback);

...
```

**Adding constraints and events on connection**
```cpp
#include "net/web_server.hpp"

class test_web_server : public web_server
{
protected:
  bool can_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    bool can_connect = true;
    
    // check constraints
    
    return can_connect;
  }
  void on_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    // Ex: send event  
  }
  
  void on_client_disconnect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept override
  {
    // Ex: send event  
  }
};

```

**Performance tweaking**

You can set a gap to the number of clients that can connect to the server to prevent overloading. Also, apart from that you can set the number of working threads that handle incoming requests. By default, it's left as 4 as many computers this day have at least 4 cores.

```cpp
#include <iostream>

#include "net/web_server.hpp"

constexpr auto HOST = "127.0.0.1";
constexpr auto PORT = 54321;
constexpr auto MAXIMUM_NR_CONNECTIONS = 100; // default 1000
constexpr auto NR_THREADS = 10; // default 4

int main() try
{
  net::web_server server(HOST, PORT, MAXIMUM_NR_CONNECTIONS, NR_THREADS);
  ...

```
### Creating a client

```cpp

#include <iostream>
#include "net/web_client.hpp"

constexpr auto URL = "127.0.0.1";
constexpr auto PORT = 54321;
  
int main()
{
  net::web_client web_client{};
  
  if (!web_client.connect(URL, PORT))
  {
  	std::cerr << "Failed to connect to server";
  	return;
  }
}
```

**HTTPS client**
```cpp

auto verify_certificate_callback =  [](bool preverified, boost::asio::ssl::verify_context& ctx) -> bool {
        // Your custom verification logic here
        // You can access preverified and verify_context as needed
        // Return true if the certificate is accepted, false otherwise
        return true;  // Example: Always accept the certificate
    }; // by default nullptr

std::vector<std::string> cert_files = { R"(..\..\..\external\boost_asio\example\cpp11\ssl\ca.pem)" };

net::secure_web_client web_client{cert_files, verify_certificate_callback};

if (!web_client.connect(URL))
{
  std::cerr << "Failed to connect to server";
  return;
}
``` 

**Sending basic empty request**

```cpp

...
std::string method = "/test";

net::http_request req(net::request_type::GET,
	method, net::content_type::any);
	
auto response = web_client.send(std::move(req));


if (!response.second)
{
	std::cerr << "Failed to get response err: " << response.second .message();
	return 5;
}
...
```
**Sending additional header flags**
```cpp
...

nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"}
	});

net::http_request req(net::request_type::GET,
	method, net::content_type::any,
	additional_header_data);
	
auto timeout = 5000; // miliseconds

auto response = web_client.send(std::move(req), timeout);

// handle recieved message
response->get_json_body(); // response->get_body_raw() for raw data

... 

```

**Adding a body**
```cpp
...

nlohmann::json additional_header_data = nlohmann::json({
	{"Accept", "*/*"},
	{"Connection", "keep-alive"}
	});

std::string body_data = R"({"a": 1, "b": 1 })";

net::http_request req(net::request_type::GET,
	method, net::content_type::any,
	additional_header_data,
	std::vector<uint8_t>(body_data.begin(), body_data.end()));
	
auto timeout = 5000;

auto response = web_client.send(std::move(req), timeout);
...

```

**Compressing data**
```cpp
...
req.gzip_compress_body();

auto timeout = 5000;
auto follow_redirects = true;

web_client.send(std::move(req), timeout, follow_redirects);
...

```
**Sending messages asynchronously**
```cpp

nlohmann::json additional_header_data = nlohmann::json({
{"Accept", "*/*"},
{"Connection", "keep-alive"},
{"Accept-Encoding", "gzip, deflate, br"}
});

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
  
	auto response_data_decoded = response->to_string(is_encoded);
  ...
  
  // in case raw undecoded data is needed
	if (is_encoded)
	{
		auto response_data_encoded = response->to_string();
		...
	}

	can_stop = true;
};

web_client.send_async(std::move(req), req_callback);

// prevent main loop return
while (!can_stop)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

```

### Legacy

TO DO

## Contributing

1. Fork the repository
2. Create a new branch: `git checkout -b feature-name`
3. Make your changes and commit them: `git commit -m 'Description of changes'`
4. Push to the branch: `git push origin feature-name`
5. Submit a pull request

## License

This project is licensed under the [MIT License](LICENSE.MD).
