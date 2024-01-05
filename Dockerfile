# Use windows image
FROM mcr.microsoft.com/windows:ltsc2019 AS runtime

# Create a directory for your application
WORKDIR C:\app

# Copy executable
COPY ./bin/x64/Debug/web_testing_server.exe C:\app

# Copy dependencies
COPY ./bin/x64/Debug/*.lib C:\app
COPY ./bin/x64/Debug/*.dll C:\app

COPY ./src/x64/Debug/*.lib C:\app
COPY ./src/x64/Debug/*.dll C:\app

# Run the server
CMD ["web_testing_server.exe", "--ip", "127.0.0.1", "--port", "54321"]