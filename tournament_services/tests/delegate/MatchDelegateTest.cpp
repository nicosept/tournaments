#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>

#include "domain/Match.hpp"
#include "delegate/MatchDelegate.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "persistence/repository/IRepository.hpp"
#include "persistence/configuration/IDbConnectionProvider.hpp"
#include "cms/IQueueMessageProducer.hpp"
#include "exception/Error.hpp"

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

// Mock del repositorio de Tournaments
class MockTournamentRepository : public IRepository<domain::Tournament, std::string> {
public:
    MOCK_METHOD(std::shared_ptr<domain::Tournament>, ReadById, (std::string id), (override));
    MOCK_METHOD(std::string, Create, (const domain::Tournament& entity), (override));
    MOCK_METHOD(std::string, Update, (const domain::Tournament& entity), (override));
    MOCK_METHOD(void, Delete, (std::string id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Tournament>>, ReadAll, (), (override));
};

// Adapter para que TournamentRepository pueda usar el mock
class TournamentRepositoryAdapter : public TournamentRepository {
private:
    std::shared_ptr<IRepository<domain::Tournament, std::string>> mock;
    
    static std::shared_ptr<IDbConnectionProvider> CreateDummyProvider() {
        static auto provider = std::make_shared<DummyConnectionProvider>();
        return provider;
    }
    
    struct DummyConnectionProvider : public IDbConnectionProvider {
        PooledConnection Connection() override { 
            return PooledConnection(nullptr, [](IDbConnection*){}); 
        }
    };

public:
    TournamentRepositoryAdapter(std::shared_ptr<IRepository<domain::Tournament, std::string>> mockRepo) 
        : TournamentRepository(CreateDummyProvider()), mock(mockRepo) {}
    
    std::shared_ptr<domain::Tournament> ReadById(std::string id) override { return mock->ReadById(id); }
    std::string Create(const domain::Tournament& entity) override { return mock->Create(entity); }
    std::string Update(const domain::Tournament& entity) override { return mock->Update(entity); }
    void Delete(std::string id) override { mock->Delete(id); }
    std::vector<std::shared_ptr<domain::Tournament>> ReadAll() override { return mock->ReadAll(); }
};

// Mock del productor de mensajes
class MockQueueMessageProducer : public IQueueMessageProducer {
public:
    MOCK_METHOD(void, SendMessage, (const std::string_view& message, const std::string_view& destination), (override));
};

class MatchDelegateTest : public ::testing::Test {
protected:
    std::shared_ptr<MockMatchRepository> mockMatchRepository;
    std::shared_ptr<MockTournamentRepository> mockTournamentRepository;
    std::shared_ptr<MockQueueMessageProducer> mockMessageProducer;
    std::shared_ptr<MatchDelegate> matchDelegate;

    void SetUp() override {
        mockMatchRepository = std::make_shared<MockMatchRepository>();
        mockTournamentRepository = std::make_shared<MockTournamentRepository>();
        mockMessageProducer = std::make_shared<MockQueueMessageProducer>();
        
        // Usar el adapter para que MatchDelegate pueda usar el mock
        auto tournamentRepositoryAdapter = std::make_shared<TournamentRepositoryAdapter>(mockTournamentRepository);
        
        matchDelegate = std::make_shared<MatchDelegate>(
            mockMatchRepository,
            tournamentRepositoryAdapter,
            mockMessageProducer
        );
    }
};


// Tests de GetMatches

// Validar busqueda exitosa de matches por tournament ID
TEST_F(MatchDelegateTest, GetMatches_Ok) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";

    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    tournament->Id() = tournamentId;

    auto match1 = std::make_shared<domain::Match>();
    match1->Id() = "660e8400-e29b-41d4-a716-446655440001";
    match1->TournamentId() = tournamentId;

    auto match2 = std::make_shared<domain::Match>();
    match2->Id() = "770e8400-e29b-41d4-a716-446655440002";
    match2->TournamentId() = tournamentId;

    std::vector<std::shared_ptr<domain::Match>> matches = {match1, match2};

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(tournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockMatchRepository, FindByTournamentId(tournamentId))
        .WillOnce(testing::Return(matches));

    auto result = matchDelegate->GetMatches(tournamentId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2);
}

