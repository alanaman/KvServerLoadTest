# Start from an image that has a C++ compiler and CMake
FROM mcr.microsoft.com/devcontainers/cpp:ubuntu

# Install libpq-dev *inside* the container
RUN sudo apt-get update && \
    sudo apt-get install -y libpq-dev

RUN git config --global --add safe.directory '*'
# Set the working directory to your app
WORKDIR /app