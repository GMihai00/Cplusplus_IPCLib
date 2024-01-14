# Use linux image
FROM ubuntu:20.04

# Create a directory for your application
WORKDIR /app

# Copy executable
COPY ./build/web_testing_server /app/

# Copy dependencies
COPY ./build/*.a /app/
COPY ./build/*.so /app/
COPY ./build/*.so* /app/
COPY ./build/zlib.pc /app/
COPY ./build/zlib.pc /app/
COPY ./build/zconf.h /app/

# copy local lib

#list all files inside directory
RUN ls

#expose port to be able to connect from outside container
EXPOSE 54321

# Install necessary libraries
RUN apt-get update && \
    apt-get install -y libstdc++6 libgcc1 libssl-dev

RUN chmod +x web_testing_server

# Run the server
CMD ["./web_testing_server", "--ip", "0.0.0.0", "--port", "54321"]