#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

#include "delegate/MatchDelegate.hpp"
#include "delegate/BracketGenerator.hpp"
#include "event/TeamAddEvent.hpp"
#include "event/ScoreUpdateEvent.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/configuration/IDbConnectionProvider.hpp"
#include "domain/Group.hpp"
#include "domain/Team.hpp"
#include "domain/Match.hpp"

// Mock del repositorio de Matches
class MockMatchRepository : public IMatchRepository {
public:
    MOCK_METHOD(std::shared_ptr<domain::Match>, FindByTournamentIdAndMatchId,
                (const std::string_view& tournamentId, const std::string_view& matchId), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, FindByTournamentId,
                (const std::string_view& tournamentId), (override));
    MOCK_METHOD(std::shared_ptr<domain::Match>, FindByTournamentIdAndName,
                (const std::string_view& tournamentId, const std::string_view& name), (override));
    MOCK_METHOD(std::vector<std::string>, CreateBulk, (const std::vector<domain::Match>& matches), (override));
    MOCK_METHOD(void, Update, (const std::string_view& matchId, const domain::Match& match), (override));
    MOCK_METHOD(void, UpdateMatchScore, (const std::string_view& matchId, const domain::Score& score), (override));
    MOCK_METHOD(bool, MatchesExistForTournament, (const std::string_view& tournamentId), (override));
};

// Mock del repositorio de Groups
class MockGroupRepository : public GroupRepository {
private:
    std::shared_ptr<IDbConnectionProvider> dummyProvider;
    
    struct DummyConnectionProvider : public IDbConnectionProvider {
        PooledConnection Connection() override { 
            return PooledConnection(nullptr, [](IDbConnection*){}); 
        }
    };
    
    static std::shared_ptr<IDbConnectionProvider> CreateDummyProvider() {
        static auto provider = std::make_shared<DummyConnectionProvider>();
        return provider;
    }

public:
    MockGroupRepository() : GroupRepository(CreateDummyProvider()), dummyProvider(CreateDummyProvider()) {}
    
    MOCK_METHOD(std::shared_ptr<domain::Group>, FindByTournamentIdAndGroupId,
                (const std::string_view& tournamentId, const std::string_view& groupId), (override));
};

class MatchDelegateTest : public ::testing::Test {
protected:
    std::shared_ptr<MockMatchRepository> mockMatchRepository;
    std::shared_ptr<MockGroupRepository> mockGroupRepository;
    std::shared_ptr<MatchDelegate> matchDelegate;

    void SetUp() override {
        mockMatchRepository = std::make_shared<MockMatchRepository>();
        mockGroupRepository = std::make_shared<MockGroupRepository>();
        
        matchDelegate = std::make_shared<MatchDelegate>(
            mockMatchRepository,
            mockGroupRepository
        );
    }
    
    // Helper para crear un grupo con N equipos
    std::shared_ptr<domain::Group> CreateGroupWithTeams(int teamCount, const std::string& groupId, const std::string& tournamentId) {
        auto group = std::make_shared<domain::Group>("Test Group", groupId);
        group->TournamentId() = tournamentId;
        
        for (int i = 0; i < teamCount; ++i) {
            domain::Team team;
            team.Id = "team-" + std::to_string(i);
            team.Name = "Team " + std::to_string(i);
            group->Teams().push_back(team);
        }
        
        return group;
    }
};

// ============================================================================
// Tests de ProcessTeamAddition - Estrategia de doble eliminación
// ============================================================================

