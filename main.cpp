// aria2c 客户端库
#include "lib/HTTPRequest.hpp"
#include "lib/json.hpp"
#include "lib/argagg.hpp"
#include "lib/base64.hpp"
#include <iostream>
#include <fstream>
#include <regex>

using json = nlohmann::json;

http::Response MakeRequest()
{
    http::Request request{"http://localhost/test"};

    json body;
    body["test"] = 12;

    std::string strbody = body.dump(-1, ' ', true);

    const auto response = request.send("POST", strbody, {{"Content-Type", "application/json"}});
    return response;
}

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

int main(int argc, const char **argv)
{
    // 加载命令行参数
    argagg::parser argparser{{
        {"help", {"-h", "--help"}, "shows this help message", 0},
        {"config", {"-c", "--config"}, "path of config file. (Default: config.json)", 1},
    }};

    argagg::parser_results args;

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

    auto configPath = args["config"].as<std::string>("config.json");
    if (args.pos.size() != 1)
    {
        std::cerr << "Usage: aria2win.exe [options] TARGET\n"
                  << argparser << '\n';
        return 1;
    }
    auto target = args.as<std::string>(0);

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
    // 若为本地文件
    if (std::regex_search(target, std::regex(R"(^(https?|ftps?|sftp)://|^magnet:)")))
    {
        method = "aria2.addUri";
    }
    else
    {
        try
        {
            target = base64_encode(readFile(target));
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
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

    json params;
    if (token != "")
    {
        params = {"token:" + token, {target}};
    }
    else
    {
        params = {target};
    }
    json body = {
        {"jsonrpc", "2.0"},
        {"id", "2.0"},
        {"method", method},
        {"params", params},
    };

    std::cout << "Request: " << body << '\n';
    try
    {
        const auto response = request.send("POST", body.dump(), {{"Content-Type", "application/json"}}, timeout);
        std::cout << "Code: " << response.status.code << '\n';
        std::cout << std::string{response.body.begin(), response.body.end()} << '\n'; // print the result
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
    }
}