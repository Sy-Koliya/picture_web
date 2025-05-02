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
    // 注册
    PrepareStatement(
        CHECK_REGISTER_INFO_EXIST,
        "select id from user_info "
        "where user_name = ?",
        CONNECTION_ASYNC);

    PrepareStatement(
        REGISTER_INTO_USER_INFO,
        "INSERT INTO user_info "
        "(user_name, nick_name, password, phone, email) "
        "VALUES (?, ?, ?, ?, ?)",
        CONNECTION_ASYNC);

    // 登录
    PrepareStatement(
        CHECK_LOGIN_PASSWORD,
        "SELECT password FROM user_info WHERE user_name = ?",
        CONNECTION_ASYNC);


    //秒传
    PrepareStatement(
        CHECK_FILE_REF_COUNT,
        "SELECT count FROM file_info WHERE md5 = ?",
        CONNECTION_ASYNC);

    PrepareStatement(
        CHECK_USER_FILE,
        "SELECT 1 FROM user_file_list WHERE user = ? AND md5 = ? AND file_name = ?",
        CONNECTION_ASYNC);

    PrepareStatement(
        UPDATE_FILE_INFO_COUNT,
        "UPDATE file_info SET count = ? WHERE md5 = ?",
        CONNECTION_ASYNC);

    PrepareStatement(
        INSERT_USER_FILE,
        "INSERT INTO user_file_list(user, md5, create_time, file_name, shared_status, pv) VALUES (?, ?, ?, ?, ?, ?)",
        CONNECTION_ASYNC);

    PrepareStatement(
        GET_USER_FILE_COUNT,
        "SELECT count FROM user_file_count WHERE user = ?",
        CONNECTION_ASYNC);

    PrepareStatement(
        INSERT_USER_FILE_COUNT,
        "INSERT INTO user_file_count(user, count) VALUES (?, ?)",
        CONNECTION_ASYNC);

    PrepareStatement(
        UPDATE_USER_FILE_COUNT,
        "UPDATE user_file_count SET count = ? WHERE user = ?",
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