// Test: Cuando el grupo tiene exactamente 32 equipos, se crean los partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_CreatesMatchesWhen32Teams) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-31";
    
    auto group = CreateGroupWithTeams(32, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    // Verificar que CreateBulk es llamado con 63 partidos (2n-1 para doble eliminación)
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .WillOnce(testing::Invoke([](const std::vector<domain::Match>& matches) {
            EXPECT_EQ(matches.size(), 63) << "Should create 63 matches for 32 teams in double elimination";
            return std::vector<std::string>(63, "match-id");
        }));
    
    // No debe lanzar excepción
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Cuando el grupo tiene menos de 32 equipos, NO se crean partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_DoesNothingWhenIncompleteTournament_31Teams) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-30";
    
    auto group = CreateGroupWithTeams(31, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    // CreateBulk NO debe ser llamado
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .Times(0);
    
    // No debe lanzar excepción, simplemente no hace nada
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Cuando el grupo tiene 16 equipos, NO se crean partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_DoesNothingWhenIncompleteTournament_16Teams) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-15";
    
    auto group = CreateGroupWithTeams(16, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    // CreateBulk NO debe ser llamado
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Cuando el grupo tiene 1 equipo, NO se crean partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_DoesNothingWhenIncompleteTournament_1Team) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-0";
    
    auto group = CreateGroupWithTeams(1, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    // CreateBulk NO debe ser llamado
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Cuando el grupo está vacío (0 equipos), NO se crean partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_DoesNothingWhenIncompleteTournament_EmptyGroup) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-0";
    
    auto group = CreateGroupWithTeams(0, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    // CreateBulk NO debe ser llamado
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Cuando el grupo no existe (nullptr), NO se crean partidos
TEST_F(MatchDelegateTest, ProcessTeamAddition_DoesNothingWhenGroupNotFound) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-0";
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(nullptr));
    
    // CreateBulk NO debe ser llamado
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// Test: Verificar que los partidos creados pertenecen al torneo correcto
TEST_F(MatchDelegateTest, ProcessTeamAddition_CreatedMatchesBelongToTournament) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string groupId = "660e8400-e29b-41d4-a716-446655440001";
    
    domain::TeamAddEvent event;
    event.tournamentId = tournamentId;
    event.groupId = groupId;
    event.teamId = "team-31";
    
    auto group = CreateGroupWithTeams(32, groupId, tournamentId);
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(tournamentId, groupId))
        .WillOnce(testing::Return(group));
    
    EXPECT_CALL(*mockMatchRepository, CreateBulk(testing::_))
        .WillOnce(testing::Invoke([tournamentId](const std::vector<domain::Match>& matches) {
            // Verificar que todos los partidos pertenecen al torneo correcto
            for (const auto& match : matches) {
                EXPECT_EQ(match.TournamentId(), tournamentId) 
                    << "Match " << match.Name() << " should belong to tournament " << tournamentId;
            }
            return std::vector<std::string>(matches.size(), "match-id");
        }));
    
    EXPECT_NO_THROW(matchDelegate->ProcessTeamAddition(event));
}

// ============================================================================
// Tests de ProcessScoreUpdate - Avance de equipos en doble eliminación
// ============================================================================

