//
// Created by developer on 10/13/25.
//

#ifndef CONSUMER_MATCHDELEGATE_HPP
#define CONSUMER_MATCHDELEGATE_HPP

#include <memory>
#include <iostream>

#include "event/TeamAddEvent.hpp"
#include "event/ScoreUpdateEvent.hpp"
#include "delegate/BracketGenerator.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/repository/IMatchRepository.hpp"

class MatchDelegate {
    std::shared_ptr<IMatchRepository> matchRepository;
    std::shared_ptr<GroupRepository> groupRepository;
    std::unique_ptr<BracketGenerator> bracketGenerator;

public:
    MatchDelegate(const std::shared_ptr<IMatchRepository>& matchRepository, const std::shared_ptr<GroupRepository>& groupRepository);
    void ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent);
    void ProcessScoreUpdate(const domain::ScoreUpdateEvent& scoreUpdateEvent);

private:
    std::string GetWinnerNextMatch(const std::string& matchName);
    std::string GetLoserNextMatch(const std::string& matchName);
    void AdvanceTeamToNextMatch(const std::string& tournamentId, const std::string& nextMatchName, const std::string& teamId, bool isHome);
};

inline MatchDelegate::MatchDelegate(const std::shared_ptr<IMatchRepository> &matchRepository, const std::shared_ptr<GroupRepository> &groupRepository)
: matchRepository(matchRepository), groupRepository(groupRepository), bracketGenerator(std::make_unique<BracketGenerator>()) {}

inline void MatchDelegate::ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent) {
    std::cout << "[MatchDelegate] Processing team addition for tournament: " << teamAddEvent.tournamentId << std::endl;
    
    auto group = groupRepository->FindByTournamentIdAndGroupId(teamAddEvent.tournamentId, teamAddEvent.groupId);
    if (group != nullptr && group->Teams().size() == 32) {
        std::cout << "creating matches for " << teamAddEvent.tournamentId << " with " << group->Teams().size() << " teams" << std::endl;
        // Generate matches using BracketGenerator
        auto matches = bracketGenerator->GenerateMatches(teamAddEvent.tournamentId, group->Teams());
        // Bulk insert matches into the repository
        matchRepository->CreateBulk(matches);
    }
    std::cout << teamAddEvent.tournamentId << " wait for teams, current teams: " << (group ? group->Teams().size() : 0) << std::endl;
}

inline void MatchDelegate::ProcessScoreUpdate(const domain::ScoreUpdateEvent& scoreUpdateEvent) {
    std::cout << "[MatchDelegate] Processing score update for match: " << scoreUpdateEvent.matchId << std::endl;
    
    // Get the match from repository
    auto match = matchRepository->FindByTournamentIdAndMatchId(scoreUpdateEvent.tournamentId, scoreUpdateEvent.matchId);
    if (!match) {
        std::cout << "[MatchDelegate] ERROR: Match not found: " << scoreUpdateEvent.matchId << std::endl;
        return;
    }
    
    // Check if both teams are assigned
    if (match->HomeTeamId().empty() || match->VisitorTeamId().empty()) {
        std::cout << "[MatchDelegate] WARNING: Match " << match->Name() << " does not have both teams assigned yet" << std::endl;
        return;
    }
    
    // Determine winner and loser
    std::string winnerTeamId, loserTeamId;
    bool homeWon = false;
    if (scoreUpdateEvent.homeTeamScore > scoreUpdateEvent.visitorTeamScore) {
        winnerTeamId = match->HomeTeamId();
        loserTeamId = match->VisitorTeamId();
        homeWon = true;
    } else if (scoreUpdateEvent.visitorTeamScore > scoreUpdateEvent.homeTeamScore) {
        winnerTeamId = match->VisitorTeamId();
        loserTeamId = match->HomeTeamId();
        homeWon = false;
    } else {
        std::cout << "[MatchDelegate] WARNING: Match " << match->Name() << " ended in a tie, no advancement" << std::endl;
        return;
    }
    
    std::cout << "[MatchDelegate] Winner team: " << winnerTeamId << ", Loser team: " << loserTeamId << std::endl;
    
    // Special handling for F0
    if (match->Name() == "F0") {
        if (!homeWon) {
            // Losers bracket champion won F0 -> bracket reset to F1
            std::cout << "[MatchDelegate] Losers bracket champion won F0, advancing to F1 for bracket reset" << std::endl;
            AdvanceTeamToNextMatch(scoreUpdateEvent.tournamentId, "F1", winnerTeamId, false); // visitor
            AdvanceTeamToNextMatch(scoreUpdateEvent.tournamentId, "F1", loserTeamId, true);   // home gets second chance
        } else {
            // Winners bracket champion won F0 -> tournament over
            std::cout << "[MatchDelegate] Winners bracket champion won F0, tournament complete!" << std::endl;
        }
        return;
    }
    
    // Get next matches based on match name
    std::string winnerNextMatch = GetWinnerNextMatch(match->Name());
    std::string loserNextMatch = GetLoserNextMatch(match->Name());
    
    // Advance winner to next match
    if (!winnerNextMatch.empty()) {
        auto nextMatch = matchRepository->FindByTournamentIdAndName(scoreUpdateEvent.tournamentId, winnerNextMatch);
        if (nextMatch) {
            // Assign to first available slot (home if empty, otherwise visitor)
            bool assignToHome = nextMatch->HomeTeamId().empty();
            AdvanceTeamToNextMatch(scoreUpdateEvent.tournamentId, winnerNextMatch, winnerTeamId, assignToHome);
            std::cout << "[MatchDelegate] Advanced winner " << winnerTeamId << " to match " << winnerNextMatch << std::endl;
        }
    } else {
        std::cout << "[MatchDelegate] Match " << match->Name() << " is a final match, no winner advancement" << std::endl;
    }
    
    // Advance loser to losers bracket (if applicable)
    if (!loserNextMatch.empty()) {
        auto nextMatch = matchRepository->FindByTournamentIdAndName(scoreUpdateEvent.tournamentId, loserNextMatch);
        if (nextMatch) {
            bool assignToHome = nextMatch->HomeTeamId().empty();
            AdvanceTeamToNextMatch(scoreUpdateEvent.tournamentId, loserNextMatch, loserTeamId, assignToHome);
            std::cout << "[MatchDelegate] Advanced loser " << loserTeamId << " to match " << loserNextMatch << std::endl;
        }
    }
}

