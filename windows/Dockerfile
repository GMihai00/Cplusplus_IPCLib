# Use windows image
FROM mcr.microsoft.com/windows:ltsc2019 AS runtime

# Create a directory for your application
WORKDIR C:\\app

# # Copy Visual C++ Redistributable packages
# COPY ./vc_redist/* C:\\app/

# Copy executable
COPY ./x64/Release/web_testing_server.exe C:\\app

# Copy dependencies
COPY ./x64/Release/*.lib C:\\app/
COPY ./x64/Release/*.dll C:\\app/

#list all files inside directory
RUN dir

#Alternative way to install vcredist found only
RUN powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://vcredist.com/install.ps1'))"

#expose port to be able to connect from outside container
EXPOSE 54321

# Run the server
CMD ["web_testing_server.exe", "--ip", "0.0.0.0", "--port", "54321"]