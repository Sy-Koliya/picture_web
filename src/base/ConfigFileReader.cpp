
#include "ConfigFileReader.h"
#include "Global.h"
#include <algorithm>

using std::map;
using std::string;
ConfigFileReader::ConfigFileReader(const char *filename)
{
	_LoadFile(filename);
}

ConfigFileReader::~ConfigFileReader()
{
}

char *ConfigFileReader::GetConfigName(const char *name)
{
	if (!m_load_ok)
		return nullptr;

	char *value = nullptr;
	map<string, string>::iterator it = m_config_map.find(name);
	if (it != m_config_map.end())
	{
		value = (char *)it->second.data();
	}

	return value;
}

int ConfigFileReader::SetConfigValue(const char *name, const char *value)
{
	if (!m_load_ok)
		return -1;

	map<string, string>::iterator it = m_config_map.find(name);
	if (it != m_config_map.end())
	{
		it->second = value;
	}
	else
	{
		m_config_map.insert(std::make_pair(name, value));
	}
	return _WriteFIle();
}
void ConfigFileReader::_LoadFile(const char *filename)
{
	m_config_file = filename;

    std::ifstream fin(filename);
    if (!fin)
    {
        std::cerr << "Cannot open " << filename
                  << ", errno=" << errno
                  << " (" << std::strerror(errno) << ")\n";
        return;
    }

    std::string line;
    while (std::getline(fin, line))
    {
        // 如果行尾有 '\r'
        if (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        // 去掉注释：'#' 
        auto pos = line.find('#');
        if (pos != std::string::npos)
            line.erase(pos);

        // 跳过空行
        if (line.empty())
            continue;

        // 解析这一行
        _ParseLine((char *)line.data());
    }

    // 如果流没出错，标记加载成功
    if (fin.good() || fin.eof())
        m_load_ok = true;
    LoadConfigToGlobal();
}

int ConfigFileReader::_WriteFIle(const char *filename)
{

    const std::string path = filename
        ? std::string(filename)
        : m_config_file;

    //打开 ofstream（默认 ios::out|ios::trunc）
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
		std::cerr << "Cannot open " << path
		<< ", errno=" << errno
		<< " (" << std::strerror(errno) << ")\n";
        return -1;
    }

    for (const auto &kv : m_config_map) {
        ofs << kv.first << '=' << kv.second << '\n';
        if (!ofs.good()) {
            // 写入出错
            return -1;
        }
    }

    // ofstream 析构时会自动 flush & close
    return 0;
}

void ConfigFileReader::_ParseLine(const std::string &line)
{
    // 1. 找到等号位置
    auto pos = line.find('=');
    if (pos == std::string::npos) {
        // 没有等号 警告
        return;
    }

    // 2. 分别取 key 和 value 的子串
    std::string rawKey   = line.substr(0, pos);
    std::string rawValue = line.substr(pos + 1);

    // 3. 去掉首尾空白
    std::string key   = _TrimSpace(rawKey);
    std::string value = _TrimSpace(rawValue);

    // 4. 如果 key 非空，就存入 map（允许 value 为空）
    if (!key.empty()) {
        m_config_map.emplace(std::move(key), std::move(value));
    }
}

std::string ConfigFileReader::_TrimSpace(const string& input) {
    // 找到第一个不为空白的位置
    const auto begin = input.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        // 全是空白
        return "";
    }
    // 找到最后一个不为空白的位置
    const auto end = input.find_last_not_of(" \t");
    // substr 的第二个参数是长度，所以要 end - begin + 1
    return input.substr(begin, end - begin + 1);
}



void ConfigFileReader::LoadConfigToGlobal(){
    auto& global = Global::Instance();

    for (const auto& [key, str_val] : m_config_map) {
        try {
            // 根据预定义的 key 做类型转换
            if (key == "WorkPool") {
                global.set(key, static_cast<size_t>(std::stoul(str_val)));
            } 
            else if (key == "SocketPool") {
                global.set(key, static_cast<size_t>(std::stoul(str_val)));
            }
            else if (key == "Content_length_type") {
                global.set(key, str_val);  // 直接存字符串
            }
            else if (key == "Http_ttl_s") {
                global.set(key, std::chrono::seconds(std::stoi(str_val)));
            }
            else if (key == "loop_wait_duration_mil") {
                global.set(key, std::stoi(str_val));
            }
            else if (key == "Debug") {
                global.set(key, std::stoi(str_val));
            }
            else if (key == "Mysql_Rpc_Server") {
                global.set(key, str_val);
            }
            // 其他已知 key 的处理...
            else {
                // 未知 key 的警告处理
            }
        } 
        catch (const std::invalid_argument& e) {
            // 转换失败处理（如 "abc" 转数字）
            throw std::runtime_error("Config parse error for key '" + key + "': " + e.what());
        }
        catch (const std::out_of_range& e) {
            // 数值超出范围处理（如 9999999999999999999）
            throw std::runtime_error("Config value overflow for key '" + key + "'");
        }
    }
}