// Test: Ganador de W0 avanza a W16 (Winners Bracket Round 1 -> Round 2)
TEST_F(MatchDelegateTest, ProcessScoreUpdate_WinnerAdvancesFromW0ToW16) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-w0";
    std::string homeTeamId = "team-home";
    std::string visitorTeamId = "team-visitor";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 1;
    
    // Current match (W0)
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "W0";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    // Next match for winner (W16)
    auto nextMatchWinner = std::make_shared<domain::Match>();
    nextMatchWinner->Id() = "match-w16";
    nextMatchWinner->Name() = "W16";
    nextMatchWinner->TournamentId() = tournamentId;
    nextMatchWinner->HomeTeamId() = ""; // Empty
    nextMatchWinner->VisitorTeamId() = "";
    
    // Next match for loser (L0)
    auto nextMatchLoser = std::make_shared<domain::Match>();
    nextMatchLoser->Id() = "match-l0";
    nextMatchLoser->Name() = "L0";
    nextMatchLoser->TournamentId() = tournamentId;
    nextMatchLoser->HomeTeamId() = "";
    nextMatchLoser->VisitorTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "W16"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchWinner));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "L0"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchLoser));
    
    // Verificar que el ganador (homeTeam) se asigna a W16
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-w16"), testing::_))
        .WillOnce(testing::Invoke([homeTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.HomeTeamId(), homeTeamId) << "Winner should be assigned to W16 as home";
        }));
    
    // Verificar que el perdedor (visitorTeam) se asigna a L0
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-l0"), testing::_))
        .WillOnce(testing::Invoke([visitorTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.HomeTeamId(), visitorTeamId) << "Loser should be assigned to L0 as home";
        }));
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Perdedor de W0 cae al bracket de perdedores en L0
TEST_F(MatchDelegateTest, ProcessScoreUpdate_LoserFallsToLosersBracket) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-w1";
    std::string homeTeamId = "team-home";
    std::string visitorTeamId = "team-visitor";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 2;
    event.visitorTeamScore = 3; // Visitor wins
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "W1";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    auto nextMatchWinner = std::make_shared<domain::Match>();
    nextMatchWinner->Id() = "match-w16";
    nextMatchWinner->Name() = "W16";
    nextMatchWinner->HomeTeamId() = "existing-team";
    nextMatchWinner->VisitorTeamId() = "";
    
    auto nextMatchLoser = std::make_shared<domain::Match>();
    nextMatchLoser->Id() = "match-l0";
    nextMatchLoser->Name() = "L0";
    nextMatchLoser->HomeTeamId() = "existing-loser";
    nextMatchLoser->VisitorTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "W16"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchWinner));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "L0"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchLoser));
    
    // Ganador (visitor) va a W16 como visitor (home ya ocupado)
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-w16"), testing::_))
        .WillOnce(testing::Invoke([visitorTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.VisitorTeamId(), visitorTeamId);
        }));
    
    // Perdedor (home) va a L0
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-l0"), testing::_))
        .WillOnce(testing::Invoke([homeTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.VisitorTeamId(), homeTeamId);
        }));
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Ganador en losers bracket avanza (L0 -> L8)
TEST_F(MatchDelegateTest, ProcessScoreUpdate_WinnerAdvancesInLosersBracket) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-l0";
    std::string homeTeamId = "team-home";
    std::string visitorTeamId = "team-visitor";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 2;
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "L0";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    auto nextMatchWinner = std::make_shared<domain::Match>();
    nextMatchWinner->Id() = "match-l8";
    nextMatchWinner->Name() = "L8";
    nextMatchWinner->HomeTeamId() = "";
    nextMatchWinner->VisitorTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "L8"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchWinner));
    
    // Solo el ganador avanza, perdedor es eliminado (no hay Update para loser)
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-l8"), testing::_))
        .WillOnce(testing::Invoke([homeTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.HomeTeamId(), homeTeamId) << "Winner should advance to L8";
        }));
    
    // No debe buscar siguiente match para el perdedor (está eliminado)
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, testing::Not(testing::Eq("L8"))))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Perdedor en losers bracket es eliminado
TEST_F(MatchDelegateTest, ProcessScoreUpdate_LoserInLosersBracketIsEliminated) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-l5";
    std::string homeTeamId = "team-home";
    std::string visitorTeamId = "team-visitor";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 1;
    event.visitorTeamScore = 3; // Visitor wins
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "L5";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    auto nextMatchWinner = std::make_shared<domain::Match>();
    nextMatchWinner->Id() = "match-l13";
    nextMatchWinner->Name() = "L13";
    nextMatchWinner->HomeTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "L13"))
        .Times(2)
        .WillRepeatedly(testing::Return(nextMatchWinner));
    
    // Solo el ganador avanza
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-l13"), testing::_))
        .Times(1);
    
    // El perdedor no avanza a ningún lado (eliminado)
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Avance de W30 a F0 (Winners bracket final a Grand Finals)
TEST_F(MatchDelegateTest, ProcessScoreUpdate_W30WinnerAdvancesToF0) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-w30";
    std::string homeTeamId = "team-winners-champ";
    std::string visitorTeamId = "team-finalist";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 1;
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "W30";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    auto f0Match = std::make_shared<domain::Match>();
    f0Match->Id() = "match-f0";
    f0Match->Name() = "F0";
    f0Match->HomeTeamId() = "";
    f0Match->VisitorTeamId() = "";
    
    auto l29Match = std::make_shared<domain::Match>();
    l29Match->Id() = "match-l29";
    l29Match->Name() = "L29";
    l29Match->HomeTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "F0"))
        .Times(2)
        .WillRepeatedly(testing::Return(f0Match));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "L29"))
        .Times(2)
        .WillRepeatedly(testing::Return(l29Match));
    
    // Ganador va a F0
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-f0"), testing::_))
        .WillOnce(testing::Invoke([homeTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.HomeTeamId(), homeTeamId);
        }));
    
    // Perdedor va a L29 (última oportunidad en losers bracket)
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-l29"), testing::_))
        .WillOnce(testing::Invoke([visitorTeamId](const std::string_view&, const domain::Match& match) {
            EXPECT_EQ(match.HomeTeamId(), visitorTeamId);
        }));
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Bracket reset - Si ganador de losers bracket gana F0, se juega F1
TEST_F(MatchDelegateTest, ProcessScoreUpdate_F0BracketResetWhenLosersBracketWins) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-f0";
    std::string homeTeamId = "team-winners-champ";
    std::string visitorTeamId = "team-losers-champ";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 1;
    event.visitorTeamScore = 3; // Losers bracket champion wins!
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "F0";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    auto f1Match = std::make_shared<domain::Match>();
    f1Match->Id() = "match-f1";
    f1Match->Name() = "F1";
    f1Match->HomeTeamId() = "";
    f1Match->VisitorTeamId() = "";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(tournamentId, "F1"))
        .Times(2) // Se llama dos veces, una para cada equipo
        .WillRepeatedly(testing::Return(f1Match));
    
    // Ambos equipos avanzan a F1 (bracket reset)
    EXPECT_CALL(*mockMatchRepository, Update(testing::Eq("match-f1"), testing::_))
        .Times(2); // Se actualiza dos veces
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: Si ganador de winners bracket gana F0, torneo termina (no F1)
TEST_F(MatchDelegateTest, ProcessScoreUpdate_F0TournamentEndsWhenWinnersBracketWins) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-f0";
    std::string homeTeamId = "team-winners-champ";
    std::string visitorTeamId = "team-losers-champ";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 1; // Winners bracket champion wins!
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "F0";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = homeTeamId;
    currentMatch->VisitorTeamId() = visitorTeamId;
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    // No debe buscar F1 ni actualizar nada (torneo terminado)
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(testing::_, testing::_))
        .Times(0);
    
    EXPECT_CALL(*mockMatchRepository, Update(testing::_, testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: No se hace nada si el match no existe
TEST_F(MatchDelegateTest, ProcessScoreUpdate_DoesNothingWhenMatchNotFound) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "non-existent-match";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 1;
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(nullptr));
    
    // No debe intentar actualizar nada
    EXPECT_CALL(*mockMatchRepository, Update(testing::_, testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: No se hace nada si el match no tiene ambos equipos asignados
TEST_F(MatchDelegateTest, ProcessScoreUpdate_DoesNothingWhenTeamsNotAssigned) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-w20";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 3;
    event.visitorTeamScore = 1;
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "W20";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = "team-home";
    currentMatch->VisitorTeamId() = ""; // Visitor no asignado aún
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    // No debe intentar avanzar equipos
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(testing::_, testing::_))
        .Times(0);
    
    EXPECT_CALL(*mockMatchRepository, Update(testing::_, testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

// Test: No se hace nada en caso de empate
TEST_F(MatchDelegateTest, ProcessScoreUpdate_DoesNothingOnTie) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "match-w5";
    
    domain::ScoreUpdateEvent event;
    event.tournamentId = tournamentId;
    event.matchId = matchId;
    event.homeTeamScore = 2;
    event.visitorTeamScore = 2; // Empate
    
    auto currentMatch = std::make_shared<domain::Match>();
    currentMatch->Id() = matchId;
    currentMatch->Name() = "W5";
    currentMatch->TournamentId() = tournamentId;
    currentMatch->HomeTeamId() = "team-home";
    currentMatch->VisitorTeamId() = "team-visitor";
    
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(currentMatch));
    
    // No debe intentar avanzar equipos
    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndName(testing::_, testing::_))
        .Times(0);
    
    EXPECT_CALL(*mockMatchRepository, Update(testing::_, testing::_))
        .Times(0);
    
    EXPECT_NO_THROW(matchDelegate->ProcessScoreUpdate(event));
}

