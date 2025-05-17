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

#include "SakilaDatabase.h"
#include "MySQLConnection.h"
#include "MySQLPreparedStatement.h"

void SakilaDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_SAKILADATABASE_STATEMENTS);

     // — 注册 & 登录 —
    PrepareStatement(CHECK_REGISTER_INFO_EXIST,
        "SELECT id FROM user_info WHERE user_name = ?",
        CONNECTION_ASYNC);
    PrepareStatement(REGISTER_INTO_USER_INFO,
        "INSERT INTO user_info (user_name, nick_name, password, phone, email) "
        "VALUES (?, ?, ?, ?, ?)",
        CONNECTION_ASYNC);
    PrepareStatement(CHECK_LOGIN_PASSWORD,
        "SELECT password FROM user_info WHERE user_name = ?",
        CONNECTION_ASYNC);

    // — file_info 秒传 & 引用计数 —
    PrepareStatement(CHECK_MD5_FILE_REF_COUNT,
        "SELECT count FROM file_info WHERE md5 = ?",
        CONNECTION_ASYNC);
    PrepareStatement(INSERT_FILE_INFO,
        "INSERT INTO file_info (md5, file_id, url, size, type, count) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        CONNECTION_ASYNC);
    PrepareStatement(UPDATE_FILE_INFO_COUNT,
        "UPDATE file_info SET count = ? WHERE md5 = ?",
        CONNECTION_ASYNC);
    PrepareStatement(DELETE_FILE_INFO,
        "DELETE FROM file_info WHERE md5 = ?",
        CONNECTION_ASYNC);
    PrepareStatement(GET_FILE_REF_COUNT,
        "SELECT count FROM file_info WHERE md5 = ?",
        CONNECTION_ASYNC);

    // — user_file_list & user_file_count —
    PrepareStatement(CHECK_USER_FILE_LIST_EXIST,
        "SELECT md5 FROM user_file_list "
        "WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);
    PrepareStatement(INSERT_USER_FILE_LIST,
        "INSERT INTO user_file_list "
        "(user, md5, create_time, file_name, shared_status, pv) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        CONNECTION_ASYNC);
    PrepareStatement(DELETE_USER_FILE_LIST,
        "DELETE FROM user_file_list "
        "WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);

    PrepareStatement(SELECT_USER_FILE_COUNT,
        "SELECT count FROM user_file_count WHERE user = ?",
        CONNECTION_ASYNC);
    PrepareStatement(INSERT_USER_FILE_COUNT,
        "INSERT INTO user_file_count (user, count) VALUES (?, ?)",
        CONNECTION_ASYNC);
    PrepareStatement(UPDATE_USER_FILE_COUNT,
        "UPDATE user_file_count SET count = ? WHERE user = ?",
        CONNECTION_ASYNC);
    PrepareStatement(GET_USER_FILE_COUNT,
        "SELECT count FROM user_file_count WHERE user = ?",
        CONNECTION_ASYNC);

    // — 分页查询 —
    PrepareStatement(GET_USER_FILES_LIST_NORMAL,
        R"SQL(
        SELECT u.`user` AS user_id, u.md5 AS file_md5,
               CAST(u.create_time AS CHAR) AS created_at,
               u.file_name AS filename, u.shared_status AS is_shared,
               u.pv AS view_count,
               f.url AS file_url, f.size AS file_size, f.type AS file_type
        FROM user_file_list u
        JOIN file_info f ON u.md5 = f.md5
        WHERE u.`user` = ?
        LIMIT ?, ?
        )SQL",
        CONNECTION_ASYNC);
    PrepareStatement(GET_USER_FILES_LIST_ASC,
        R"SQL(
        SELECT u.`user` AS user_id, u.md5 AS file_md5,
               CAST(u.create_time AS CHAR) AS created_at,
               u.file_name AS filename, u.shared_status AS is_shared,
               u.pv AS view_count,
               f.url AS file_url, f.size AS file_size, f.type AS file_type
        FROM user_file_list u
        JOIN file_info f ON u.md5 = f.md5
        WHERE u.`user` = ?
        ORDER BY u.pv ASC
        LIMIT ?, ?
        )SQL",
        CONNECTION_ASYNC);
    PrepareStatement(GET_USER_FILES_LIST_DESC,
        R"SQL(
        SELECT u.`user` AS user_id, u.md5 AS file_md5,
               CAST(u.create_time AS CHAR) AS created_at,
               u.file_name AS filename, u.shared_status AS is_shared,
               u.pv AS view_count,
               f.url AS file_url, f.size AS file_size, f.type AS file_type
        FROM user_file_list u
        JOIN file_info f ON u.md5 = f.md5
        WHERE u.`user` = ?
        ORDER BY u.pv DESC
        LIMIT ?, ?
        )SQL",
        CONNECTION_ASYNC);

    // — ShareFile 相关 —
    PrepareStatement(CHECK_SHARE_FILE_EXIST,
        "SELECT 1 FROM share_file_list WHERE md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);
    PrepareStatement(INSERT_SHARE_FILE,
        "INSERT INTO share_file_list (user, md5, create_time, file_name, pv) "
        "VALUES (?, ?, ?, ?, 0)",
        CONNECTION_ASYNC);
    PrepareStatement(UPDATE_USER_FILE_LIST_SHARE_STATUS,
        "UPDATE user_file_list SET shared_status = 1 "
        "WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);

    PrepareStatement(GET_SHARE_FILE_COUNT,
        "SELECT count FROM user_file_count WHERE user = ?",
        CONNECTION_ASYNC);
    PrepareStatement(INSERT_SHARE_FILE_COUNT,
        "INSERT INTO user_file_count (user, count) VALUES (?, ?)",
        CONNECTION_ASYNC);
    PrepareStatement(UPDATE_SHARE_FILE_COUNT,
        "UPDATE user_file_count SET count = ? WHERE user = ?",
        CONNECTION_ASYNC);          // DELETE FROM user_file_list WHERE user = ? AND md5 = ? AND file_name = ?

    // — DeleteFile 相关 —
    PrepareStatement(DELETE_SHARE_FILE,
        "DELETE FROM share_file_list "
        "WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);
    PrepareStatement(DELETE_USER_FILE_LIST,
        "DELETE FROM user_file_list "
        "WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);
    PrepareStatement(GET_USER_FILE_ID,
        "select file_id from file_info"
        " where md5 = ?",
        CONNECTION_ASYNC);
}


SakilaDatabaseConnection::SakilaDatabaseConnection(MySQLConnectionInfo &connInfo) : MySQLConnection(connInfo)
{
}

SakilaDatabaseConnection::SakilaDatabaseConnection(ProducerConsumerQueue<SQLOperation *> *q, MySQLConnectionInfo &connInfo) : MySQLConnection(q, connInfo)
{
}

SakilaDatabaseConnection::~SakilaDatabaseConnection()
{
}
