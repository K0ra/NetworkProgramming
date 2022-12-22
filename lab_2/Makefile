# DO NOT TOUCH IT FILE !!!

SHELL := /bin/bash

all:
	$(CXX) server.cpp -std=c++17 -g -O3 -Werror -Wall -Wextra -pthread -pedantic -o server
	$(CXX) client.cpp -std=c++17 -g -O3 -Werror -Wall -Wextra -pthread -pedantic -o client

clang_format:
	diff -u <(cat *.cpp ) <(clang-format *.cpp)

clang_format_fix:
	clang-format -i *.cpp

clang_tidy:
	clang-tidy *.cpp -checks=-*,clang-analyzer-*,-clang-analyzer-cplusplus* -- -I . -std=c++17 -g -O3 -Werror -Wall -Wextra -pthread -pedantic

clean:
	rm -f server
	rm -f client
