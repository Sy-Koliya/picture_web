/*
 * ConfigFileReader.h
 *
 *  Created on: 2013-7-2
 *      Author: ziteng@mogujie.com
 */

#ifndef CONFIGFILEREADER_H_
#define CONFIGFILEREADER_H_


#include <string>     // std::string
#include <fstream>    // std::ifstream
#include <iostream>   // std::cerr, std::cout
#include <cerrno>     // errno
#include <cstring>    // std::strerror
#include <map>

class ConfigFileReader
{
public:
    ConfigFileReader(const char *filename);
    ~ConfigFileReader();

    char *GetConfigName(const char *name);
    int SetConfigValue(const char *name, const char *value);

private:
    void _LoadFile(const char *filename);
    int _WriteFIle(const char *filename = nullptr);
    void _ParseLine(const std::string& line);
    std::string _TrimSpace(const std::string& name);
    
    void LoadConfigToGlobal();

    bool m_load_ok;
    std::map<std::string, std::string> m_config_map;
    std::string m_config_file;
};

#endif /* CONFIGFILEREADER_H_ */
