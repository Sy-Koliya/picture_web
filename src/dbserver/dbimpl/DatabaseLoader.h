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

#ifndef DatabaseLoader_h__
#define DatabaseLoader_h__

#include "Define.h"

#include <functional>
#include <queue>
#include <stack>
#include <string>

template <class T>
class DatabaseWorkerPool;

// A helper class to initiate all database worker pools,
// handles updating, delays preparing of statements and cleans up on failure.
class TC_DATABASE_API DatabaseLoader
{
public:
    DatabaseLoader();

    // Register a database to the loader (lazy implemented)
    template <class T>
    DatabaseLoader& AddDatabase(DatabaseWorkerPool<T>& pool, std::string const& dbString,
        const uint8 asyncThreads = 8, const uint8 syncThreads = 2);

    // Load all databases
    bool Load();

    enum DatabaseTypeFlags
    {
        DATABASE_NONE       = 0,

        DATABASE_SAKILA     = 1,

        DATABASE_MASK_ALL   = DATABASE_SAKILA
    };

private:
    bool OpenDatabases();
    bool PrepareStatements();

    using Predicate = std::function<bool()>;
    using Closer = std::function<void()>;

    // Invokes all functions in the given queue and closes the databases on errors.
    // Returns false when there was an error.
    bool Process(std::queue<Predicate>& queue);

    std::string const _logger;

    std::queue<Predicate> _open, _prepare;
    std::stack<Closer> _close;
};

#endif // DatabaseLoader_h__
