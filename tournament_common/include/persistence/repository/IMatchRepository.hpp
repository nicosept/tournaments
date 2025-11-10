#ifndef TOURNAMENTS_IMATCHREPOSITORY_HPP
#define TOURNAMENTS_IMATCHREPOSITORY_HPP

#include <string_view>
#include <vector>
#include <memory>

#include "domain/Match.hpp"

class IMatchRepository : public IRepository<domain::Match, std::string> {
public:
    virtual ~IMatchRepository() = default;

    // Find match with only one team to be added
    virtual std::shared_ptr<domain::Match> FindLastOpenMatch(const std::string_view& tournamentId) = 0;

    // Find matches by tournament and round
    virtual std::vector<domain::Match> FindMatchesByTournamentAndRound(const std::string_view& tournamentId) = 0;

    // Bulk create matches for tournament initialization
    virtual std::vector<std::string> CreateBulk(const std::vector<domain::Match>& matches) = 0;

    // Check if matches already exist for a tournament
    virtual bool MatchesExistForTournament(const std::string_view& tournamentId, const std::string_view& groupId) = 0;

    // Find all matches for a tournament
    virtual std::vector<domain::Match> FindByTournamentAndGroup(const std::string_view& tournamentId, const std::string_view& groupId) = 0;
};
#endif //TOURNAMENTS_IMATCHREPOSITORY_HPP