// Validar error cuando el torneo no existe
TEST_F(MatchDelegateTest, GetMatches_TournamentNotFound) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(tournamentId)))
        .WillOnce(testing::Return(nullptr));

    auto result = matchDelegate->GetMatches(tournamentId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// Validar formato invalido de tournament ID
TEST_F(MatchDelegateTest, GetMatches_InvalidFormat) {
    std::string invalidTournamentId = "invalid-id-format!@#";

    auto result = matchDelegate->GetMatches(invalidTournamentId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// Validar lista vacia de matches
TEST_F(MatchDelegateTest, GetMatches_Empty) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";

    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    tournament->Id() = tournamentId;

    std::vector<std::shared_ptr<domain::Match>> emptyMatches;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(tournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockMatchRepository, FindByTournamentId(tournamentId))
        .WillOnce(testing::Return(emptyMatches));

    auto result = matchDelegate->GetMatches(tournamentId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);
}

// ============================================================================
// Tests de GetMatch
// ============================================================================

// Validar busqueda exitosa de un match especifico
TEST_F(MatchDelegateTest, GetMatch_Ok) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    tournament->Id() = tournamentId;

    auto match = std::make_shared<domain::Match>();
    match->Id() = matchId;
    match->TournamentId() = tournamentId;
    match->HomeTeamId() = "aa0e8400-e29b-41d4-a716-446655440011";
    match->VisitorTeamId() = "bb0e8400-e29b-41d4-a716-446655440022";

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(tournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(match));

    auto result = matchDelegate->GetMatch(tournamentId, matchId);

    ASSERT_TRUE(result.has_value());
    auto retrievedMatch = result.value();
    EXPECT_EQ(retrievedMatch->Id(), matchId);
    EXPECT_EQ(retrievedMatch->TournamentId(), tournamentId);
}

// Validar error cuando el torneo no existe
TEST_F(MatchDelegateTest, GetMatch_TournamentNotFound) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(tournamentId)))
        .WillOnce(testing::Return(nullptr));

    auto result = matchDelegate->GetMatch(tournamentId, matchId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// Validar formato invalido de IDs
TEST_F(MatchDelegateTest, GetMatch_InvalidFormat) {
    std::string invalidTournamentId = "invalid!@#";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    auto result = matchDelegate->GetMatch(invalidTournamentId, matchId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// ============================================================================
// Tests de UpdateMatchScore - Reglas de Negocio
// ============================================================================

// Validar actualizacion exitosa del marcador y envio de mensaje
TEST_F(MatchDelegateTest, UpdateMatchScore_Ok) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = 3;
    match.MatchScore().visitorTeamScore = 2;

    auto existingMatch = std::make_shared<domain::Match>();
    existingMatch->Id() = matchId;
    existingMatch->TournamentId() = tournamentId;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(existingMatch));

    EXPECT_CALL(*mockMatchRepository, UpdateMatchScore(matchId, testing::_))
        .Times(1);

    EXPECT_CALL(*mockMessageProducer, SendMessage(testing::_, testing::_))
        .Times(1);

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), matchId);
}

// Validar error cuando el match no existe
TEST_F(MatchDelegateTest, UpdateMatchScore_MatchNotFound) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "990e8400-e29b-41d4-a716-446655440099";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = 3;
    match.MatchScore().visitorTeamScore = 2;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(nullptr));

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// Validar error con formato invalido de IDs
TEST_F(MatchDelegateTest, UpdateMatchScore_InvalidFormat) {
    domain::Match match;
    match.Id() = "invalid!@#";
    match.TournamentId() = "invalid-tournament!@#";
    match.MatchScore().homeTeamScore = 3;
    match.MatchScore().visitorTeamScore = 2;

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// REGLA DE NEGOCIO: Validar que los scores no sean negativos
TEST_F(MatchDelegateTest, UpdateMatchScore_NegativeScore) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = -1; // Score negativo
    match.MatchScore().visitorTeamScore = 2;

    auto existingMatch = std::make_shared<domain::Match>();
    existingMatch->Id() = matchId;
    existingMatch->TournamentId() = tournamentId;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(existingMatch));

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// REGLA DE NEGOCIO: Validar que ambos scores no sean negativos
TEST_F(MatchDelegateTest, UpdateMatchScore_BothScoresNegative) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = -1;
    match.MatchScore().visitorTeamScore = -2;

    auto existingMatch = std::make_shared<domain::Match>();
    existingMatch->Id() = matchId;
    existingMatch->TournamentId() = tournamentId;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(existingMatch));

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// REGLA DE NEGOCIO: No se permiten empates en doble eliminación
TEST_F(MatchDelegateTest, UpdateMatchScore_TieScore) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = 2;
    match.MatchScore().visitorTeamScore = 2;

    auto existingMatch = std::make_shared<domain::Match>();
    existingMatch->Id() = matchId;
    existingMatch->TournamentId() = tournamentId;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(existingMatch));

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::INVALID_FORMAT);
}

// Validar que el mensaje enviado contiene los datos correctos
TEST_F(MatchDelegateTest, UpdateMatchScore_MessageContainsCorrectData) {
    std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
    std::string matchId = "880e8400-e29b-41d4-a716-446655440003";

    domain::Match match;
    match.Id() = matchId;
    match.TournamentId() = tournamentId;
    match.MatchScore().homeTeamScore = 5;
    match.MatchScore().visitorTeamScore = 3;

    auto existingMatch = std::make_shared<domain::Match>();
    existingMatch->Id() = matchId;
    existingMatch->TournamentId() = tournamentId;

    EXPECT_CALL(*mockMatchRepository, FindByTournamentIdAndMatchId(tournamentId, matchId))
        .WillOnce(testing::Return(existingMatch));

    EXPECT_CALL(*mockMatchRepository, UpdateMatchScore(matchId, testing::_))
        .Times(1);

    // Validar que el mensaje JSON contenga los campos correctos
    EXPECT_CALL(*mockMessageProducer, SendMessage(
        testing::AllOf(
            testing::HasSubstr("\"tournamentId\":\"" + tournamentId + "\""),
            testing::HasSubstr("\"matchId\":\"" + matchId + "\""),
            testing::HasSubstr("\"homeTeamScore\":5"),
            testing::HasSubstr("\"visitorTeamScore\":3")
        ),
        testing::_
    )).Times(1);

    auto result = matchDelegate->UpdateMatchScore(match);

    ASSERT_TRUE(result.has_value());
}
