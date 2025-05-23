/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

 #ifndef HTTPPARSER_REQUEST_H
 #define HTTPPARSER_REQUEST_H
 
 #include <sstream>
 #include <string>
 #include <vector>
 #include <unordered_map>

 namespace httpparser
 {
 
 struct Request {
     Request()
         : versionMajor(0), versionMinor(0), keepAlive(false)
     {}
     
     struct HeaderItem
     {
         std::string name;
         std::string value;
     };
 
     std::string method;
     std::string uri;
     int versionMajor;
     int versionMinor;
     std::vector<HeaderItem> headers;
     std::vector<char> content;
     std::string m_content;
     bool keepAlive;
     std::unordered_map<std::string,std::string>mp;
     int body_length;

     std::string inspect() const
     {
         std::stringstream str;
         str << method << " " << uri << " HTTP/"
                << versionMajor << "." << versionMinor << "\n";
 
         for(std::vector<Request::HeaderItem>::const_iterator it = headers.begin();
             it != headers.end(); ++it)
         {
             str << it->name << ": " << it->value << "\n";
         }
 
         std::string data(content.begin(), content.end());
         str << data << "\n";
         str << "+ keep-alive: " << keepAlive << "\n";;
         return str.str();
     }

     void nv2map(){
        for(auto hi:headers){
            mp.insert(std::make_pair(hi.name,hi.value));
        }
        //headers.clear();
     }
     bool HaveName(std::string name){
        if(mp.find(name)!=mp.end())return true;
        return false;
     }
     std::string GetValue(std::string name){
        if(HaveName(name)){
            return mp[name];
        }
        return "";
     }
     std::string Content2String(){
        m_content.assign(content.begin(),content.end());
        return m_content;
     }

     void clear() {
        method.clear();
        uri.clear();
        versionMajor = 0;
        versionMinor = 0;
        headers.clear();
        content.clear();
        m_content.clear();
        keepAlive = false;
        mp.clear();
        body_length = 0;
    }
 };

 } // namespace httpparser
 
 
 #endif // HTTPPARSER_REQUEST_H