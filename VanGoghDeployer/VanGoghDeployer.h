#pragma once

#define SSH_NO_CPP_EXCEPTIONS
#define S_IRWXU 0700 

constexpr int APP_URL = 0;
constexpr int DB_HOST = 1;
constexpr int DB_PORT = 2;
constexpr int DB_DATABASE = 3;
constexpr int DB_USERNAME = 4;
constexpr int DB_PASSWORD = 5;

#include <string>
#include <algorithm>
#include <regex>
#include <cstdio>
#include <experimental/filesystem>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <libssh/sftp.h>
#include <libssh/libssh.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")
