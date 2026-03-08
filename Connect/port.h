#pragma once
#pragma once
#include "V2P.h"
#include "port.h"

class port {
	HANDLE hPort;
public:
	port(const wchar_t* name, bool isAsync);
	~port();
	// 串口初始化配置
	void _init();
	// 获取hPort
	HANDLE getHandle();
};