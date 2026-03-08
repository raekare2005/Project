#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include <chrono>
#include <fstream>
#include <ctime>
#include <thread>
#include <mutex>
#include "port.h"
using namespace std;

#define BUFFER_SIZE 1050
#define MAX_WRITE_CHUNK_SIZE 512