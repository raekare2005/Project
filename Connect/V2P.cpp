#define _CRT_SECURE_NO_WARNINGS
#include "V2P.h"

const string filename = "input.txt";
mutex comMutex;
condition_variable cv;
vector<char> inputBuffer;

void saveToFile(const string& filename, const char* data, size_t length) {
    static int index = 0; // 静态变量记录写入次数（序号）

    // 获取当前时间（北京时间）
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    tm ltm; // 注意：确保系统时区设置为中国时区（东八区）
    localtime_s(&ltm, &now_c);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    // 格式化时间为字符串
    std::ostringstream timeStream;
    timeStream << 1900 + ltm.tm_year << '-'
        << std::setw(2) << std::setfill('0') << 1 + ltm.tm_mon << '-'
        << std::setw(2) << std::setfill('0') << ltm.tm_mday << ' '
        << std::setw(2) << std::setfill('0') << ltm.tm_hour << ':'
        << std::setw(2) << std::setfill('0') << ltm.tm_min << ':'
        << std::setw(2) << std::setfill('0') << ltm.tm_sec
        << std::setw(3) << std::setfill('0') << ms.count();

    // 构建前缀信息 [序号] 时间戳 - 
    std::ostringstream prefixStream;
    prefixStream << "[" << ++index << "] " << timeStream.str() << " - ";
    std::string prefix = prefixStream.str();

    // 转换为十六进制字符串
    std::ostringstream hexStream;
    for (size_t i = 0; i < length; ++i)
    {
        hexStream << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << (static_cast<int>(static_cast<unsigned char>(data[i])) & 0xFF);

        //if ((i + 1) % 4 == 0 && i != length - 1) // 每四个字节后加空格（可选）
           //hexStream << " ";
    }

    // 组合最终内容
    std::string contentToWrite = prefix + hexStream.str() + "\n";

    // 打开文件并追加写入
    std::ofstream outfile(filename, std::ios::app);
    if (!outfile.is_open())
    {
        std::cerr << "无法打开文件：" << filename << std::endl;
        return;
    }

    outfile << contentToWrite;
    outfile.close();
}

uint32_t crc24q_table[256];
void init_crc24q_table() {
    const uint32_t poly = 0x1864CFB;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i << 16;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x800000) ? poly : 0);
        }
        crc24q_table[i] = crc & 0xFFFFFF;
    }
}

uint32_t crc24q(const char* data, size_t length) {
    uint32_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t unsigned_byte = static_cast<uint8_t>(data[i]);
        crc = ((crc << 8) & 0xFFFFFF) ^ crc24q_table[(crc >> 16) ^ unsigned_byte];
    }
    return crc;
}

bool ProcessRTCMFrame(std::vector<char>& inputBuffer, HANDLE hWrite) {
    if (inputBuffer.size() < 7) return false; // 确保至少有7字节以开始解析
    size_t pos = 0;
        // 搜索帧头
    while (pos + 3 <= inputBuffer.size()) {
        if (static_cast<unsigned char>(inputBuffer[pos]) == 0xD3 && (static_cast<unsigned char>(inputBuffer[pos + 1]) & 0xFC) == 0x00) {

            uint16_t lengthField = (((static_cast<unsigned char>(inputBuffer[pos + 1]) & 0x03) << 8) | static_cast<unsigned char>(inputBuffer[pos + 2])) & 0x3FF;
            size_t frameLength = 3 + lengthField + 3; // preamble(1) + length(2) + payload + crc(3)
            if (pos + frameLength > inputBuffer.size())
            {
                // 长度错误导致长度太长，跳过该帧头
                if (pos + frameLength > 3 + 1023 + 3) {
                    pos += 1;
                    continue;
                }
                else break; // 如果当前缓冲区内没有完整的帧，则退出
            }
            // 提取帧并进行CRC校验
            std::vector<char> frame(inputBuffer.begin() + pos, inputBuffer.begin() + pos + frameLength);
            uint32_t expected_crc = (0xFFFFFF & (static_cast<uint32_t>(static_cast<unsigned char>(frame[frameLength - 3])) << 16)) |
                (0xFFFFFF & (static_cast<uint32_t>(static_cast<unsigned char>(frame[frameLength - 2])) << 8)) |
                (0xFFFFFF & static_cast<uint32_t>(static_cast<unsigned char>(frame[frameLength - 1])));
            uint32_t calculated_crc = crc24q(frame.data(), frameLength - 3);

            if (calculated_crc == expected_crc) {
                DWORD bytesWritten;
                WriteFile(hWrite, frame.data(), frameLength, &bytesWritten, NULL);
                //std::cout << "成功发送一帧数据" << std::endl;
                // 删除已处理的数据
                inputBuffer.erase(inputBuffer.begin(), inputBuffer.begin() + pos + frameLength);
            }
            else {
                //std::cerr << "CRC校验失败，丢弃该帧数据" << std::endl;
                // 跳过当前帧头，尝试寻找下一个可能的帧头
                pos += 1;
            }
        }
        else {
            pos += 1;
        }
        
    }
    return false;
}

bool readSrcAndWriteDst(HANDLE hSrc, HANDLE hDst) {
    char buffer[BUFFER_SIZE];
    vector<char> inputBuffer;
    while (true)
    {
        this_thread::sleep_for(chrono::milliseconds(5));
        lock_guard<mutex> lock(comMutex);
        DWORD bytesRead;
        if (ReadFile(hSrc, buffer, BUFFER_SIZE, &bytesRead, NULL)) {
            saveToFile(filename, buffer, bytesRead);
            inputBuffer.insert(inputBuffer.end(), buffer, buffer + bytesRead);
            while (ProcessRTCMFrame(inputBuffer, hDst));
        }
        else {
            cerr << "读取虚拟串口失败！错误代码：" << GetLastError() << endl;
            break;
        }
    }
    return false;
}

bool readDstAndWriteSrc(HANDLE hSrc, HANDLE hDst) {
    char buffer[BUFFER_SIZE];

    while (true)
    {
        this_thread::sleep_for(chrono::seconds(3));
        lock_guard<mutex> lock(comMutex);
        DWORD bytesRead;
        DWORD bytesWritten;
        if (ReadFile(hDst, buffer, BUFFER_SIZE, &bytesRead, NULL)) {
            WriteFile(hSrc, buffer, bytesRead, &bytesWritten, NULL);
        }
    }
    return false;
}

int main()
{
    port phSrc(L"COM18", false), phDst(L"COM3",false);
    phSrc._init();
    phDst._init();
    init_crc24q_table();

    thread srcThread(readSrcAndWriteDst, phSrc.getHandle(), phDst.getHandle());
    thread dstThread(readDstAndWriteSrc, phSrc.getHandle(), phDst.getHandle());

    srcThread.join();
    dstThread.join();

    std::cout << "串口已关闭" << endl;

    system("pause");
    return 0;
}