/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SAKILADATABASE_H
#define _SAKILADATABASE_H

#include "MySQLConnection.h"

enum SakilaDatabaseStatements : uint32
{
    /*  Naming standard for defines:
        {DB}_{SEL/INS/UPD/DEL/REP}_{Summary of data changed}
        When updating more than one field, consider looking at the calling function
        name for a suiting suffix.
    */
    //注册相关
    CHECK_REGISTER_INFO_EXIST,
    REGISTER_INTO_USER_INFO,

    //登录相关
    CHECK_LOGIN_PASSWORD,

    // 秒传相关
    CHECK_MD5_FILE_REF_COUNT,        // SELECT count FROM file_info WHERE md5=?
    CHECK_USER_FILE_LIST_EXIST,      // SELECT md5 FROM user_file_list WHERE user=? AND md5=? AND file_name=?
    UPDATE_FILE_INFO_COUNT,          // UPDATE file_info SET count=? WHERE md5=?
    INSERT_USER_FILE_LIST,           // INSERT INTO user_file_list(...)
    SELECT_USER_FILE_COUNT,          // SELECT count FROM user_file_count WHERE user=?
    INSERT_USER_FILE_COUNT,          // INSERT INTO user_file_count(...)
    UPDATE_USER_FILE_COUNT,          // UPDATE user_file_count SET count=? WHERE user=?
    INSERT_FILE_INFO,                // 插入 file_info

    GET_USER_FILE_COUNT,      // SELECT count FROM user_file_count WHERE user = ?
    GET_USER_FILES_LIST_NORMAL,
    GET_USER_FILES_LIST_ASC,      
    GET_USER_FILES_LIST_DESC,
    MAX_SAKILADATABASE_STATEMENTS,
};

class TC_DATABASE_API SakilaDatabaseConnection : public MySQLConnection
{
public:
    typedef SakilaDatabaseStatements Statements;

    //- Constructors for sync and async connections
    SakilaDatabaseConnection(MySQLConnectionInfo &connInfo);
    SakilaDatabaseConnection(ProducerConsumerQueue<SQLOperation *> *q, MySQLConnectionInfo &connInfo);
    ~SakilaDatabaseConnection();

    //- Loads database type specific prepared statements
    void DoPrepareStatements() override;
};

#endif
