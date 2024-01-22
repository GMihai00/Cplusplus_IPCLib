# Use linux image
FROM ubuntu:20.04

# Create a directory for your application
WORKDIR /app

# Install necessary libraries
RUN apt-get update && \
    apt-get install -y libstdc++6 libgcc1 libssl-dev g++ cmake

#copy everything from repo I quess
COPY . /app

#list all files inside directory
RUN ls

WORKDIR /app/cross-platform/testing

RUN rm -rf build

RUN mkdir build

WORKDIR /app/cross-platform/testing/build

#generate make files
RUN cmake -S .. -B . -DCMAKE_CXX_FLAGS="-std=c++17" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_GENERATOR="Unix Makefiles"

# #build project
RUN make

#expose port to be able to connect from outside container
EXPOSE 54321

RUN chmod +x web_testing_server

# # # Run the server
CMD ["./web_testing_server", "--ip", "0.0.0.0", "--port", "54321"]