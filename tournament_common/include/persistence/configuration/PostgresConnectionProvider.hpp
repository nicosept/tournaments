//
// Created by tomas on 8/29/25.
//

#ifndef TOURNAMENTS_POSTGRESCONNECTIONPROVIDER_HPP
#define TOURNAMENTS_POSTGRESCONNECTIONPROVIDER_HPP
#include <condition_variable>
#include <queue>
#include <pqxx/pqxx>

#include "IDbConnectionProvider.hpp"
#include "PostgresConnection.hpp"

class PostgresConnectionProvider : public IDbConnectionProvider{
    std::string_view connectionString;
    size_t poolSize = 1;
    std::queue<std::unique_ptr<pqxx::connection>> connectionPool;
    std::mutex connectionPoolMutex;
    std::condition_variable connectionPoolCondition;

public:
    PostgresConnectionProvider(std::string_view connectionString, size_t poolSize) : connectionString(connectionString), poolSize(poolSize) {
        for (size_t i = 0; i < poolSize; i++) {
            connectionPool.push(std::make_unique<pqxx::connection>(connectionString.data()));
            connectionPool.back()->prepare("insert_tournament", "insert into TOURNAMENTS (document) values($1) RETURNING id");
            connectionPool.back()->prepare("select_tournament_by_id", "select * from TOURNAMENTS where id = $1");
            connectionPool.back()->prepare("update_tournament", "UPDATE TOURNAMENTS SET document = document || $1::jsonb WHERE id = $2 RETURNING document");
            connectionPool.back()->prepare("delete_tournament", "DELETE FROM TOURNAMENTS WHERE id = $1");
            connectionPool.back()->prepare("insert_team", "insert into TEAMS (document) values($1) RETURNING id");
            connectionPool.back()->prepare("select_team_by_id", "select * from TEAMS where id = $1");
            connectionPool.back()->prepare("update_team", "UPDATE TEAMS SET document = document || $1::jsonb WHERE id = $2 RETURNING document");
            connectionPool.back()->prepare("delete_team", "DELETE FROM TEAMS WHERE id = $1");
            connectionPool.back()->prepare("insert_group", "insert into GROUPS (tournament_id, document) values($1, $2) RETURNING id");
            connectionPool.back()->prepare("select_groups_by_tournament", "select * from GROUPS where tournament_id = $1");
            connectionPool.back()->prepare("select_group_in_tournament", R"(
                select * from groups
                where  tournament_id = $1
                and document @> jsonb_build_object('teams', jsonb_build_array(jsonb_build_object('id', $2::text)))
            )");

            connectionPool.back()->prepare("select_group_by_tournamentid_groupid", "select * from GROUPS where tournament_id = $1 and id = $2");
            connectionPool.back()->prepare("select_group_by_group_id_team_id", R"(
                select * from groups
                where id = $1
                and document @> jsonb_build_object('teams', jsonb_build_array(jsonb_build_object('id', $2::text)))
            )");
            connectionPool.back()->prepare("update_group", "UPDATE GROUPS SET document = $2, last_update_date = CURRENT_TIMESTAMP WHERE id = $1 RETURNING document");
            connectionPool.back()->prepare("update_group_add_team", R"(
                update groups
                    set document = jsonb_insert(
                            document, '{teams,-1}', $2
                                   ),
                    last_update_date = CURRENT_TIMESTAMP
                where id = $1
            )");
            connectionPool.back()->prepare("delete_group", "DELETE FROM GROUPS WHERE id = $1 RETURNING id");
            connectionPool.back()->prepare("select_matches_by_tournamentid", "select * from MATCHES where tournament_id = $1");
            connectionPool.back()->prepare("select_match_by_tournamentid_matchid", "select * from MATCHES where tournament_id = $1 and id = $2");
            connectionPool.back()->prepare("update_match_score", "UPDATE MATCHES SET document = $1, last_update_date = CURRENT_TIMESTAMP WHERE id = $2 RETURNING document");
        }
    }

    PooledConnection Connection() override {
        std::unique_lock lock(connectionPoolMutex);

        // wait until a connection is available
        connectionPoolCondition.wait(lock, [this] { return !connectionPool.empty(); });

        // take one out
        auto conn = std::move(connectionPool.front());
        connectionPool.pop();

        // build PostgresConnection wrapper (adapts pqxx::connection -> IDbConnection)
        auto dbc = new PostgresConnection(std::move(conn));

        // return a RAII PooledConnection
        return PooledConnection(
            dbc,
            [this](IDbConnection* dbc) {
                auto pc = dynamic_cast<PostgresConnection*>(dbc);

                {
                    std::lock_guard<std::mutex> lock(connectionPoolMutex);
                    connectionPool.push(std::move(pc->connection));
                }

                delete pc;
                connectionPoolCondition.notify_one();
            }
        );
    }
};
#endif //TOURNAMENTS_POSTGRESCONNECTIONPROVIDER_HPP