inline std::string MatchDelegate::GetWinnerNextMatch(const std::string& matchName) {
    // Winners bracket advancement (W0-W30)
    if (matchName[0] == 'W') {
        int matchNum = std::stoi(matchName.substr(1));
        
        // Round 1 (W0-W15) -> Round 2 (W16-W23)
        if (matchNum >= 0 && matchNum <= 15) {
            return "W" + std::to_string(16 + matchNum / 2);
        }
        // Round 2 (W16-W23) -> Round 3 (W24-W27)
        if (matchNum >= 16 && matchNum <= 23) {
            return "W" + std::to_string(24 + (matchNum - 16) / 2);
        }
        // Round 3 (W24-W27) -> Round 4 (W28-W29)
        if (matchNum >= 24 && matchNum <= 27) {
            return "W" + std::to_string(28 + (matchNum - 24) / 2);
        }
        // Round 4 (W28-W29) -> Round 5 (W30)
        if (matchNum >= 28 && matchNum <= 29) {
            return "W30";
        }
        // W30 -> F0 (Finals)
        if (matchNum == 30) {
            return "F0";
        }
    }
    
    // Losers bracket advancement (L0-L29)
    if (matchName[0] == 'L') {
        int matchNum = std::stoi(matchName.substr(1));
        
        // L0-L7 -> L8-L15
        if (matchNum >= 0 && matchNum <= 7) {
            return "L" + std::to_string(8 + matchNum);
        }
        // L8-L15 -> L16-L19
        if (matchNum >= 8 && matchNum <= 15) {
            return "L" + std::to_string(16 + (matchNum - 8) / 2);
        }
        // L16-L19 -> L20-L23
        if (matchNum >= 16 && matchNum <= 19) {
            return "L" + std::to_string(20 + matchNum - 16);
        }
        // L20-L23 -> L24-L25
        if (matchNum >= 20 && matchNum <= 23) {
            return "L" + std::to_string(24 + (matchNum - 20) / 2);
        }
        // L24-L25 -> L26-L27
        if (matchNum >= 24 && matchNum <= 25) {
            return "L" + std::to_string(26 + matchNum - 24);
        }
        // L26-L27 -> L28
        if (matchNum >= 26 && matchNum <= 27) {
            return "L28";
        }
        // L28 -> L29
        if (matchNum == 28) {
            return "L29";
        }
        // L29 -> F0 (Finals)
        if (matchNum == 29) {
            return "F0";
        }
    }
    
    // Finals (F0)
    if (matchName == "F0") {
        // If loser bracket wins, go to F1 (bracket reset)
        return ""; // F0 winner wins tournament (unless from losers bracket)
    }
    
    // F1 is the absolute final
    if (matchName == "F1") {
        return ""; // Tournament complete
    }
    
    return "";
}

inline std::string MatchDelegate::GetLoserNextMatch(const std::string& matchName) {

    if (matchName[0] == 'W') {
        int matchNum = std::stoi(matchName.substr(1));
        
        // W0-W15 losers -> L0-L7
        if (matchNum >= 0 && matchNum <= 15) {
            // W0,W1 -> L0, W2,W3 -> L1, etc.
            return "L" + std::to_string(matchNum / 2);
        }
        // W16-W23 losers -> L8-L15
        if (matchNum >= 16 && matchNum <= 23) {
            return "L" + std::to_string(8 + (matchNum - 16));
        }
        // W24-W27 losers -> L20-L23
        if (matchNum >= 24 && matchNum <= 27) {
            return "L" + std::to_string(20 + (matchNum - 24));
        }
        // W28-W29 losers -> L26-L27
        if (matchNum >= 28 && matchNum <= 29) {
            return "L" + std::to_string(26 + (matchNum - 28));
        }
        // W30 loser -> L29
        if (matchNum == 30) {
            return "L29";
        }
    }
    
    // Losers bracket losers are eliminated (no next match)
    if (matchName[0] == 'L') {
        return ""; // Eliminated
    }
    
    // Finals losers
    if (matchName == "F0") {
        return ""; // One loss in finals = eliminated
    }
    
    if (matchName == "F1") {
        return ""; // Tournament over
    }
    
    return "";
}

inline void MatchDelegate::AdvanceTeamToNextMatch(const std::string& tournamentId, const std::string& nextMatchName, const std::string& teamId, bool isHome) {
    auto nextMatch = matchRepository->FindByTournamentIdAndName(tournamentId, nextMatchName);
    if (!nextMatch) {
        std::cout << "[MatchDelegate] ERROR: Next match " << nextMatchName << " not found" << std::endl;
        return;
    }
    
    // Update the next match with the advancing team
    if (isHome) {
        nextMatch->HomeTeamId() = teamId;
    } else {
        nextMatch->VisitorTeamId() = teamId;
    }
    
    // Save the updated match
    matchRepository->Update(nextMatch->Id(), *nextMatch);
    std::cout << "[MatchDelegate] Team " << teamId << " assigned to match " << nextMatchName << " as " << (isHome ? "home" : "visitor") << std::endl;
}

#endif //CONSUMER_MATCHDELEGATE_HPP