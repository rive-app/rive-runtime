FROM dart:stable

RUN apt update && apt-get -y install unzip zip clang cmake ninja-build pkg-config libgtk-3-dev xvfb cargo wget g++

WORKDIR /
RUN wget -q https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz
RUN tar -xf premake-5.0.0-beta2-linux.tar.gz
RUN mv premake5 /usr/bin/

ENV LDFLAGS="-pthreads"
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# ADD skia/dependencies/make_dependencies.sh /app/skia/dependencies/make_dependencies.sh
ADD skia/dependencies/make_skia.sh /app/skia/dependencies/make_skia.sh
ADD skia/dependencies/make_glfw.sh /app/skia/dependencies/make_glfw.sh
WORKDIR /app/skia/dependencies
# RUN /app/skia/dependencies/make_dependencies.sh
RUN /app/skia/dependencies/make_skia.sh
RUN /app/skia/dependencies/make_glfw.sh

WORKDIR /app/packages/peon_process
ADD rive /app/rive
ADD skia /app/skia
WORKDIR /app/skia/thumbnail_generator

RUN /app/skia/thumbnail_generator/build.sh clean
RUN /app/skia/thumbnail_generator/build.sh
