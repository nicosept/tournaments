#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "delegate/BracketGenerator.hpp"
#include "domain/Match.hpp"
#include "domain/Team.hpp"

class BracketGeneratorTest : public ::testing::Test {
protected:
    std::unique_ptr<BracketGenerator> generator;
    std::string tournamentId = "test-tournament-123";
    std::vector<domain::Team> teams;

    void SetUp() override {
        generator = std::make_unique<BracketGenerator>();
        
        // Create 32 teams
        for (int i = 1; i <= 32; ++i) {
            domain::Team team;
            team.Id = "team-" + std::to_string(i);
            team.Name = "Team " + std::to_string(i);
            teams.push_back(team);
        }
    }
    
    // Helper to check if match name starts with W/L/F
    bool MatchNameStartsWith(const domain::Match& match, const std::string& prefix) {
        return match.Name().substr(0, prefix.length()) == prefix;
    }
};

TEST_F(BracketGeneratorTest, GeneratesExactly63Matches) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    EXPECT_EQ(matches.size(), 63) << "Double elimination for 32 teams should generate 63 matches (2n-1)";
}

TEST_F(BracketGeneratorTest, WinnersBracketHas31Matches) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    int winnersCount = 0;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "W")) {
            winnersCount++;
        }
    }
    
    EXPECT_EQ(winnersCount, 31) << "Winners bracket should have 31 matches (W0-W30)";
}

TEST_F(BracketGeneratorTest, LosersBracketHas30Matches) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    int losersCount = 0;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "L")) {
            losersCount++;
        }
    }
    
    EXPECT_EQ(losersCount, 30) << "Losers bracket should have 30 matches (L0-L29)";
}

TEST_F(BracketGeneratorTest, FinalsHas2Matches) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    int finalsCount = 0;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "F")) {
            finalsCount++;
        }
    }
    
    EXPECT_EQ(finalsCount, 2) << "Finals should have 2 matches (F0-F1) for bracket reset";
}

TEST_F(BracketGeneratorTest, First16MatchesHaveTeamsAssigned) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    // First 16 matches (W0-W15) should have teams assigned
    int matchesWithTeams = 0;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "W") && !match.HomeTeamId().empty() && !match.VisitorTeamId().empty()) {
            matchesWithTeams++;
        }
    }
    
    EXPECT_EQ(matchesWithTeams, 16) << "First 16 winners bracket matches should have teams assigned";
}

TEST_F(BracketGeneratorTest, AllTeamsAssignedToFirstRound) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    std::unordered_set<std::string> usedTeamIds;
    
    // Collect teams from first 16 matches (W0-W15)
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "W") && 
            !match.HomeTeamId().empty() && 
            !match.VisitorTeamId().empty()) {
            usedTeamIds.insert(match.HomeTeamId());
            usedTeamIds.insert(match.VisitorTeamId());
        }
    }
    
    EXPECT_EQ(usedTeamIds.size(), 32) << "All 32 teams should be assigned to first round";
}

TEST_F(BracketGeneratorTest, AllMatchesHaveTournamentId) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    for (const auto& match : matches) {
        EXPECT_EQ(match.TournamentId(), tournamentId) << "Match " << match.Name() << " should have tournament ID";
    }
}

TEST_F(BracketGeneratorTest, AllMatchesHaveUniqueName) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    std::unordered_set<std::string> names;
    for (const auto& match : matches) {
        EXPECT_FALSE(match.Name().empty()) << "All matches should have a name";
        EXPECT_EQ(names.count(match.Name()), 0) << "Match name " << match.Name() << " should be unique";
        names.insert(match.Name());
    }
    
    EXPECT_EQ(names.size(), 63) << "Should have 63 unique match names";
}

TEST_F(BracketGeneratorTest, WinnersMatchesNamedCorrectly) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    std::vector<std::string> winnersNames;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "W")) {
            winnersNames.push_back(match.Name());
        }
    }
    
    // Should have W0, W1, ..., W30
    ASSERT_EQ(winnersNames.size(), 31);
    
    // Check they're in order
    std::sort(winnersNames.begin(), winnersNames.end(), [](const std::string& a, const std::string& b) {
        int numA = std::stoi(a.substr(1));
        int numB = std::stoi(b.substr(1));
        return numA < numB;
    });
    
    for (int i = 0; i < 31; ++i) {
        EXPECT_EQ(winnersNames[i], "W" + std::to_string(i)) << "Winners match " << i << " should be named W" << i;
    }
}

TEST_F(BracketGeneratorTest, LosersMatchesNamedCorrectly) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    std::vector<std::string> losersNames;
    for (const auto& match : matches) {
        if (MatchNameStartsWith(match, "L")) {
            losersNames.push_back(match.Name());
        }
    }
    
    // Should have L0, L1, ..., L29
    ASSERT_EQ(losersNames.size(), 30);
    
    std::sort(losersNames.begin(), losersNames.end(), [](const std::string& a, const std::string& b) {
        int numA = std::stoi(a.substr(1));
        int numB = std::stoi(b.substr(1));
        return numA < numB;
    });
    
    for (int i = 0; i < 30; ++i) {
        EXPECT_EQ(losersNames[i], "L" + std::to_string(i)) << "Losers match " << i << " should be named L" << i;
    }
}

TEST_F(BracketGeneratorTest, FinalsMatchesNamedCorrectly) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    bool hasF0 = false;
    bool hasF1 = false;
    
    for (const auto& match : matches) {
        if (match.Name() == "F0") hasF0 = true;
        if (match.Name() == "F1") hasF1 = true;
    }
    
    EXPECT_TRUE(hasF0) << "Should have final match F0";
    EXPECT_TRUE(hasF1) << "Should have final match F1";
}

TEST_F(BracketGeneratorTest, RemainingMatchesHaveNoTeamsAssigned) {
    auto matches = generator->GenerateMatches(tournamentId, teams);
    
    int emptyMatches = 0;
    for (const auto& match : matches) {
        // Skip first 16 winners matches (W0-W15)
        if (MatchNameStartsWith(match, "W")) {
            int num = std::stoi(match.Name().substr(1));
            if (num >= 16) {
                EXPECT_TRUE(match.HomeTeamId().empty()) << "Match " << match.Name() << " should have no home team";
                EXPECT_TRUE(match.VisitorTeamId().empty()) << "Match " << match.Name() << " should have no visitor team";
                emptyMatches++;
            }
        } else {
            // All losers and finals should be empty
            EXPECT_TRUE(match.HomeTeamId().empty()) << "Match " << match.Name() << " should have no home team";
            EXPECT_TRUE(match.VisitorTeamId().empty()) << "Match " << match.Name() << " should have no visitor team";
            emptyMatches++;
        }
    }
    
    EXPECT_EQ(emptyMatches, 47) << "47 matches (63 - 16) should start without teams";
}

TEST_F(BracketGeneratorTest, ThrowsExceptionForWrongNumberOfTeams) {
    std::vector<domain::Team> wrongTeams;
    for (int i = 0; i < 16; ++i) {
        domain::Team team;
        team.Id = "team-" + std::to_string(i);
        wrongTeams.push_back(team);
    }
    
    EXPECT_THROW(generator->GenerateMatches(tournamentId, wrongTeams), std::invalid_argument);
}
