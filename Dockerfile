# Use a pre-built image that contains the i686-elf cross-compiler
FROM lordmilko/i686-elf-tools:latest

# Install NASM and Make (the GCC cross compiler is already in this image)
RUN apt-get update && apt-get install -y nasm make

# Set the working directory inside the container
WORKDIR /os

# When the container runs, automatically execute 'make clean' and 'make'
CMD ["sh", "-c", "make clean && make"]
