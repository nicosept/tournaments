#ifndef TOURNAMENTS_DOUBLEELIMINATIONMATCHSTRATEGY_HPP
#define TOURNAMENTS_DOUBLEELIMINATIONMATCHSTRATEGY_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include "domain/Match.hpp"
#include "IMatchStrategy.hpp"

class DoubleEliminationMatchStrategy : public IMatchStrategy {
private:
    // Generate unique match ID: format "tournamentId_WR1M0" or "tournamentId_LR2M3"
    std::string GenerateMatchId(const std::string& tournamentId,
                                domain::BracketType bracket,
                                int round,
                                int matchNumber) const {
        std::string bracketPrefix = (bracket == domain::BracketType::WINNERS) ? "W" : "L";
        return tournamentId + "_" + bracketPrefix + "R" + std::to_string(round) + "M" + std::to_string(matchNumber);
    }

    // Create winners bracket matches: 16→8→4→2→1 (31 total matches)
    std::vector<domain::Match> CreateWinnersBracket(const std::string& tournamentId,
                                                    const std::string& groupId) {
        std::vector<domain::Match> matches;
        std::vector<int> roundSizes = {16, 8, 4, 2, 1};

        for (size_t roundIdx = 0; roundIdx < roundSizes.size(); ++roundIdx) {
            int roundNumber = roundIdx + 1;
            int matchesInRound = roundSizes[roundIdx];

            for (int matchNum = 0; matchNum < matchesInRound; ++matchNum) {
                domain::Match match;
                match.Id() = GenerateMatchId(tournamentId, domain::BracketType::WINNERS, roundNumber, matchNum);
                match.TournamentId() = tournamentId;
                match.GroupId() = groupId;
                match.Bracket() = domain::BracketType::WINNERS;
                match.RoundNumber() = roundNumber;
                match.MatchNumberInRound() = matchNum;
                match.Status() = domain::MatchStatus::PENDING;

                // Winner advances to next winners round (2 matches combine into 1)
                if (roundIdx < roundSizes.size() - 1) {
                    int nextMatchNum = matchNum / 2;
                    match.NextMatchWinnerId() = GenerateMatchId(tournamentId, domain::BracketType::WINNERS, roundNumber + 1, nextMatchNum);
                }

                matches.push_back(match);
            }
        }

        return matches;
    }

    // Create losers bracket matches: 8→8→4→4→2→2→1→1→1 (30 total matches)
    // Alternates between receiving new losers and consolidation rounds
    std::vector<domain::Match> CreateLosersBracket(const std::string& tournamentId,
                                                   const std::string& groupId) {
        std::vector<domain::Match> matches;
        std::vector<int> roundSizes = {8, 8, 4, 4, 2, 2, 1, 1, 1};

        for (size_t roundIdx = 0; roundIdx < roundSizes.size(); ++roundIdx) {
            int roundNumber = roundIdx + 1;
            int matchesInRound = roundSizes[roundIdx];

            for (int matchNum = 0; matchNum < matchesInRound; ++matchNum) {
                domain::Match match;
                match.Id() = GenerateMatchId(tournamentId, domain::BracketType::LOSERS, roundNumber, matchNum);
                match.TournamentId() = tournamentId;
                match.GroupId() = groupId;
                match.Bracket() = domain::BracketType::LOSERS;
                match.RoundNumber() = roundNumber;
                match.MatchNumberInRound() = matchNum;
                match.Status() = domain::MatchStatus::PENDING;

                // Winner advances to next losers round
                if (roundIdx < roundSizes.size() - 1) {
                    int nextMatchNum = matchNum / 2;
                    match.NextMatchWinnerId() = GenerateMatchId(tournamentId, domain::BracketType::LOSERS, roundNumber + 1, nextMatchNum);
                }

                matches.push_back(match);
            }
        }

        return matches;
    }

