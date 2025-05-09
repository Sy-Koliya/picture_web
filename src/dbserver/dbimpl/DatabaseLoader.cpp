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

#include "DatabaseLoader.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <mysqld_error.h>

DatabaseLoader::DatabaseLoader()
{
}

template <class T>
DatabaseLoader& DatabaseLoader::AddDatabase(DatabaseWorkerPool<T>& pool, std::string const& dbString,
    const uint8 asyncThreads, const uint8 syncThreads)
{
    // bool const updatesEnabledForThis = DBUpdater<T>::IsEnabled(_updateFlags);

    _open.push([this, dbString, &pool, asyncThreads, syncThreads]() -> bool
    {
        pool.SetConnectionInfo(dbString, asyncThreads, syncThreads);
        if (uint32 error = pool.Open())
        {
            // If the error wasn't handled quit
            if (error)
            {
                TC_LOG_ERROR("sql.driver", "\nDatabasePool %s NOT opened. There were errors opening the MySQL connections. Check your SQLDriverLogFile "
                    "for specific errors. Read wiki at http://www.trinitycore.info/display/tc/TrinityCore+Home", dbString.c_str());

                return false;
            }
        }
        // Add the close operation
        _close.push([&pool]
        {
            pool.Close();
        });
        return true;
    });

    _prepare.push([this, dbString, &pool]() -> bool
    {
        if (!pool.PrepareStatements())
        {
            TC_LOG_ERROR(_logger, "Could not prepare statements of the %s database, see log for details.", dbString.c_str());
            return false;
        }
        return true;
    });

    return *this;
}

bool DatabaseLoader::Load()
{
    if (!OpenDatabases())
        return false;

    if (!PrepareStatements())
        return false;

    return true;
}

bool DatabaseLoader::OpenDatabases()
{
    return Process(_open);
}

bool DatabaseLoader::PrepareStatements()
{
    return Process(_prepare);
}

bool DatabaseLoader::Process(std::queue<Predicate>& queue)
{
    while (!queue.empty())
    {
        if (!queue.front()())
        {
            // Close all open databases which have a registered close operation
            while (!_close.empty())
            {
                _close.top()();
                _close.pop();
            }

            return false;
        }

        queue.pop();
    }
    return true;
}

template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<SakilaDatabaseConnection>(DatabaseWorkerPool<SakilaDatabaseConnection>&, std::string const&,
    const uint8, const uint8);
