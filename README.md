# Cplusplus_IPCLib

This project aims to provide an easier to use C++ IPC library. It's build to be easy to debug, fast and versitile. The ideeea of creating this project was due to not finding any easy to use librarys for sending messages over the network while writing my licence. It first started as a quick local IPC that can be found in IPC.vcxproj under net/legacy_fast_ipc that still might have few crashes due to concurency issues that had yet to be solved (Note: I'm not planning to invest time into solving them in the near future). If despite this you still want to use it, I will have a section describing it at the end. The bread and butter of this project is an web-based IPC system. During the development of this project I managed to cover almost all HTTP1.0 messages, providing an client for http and https as well as a server using http (Note: Planning to add a https one as well). While developing the library, I was following what Postman does and tried to include all the features that I found there, apart from that, I added "Transfer-Encoding: chuncked" compatibility as I remarked that Postman doesn't support it( While debugging the library with Postman I saw that I get random data inside my body when sending messages with that flag on.) Right now it supports sync and asyncronious sending/handling messages, gzip, formats bodys to json and follows redirects (can't follow from http to https and backwards due to design flow a.t.m.). By using the library you are able to code servers in no time, in few lines of code, almost like python.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
  -[Web-based](#web_based)
  -[Legacy](#local)
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
TO DO

### Legacy

## Contributing

1. Fork the repository
2. Create a new branch: `git checkout -b feature-name`
3. Make your changes and commit them: `git commit -m 'Description of changes'`
4. Push to the branch: `git push origin feature-name`
5. Submit a pull request

## License

This project is licensed under the [MIT License](LICENSE.MD).