    // Link losers from winners bracket to appropriate losers bracket matches
    void LinkBracketsLoserPaths(std::vector<domain::Match>& winnersMatches,
                               const std::vector<domain::Match>& losersMatches) {
        // Winners R1 (16 matches) → Losers R1 (8 matches): 2 losers per L.B. match
        for (int i = 0; i < 16; ++i) {
            int loserMatchNum = i / 2;
            winnersMatches[i].NextMatchLoserId() = losersMatches[loserMatchNum].Id();
        }

        // Winners R2 (8 matches) → Losers R2 (8 matches): 1-to-1 mapping
        for (int i = 0; i < 8; ++i) {
            winnersMatches[16 + i].NextMatchLoserId() = losersMatches[8 + i].Id();
        }

        // Winners R3 (4 matches) → Losers R4 (4 matches): Skip R3 (consolidation round)
        for (int i = 0; i < 4; ++i) {
            winnersMatches[24 + i].NextMatchLoserId() = losersMatches[16 + i].Id();
        }

        // Winners R4 (2 matches) → Losers R6 (2 matches): Skip R5 (consolidation round)
        for (int i = 0; i < 2; ++i) {
            winnersMatches[28 + i].NextMatchLoserId() = losersMatches[24 + i].Id();
        }

        // Winners R5 (Winners Final) → Losers R8 (Losers Final): Skip R7 (consolidation)
        winnersMatches[30].NextMatchLoserId() = losersMatches[32].Id();
    }

    // Create grand final matches (2 matches total for bracket reset)
    std::vector<domain::Match> CreateGrandFinal(const std::string& tournamentId,
                                               const std::string& groupId) {
        std::vector<domain::Match> matches;

        // Grand Final - First Match (Round 6)
        domain::Match grandFinal1;
        grandFinal1.Id() = GenerateMatchId(tournamentId, domain::BracketType::WINNERS, 6, 0);
        grandFinal1.TournamentId() = tournamentId;
        grandFinal1.GroupId() = groupId;
        grandFinal1.Bracket() = domain::BracketType::WINNERS;
        grandFinal1.RoundNumber() = 6;
        grandFinal1.MatchNumberInRound() = 0;
        grandFinal1.Status() = domain::MatchStatus::PENDING;
        grandFinal1.IsGrandFinal() = true;
        grandFinal1.IsBracketReset() = false;

        // Grand Final - Bracket Reset (Round 7)
        domain::Match grandFinal2;
        grandFinal2.Id() = GenerateMatchId(tournamentId, domain::BracketType::WINNERS, 7, 0);
        grandFinal2.TournamentId() = tournamentId;
        grandFinal2.GroupId() = groupId;
        grandFinal2.Bracket() = domain::BracketType::WINNERS;
        grandFinal2.RoundNumber() = 7;
        grandFinal2.MatchNumberInRound() = 0;
        grandFinal2.Status() = domain::MatchStatus::PENDING;
        grandFinal2.IsGrandFinal() = true;
        grandFinal2.IsBracketReset() = true;

        // Link: Grand Final 1 winner → Grand Final 2 (bracket reset)
        grandFinal1.NextMatchWinnerId() = grandFinal2.Id();

        matches.push_back(grandFinal1);
        matches.push_back(grandFinal2);

        return matches;
    }

public:
    // Main method: Generate all 63 matches for a 32-team double elimination tournament
    std::vector<domain::Match> GenerateMatches(const std::string& tournamentId,
                                               const std::string& groupId) {
        std::vector<domain::Match> allMatches;

        // Step 1: Create winners bracket (31 matches: 16+8+4+2+1)
        auto winnersMatches = CreateWinnersBracket(tournamentId, groupId);

        // Step 2: Create losers bracket (30 matches: 8+8+4+4+2+2+1+1+1)
        auto losersMatches = CreateLosersBracket(tournamentId, groupId);

        // Step 3: Link losers from winners bracket to losers bracket
        LinkBracketsLoserPaths(winnersMatches, losersMatches);

        // Step 4: Create grand final matches (2 matches)
        auto grandFinalMatches = CreateGrandFinal(tournamentId, groupId);

        // Step 5: Link finals to grand final
        winnersMatches[30].NextMatchWinnerId() = grandFinalMatches[0].Id(); // Winners Final → GF1
        losersMatches[32].NextMatchWinnerId() = grandFinalMatches[0].Id();  // Losers Final → GF1

        // Step 6: Combine all matches (31 + 30 + 2 = 63)
        allMatches.reserve(63);
        allMatches.insert(allMatches.end(), winnersMatches.begin(), winnersMatches.end());
        allMatches.insert(allMatches.end(), losersMatches.begin(), losersMatches.end());
        allMatches.insert(allMatches.end(), grandFinalMatches.begin(), grandFinalMatches.end());

        return allMatches;
    }
};

#endif //TOURNAMENTS_DOUBLEELIMINATIONMATCHSTRATEGY_HPP