// aria2c jsonrpc 客户端
#include <iostream>
#include <fstream>
#include <regex>
#include <codecvt> // codecvt_utf8
#include <locale>  // wstring_convert

#include "lib/HTTPRequest.hpp"
#include "lib/json.hpp"
#include "lib/argagg.hpp"
#include "lib/base64.hpp"
#include "lib/message.hpp"

#include <windows.h>
#include <winuser.h>
#include <shellapi.h>

using json = nlohmann::json;

// 读取文件内容
std::string readFile(std::string filename)
{
    std::ifstream file(filename, std::ios_base::binary | std::ios_base::in);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open " + filename);
    }
    using Iterator = std::istreambuf_iterator<char>;
    std::string content(Iterator{file}, Iterator{});
    if (!file)
    {
        throw std::runtime_error("Failed to read " + filename);
    }
    return content;
}

// 获取json字符串. 无数据时使用默认值
std::string getString(json config, std::string defaultValue)
{
    if (config.is_string())
    {
        return config.get<std::string>();
    }
    else
    {
        return defaultValue;
    }
}

// Convert from string to utf-8 string
// Based on: https://stackoverflow.com/a/4023686 and https://json.nlohmann.me/home/faq/#wide-string-handling
std::string toUtf8String(std::string sourceString)
{
    LPCSTR rawString = sourceString.c_str();
    int retval;
    retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, rawString, -1, NULL, 0);
    if (!SUCCEEDED(retval))
        return NULL;

    LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
    if (lpWideCharStr == NULL)
        return NULL;

    retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, rawString, -1, lpWideCharStr, retval);
    if (!SUCCEEDED(retval))
    {
        free(lpWideCharStr);
        return NULL;
    }

    static std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
    return utf8_conv.to_bytes(lpWideCharStr);
}

// Based on: https://stackoverflow.com/a/4023686 
LPSTR *CommandLineToArgv(LPWSTR lpWideCharStr, INT *pNumArgs)
{
    int retval;

    int numArgs;
    LPWSTR *args;
    args = CommandLineToArgvW(lpWideCharStr, &numArgs);
    free(lpWideCharStr);
    if (args == NULL)
        return NULL;

    int storage = numArgs * sizeof(LPSTR);
    for (int i = 0; i < numArgs; ++i)
    {
        BOOL lpUsedDefaultChar = FALSE;
        retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
        if (!SUCCEEDED(retval))
        {
            LocalFree(args);
            return NULL;
        }

        storage += retval;
    }

    LPSTR *result = (LPSTR *)LocalAlloc(LMEM_FIXED, storage);
    if (result == NULL)
    {
        LocalFree(args);
        return NULL;
    }

    int bufLen = storage - numArgs * sizeof(LPSTR);
    LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
    for (int i = 0; i < numArgs; ++i)
    {
        assert(bufLen > 0);
        BOOL lpUsedDefaultChar = FALSE;
        retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
        if (!SUCCEEDED(retval))
        {
            LocalFree(result);
            LocalFree(args);
            return NULL;
        }

        result[i] = buffer;
        buffer += retval;
        bufLen -= retval;
    }

    LocalFree(args);

    *pNumArgs = numArgs;
    return result;
}

/* @brief
    主函数: 分析目标类型, 通过jsonrpc触发相应的下载任务
*/
int main(int argc, const char **argv)
{
    // 加载命令行参数
    argagg::parser argparser{{
        {"help", {"-h", "--help"}, "shows this help message", 0},
        {"config", {"-c", "--config"}, "path of config file. (Default: config.json)", 1},
    }};

    argagg::parser_results args;
    auto exepath = std::string(argv[0]);
    auto defaultConfigPath = exepath.substr(0, exepath.find_last_of("/\\")) + "/config.json";

    std::cout << "Command: " << exepath << '\n'
              << "Default config: " << defaultConfigPath << '\n';

    try
    {
        args = argparser.parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    // help
    if (args["help"])
    {
        std::cerr << "Usage: aria2win.exe [options] TARGET\n"
                  << argparser << '\n';
        return 0;
    }
    if (args.pos.size() != 1)
    {
        std::cerr << "Need ONE TARGET\n";
        ShowMessage("Need ONE TARGET", "Error");
        return 1;
    }
    auto configPath = args["config"].as<std::string>(defaultConfigPath);
    auto target = args.as<std::string>(0);
    std::cout << "Download: " << target << '\n';

    // 加载配置
    json config;
    try
    {
        config = json::parse(readFile(configPath));
        std::cout << "Config loaded" << '\n';
    }
    catch (const std::exception &e)
    {
        std::cout << "Load config failed, will use default values:\n"
                  << e.what() << '\n';
    }

    std::string url = getString(config["rpc_url"], "http://localhost:6800/jsonrpc");
    std::string rpc_id = getString(config["rpc_id"], "aria2win");
    std::string token = getString(config["rpc_token"], "");
    auto timeout = std::chrono::milliseconds{10000};
    if (config["timeout"].is_number())
    {
        timeout = std::chrono::milliseconds{(long long)(config["timeout"].get<float>() * 1000)};
    }

    std::cout << "url: " << url << '\n';
    std::cout << "rpc_id: " << rpc_id << '\n';
    std::cout << "with token: " << (token != "") << '\n';

    // 构造请求
    http::Request request{url};

    std::string method;
    json params;
    // 若为本地文件
    if (std::regex_search(target, std::regex(R"(^(https?|ftps?|sftp)://|^magnet:)")))
    {
        method = "aria2.addUri";
        params = {{toUtf8String(target)}};
    }
    else
    {
        try
        {
            params = {base64_encode(readFile(target))};
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            ShowMessage(e.what(), "Error");
            return 1;
        }
        if (std::regex_search(target, std::regex(R"(\.torrent$)")))
        {
            method = "aria2.addTorrent";
        }
        else
        {
            method = "aria2.addMetalink";
        }
    }

    if (token != "")
    {
        params.insert(params.begin(), "token:" + token);
    }
    json body = {
        {"jsonrpc", "2.0"},
        {"id", "2.0"},
        {"method", method},
        {"params", params},
    };

    std::string bodyStr = body.dump(-1, ' ', true);
    std::cout << "Request: " << bodyStr.substr(0, 500) << '\n';
    try
    {
        const auto response = request.send("POST", bodyStr, {{"Content-Type", "application/json"}}, timeout);
        std::cout << "Code: " << response.status.code << '\n';
        std::string msg = std::string{response.body.begin(), response.body.end()};
        std::cout << msg << '\n'; // print the result
        ShowMessage(msg.c_str(), "Success");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        ShowMessage(e.what(), "Error");
        return 1;
    }
    return 0;
}

/* @brief
Windows GUI 入口函数

解析参数并调用main函数
*/
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR lpCmdLine, INT nCmdShow)
{
    int argc = 0;
    auto argv = CommandLineToArgv(GetCommandLineW(), &argc);
    return main(argc, (const char **)argv);
}
