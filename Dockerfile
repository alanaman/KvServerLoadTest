# Start from an image that has a C++ compiler and CMake
FROM mcr.microsoft.com/devcontainers/cpp:ubuntu

# Install libpq-dev *inside* the container
RUN sudo apt-get update && \
    sudo apt-get install -y libpq-dev

WORKDIR /app

# Copy the entire project
# This will now ignore the local 'build/' directory
COPY . .

# Create the build directory and run CMake/make
# We specifically build the 'server' target
RUN rm -rf build && mkdir build && cd build && \
    cmake .. && \
    cmake --build .


ENTRYPOINT ["./build/server/my_app"]

# # Set a default command (number of threads)
# # This can be overridden when you run the container
CMD ["4"]