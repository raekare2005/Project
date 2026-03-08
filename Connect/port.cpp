#include "port.h"

port::port(const wchar_t* name, bool isAsync) {
    std::wstring fullPortName = L"\\\\.\\" + std::wstring(name);
    if (isAsync) {
        hPort = CreateFile(fullPortName.c_str(),  //用Windows API打开设备
            GENERIC_READ | GENERIC_WRITE,  //读写控制，允许对文件进行读写
            0,  //共享模式，不共享
            NULL, //安全属性
            OPEN_EXISTING,  //文件必须已经存在
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,  //文件属性，默认属性
            NULL);
    }
    else {
        hPort = CreateFile(fullPortName.c_str(),  //用Windows API打开设备
            GENERIC_READ | GENERIC_WRITE,  //读写控制，允许对文件进行读写
            0,  //共享模式，不共享
            NULL, //安全属性·
            OPEN_EXISTING,  //文件必须已经存在
            FILE_ATTRIBUTE_NORMAL,  //文件属性，默认属性
            NULL);
    }
    if (hPort == INVALID_HANDLE_VALUE) {
        cerr << "无法打开串口！错误代码：" << GetLastError() << endl;
    }
}

port::~port() {
    if (hPort != INVALID_HANDLE_VALUE) {
        CancelIo(hPort);
        CloseHandle(hPort);
    }
}

void port::_init() {
    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    GetCommState(hPort, &dcb); // 先获取当前配置

    // 修改参数
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fRtsControl = RTS_CONTROL_DISABLE; // 禁用RTS流控制
    dcb.fDtrControl = DTR_CONTROL_DISABLE; // 禁用DTR流控制

    if (!SetCommState(hPort, &dcb)) {
        // 处理错误
        std::cerr << "配置错误";
    }

    // 设置超时参数
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 200;  //读操作时，两个字符间的间隔，如果间隔超过该限制，则立即返回（单位是ms）
    timeouts.ReadTotalTimeoutConstant = 100;  //读操作的固定超时
    timeouts.ReadTotalTimeoutMultiplier = 50; //读取每个字符时的超时
    timeouts.WriteTotalTimeoutConstant = 50;  //写操作的固定超时
    timeouts.WriteTotalTimeoutMultiplier = 10;  //写入每个字符时的超时

    SetCommTimeouts(hPort, &timeouts);
}

HANDLE port::getHandle() {
    return hPort;
}