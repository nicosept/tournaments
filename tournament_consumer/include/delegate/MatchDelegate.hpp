#ifndef CONSUMER_MATCHDELEGATE_HPP
#define CONSUMER_MATCHDELEGATE_HPP

#include <memory>
#include <print>

#include "event/TeamAddEvent.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "strategy/DoubleEliminationMatchStrategy.hpp"

class MatchDelegate {
    std::shared_ptr<IMatchRepository> matchRepository;
    std::shared_ptr<GroupRepository> groupRepository;
    std::unique_ptr<DoubleEliminationMatchStrategy> matchStrategy;

public:
    MatchDelegate(const std::shared_ptr<IMatchRepository>& matchRepository,
                  const std::shared_ptr<GroupRepository>& groupRepository);

    void ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent);

private:
    void CreateTournamentMatches(const std::string& tournamentId, const std::string& groupId);
};

inline MatchDelegate::MatchDelegate(
    const std::shared_ptr<IMatchRepository>& matchRepository,
    const std::shared_ptr<GroupRepository>& groupRepository)
    : matchRepository(matchRepository),
      groupRepository(groupRepository),
      matchStrategy(std::make_unique<DoubleEliminationMatchStrategy>()) {}

inline void MatchDelegate::ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent) {
    auto group = groupRepository->FindByTournamentIdAndGroupId(
        teamAddEvent.tournamentId,
        teamAddEvent.groupId
    );

    if (group == nullptr) {
        std::println("Group not found for tournament: {}, group: {}",
                    teamAddEvent.tournamentId,
                    teamAddEvent.groupId);
        return;
    }

    std::println("Tournament {} has {} teams",
                teamAddEvent.tournamentId,
                group->Teams().size());

    // Check if we have all 32 teams
    if (group->Teams().size() == 32) {
        // Check if matches already exist (idempotency)
        if (matchRepository->MatchesExistForTournament(teamAddEvent.tournamentId, teamAddEvent.groupId)) {
            std::println("Matches already exist for tournament: {}", teamAddEvent.tournamentId);
            return;
        }

        std::println("Creating matches for tournament: {}", teamAddEvent.tournamentId);
        CreateTournamentMatches(teamAddEvent.tournamentId, teamAddEvent.groupId);
        std::println("Successfully created 63 matches for tournament: {}", teamAddEvent.tournamentId);
    } else {
        std::println("Waiting for more teams. Current: {}, Required: 32", group->Teams().size());
    }
}

inline void MatchDelegate::CreateTournamentMatches(const std::string& tournamentId,
                                                   const std::string& groupId) {
    try {
        // Generate all 63 matches using the strategy
        auto matches = matchStrategy->GenerateMatches(tournamentId, groupId);

        // Verify we have exactly 63 matches
        if (matches.size() != 63) {
            std::println("ERROR: Expected 63 matches, generated {}", matches.size());
            return;
        }

        // Save all matches to database in one bulk operation
        auto createdIds = matchRepository->CreateBulk(matches);

        if (createdIds.size() != 63) {
            std::println("ERROR: Expected to create 63 matches, created {}", createdIds.size());
            return;
        }

        std::println("Successfully saved 63 matches to database");

    } catch (const std::exception& ex) {
        std::println("ERROR creating matches: {}", ex.what());
        throw;
    }
}

#endif //CONSUMER_MATCHDELEGATE_HPP