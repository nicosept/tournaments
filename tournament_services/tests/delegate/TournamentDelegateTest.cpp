#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>
#include <pqxx/pqxx>

#include "domain/Tournament.hpp"
#include "delegate/TournamentDelegate.hpp"
#include "persistence/repository/IRepository.hpp"
#include "exception/Error.hpp"

// Mock del repositorio
class MockTournamentRepository : public IRepository<domain::Tournament, std::string> {
public:
    MOCK_METHOD(std::shared_ptr<domain::Tournament>, ReadById, (std::string id), (override));
    MOCK_METHOD(std::string, Create, (const domain::Tournament& entity), (override));
    MOCK_METHOD(std::string, Update, (const domain::Tournament& entity), (override));
    MOCK_METHOD(void, Delete, (std::string id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Tournament>>, ReadAll, (), (override));
};

class TournamentDelegateTest : public ::testing::Test {
protected:
    std::shared_ptr<MockTournamentRepository> mockRepository;
    std::shared_ptr<TournamentDelegate> tournamentDelegate;

    void SetUp() override {
        mockRepository = std::make_shared<MockTournamentRepository>();
        tournamentDelegate = std::make_shared<TournamentDelegate>(mockRepository);
    }
};

// ========== Tests para CreateTournament ==========

// Validar creación exitosa: transferencia de valor y retorno de ID generado
TEST_F(TournamentDelegateTest, CreateTournament_ValidTournament_ReturnsGeneratedId) {
    // Arrange
    domain::Tournament newTournament("Test Tournament");
    std::string expectedId = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_CALL(*mockRepository, Create(testing::Property(&domain::Tournament::Name, testing::Eq("Test Tournament"))))
        .WillOnce(testing::Return(expectedId));

    // Act
    auto result = tournamentDelegate->CreateTournament(newTournament);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expectedId);
}

// Validar creación fallida: error de duplicado usando std::expected
TEST_F(TournamentDelegateTest, CreateTournament_DuplicateName_ReturnsDuplicateError) {
    // Arrange
    domain::Tournament duplicateTournament("Duplicate Tournament");

    EXPECT_CALL(*mockRepository, Create(testing::_))
        .WillOnce(testing::Throw(pqxx::unique_violation("duplicate key value violates unique constraint", "", "23505")));

    // Act
    auto result = tournamentDelegate->CreateTournament(duplicateTournament);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::DUPLICATE);
}

// ========== Tests para GetTournament (búsqueda por ID) ==========

// Validar búsqueda exitosa: transferencia del ID y retorno del objeto con validación de valores
TEST_F(TournamentDelegateTest, GetTournament_ValidId_ReturnsTournamentObject) {
    // Arrange
    std::string testId = "550e8400-e29b-41d4-a716-446655440000";
    auto expectedTournament = std::make_shared<domain::Tournament>("Test Tournament");
    expectedTournament->Id() = testId;

    EXPECT_CALL(*mockRepository, ReadById(testing::Eq(testId)))
        .WillOnce(testing::Return(expectedTournament));

    // Act
    auto result = tournamentDelegate->GetTournament(testId);

    // Assert
    ASSERT_TRUE(result.has_value());
    auto tournament = result.value();
    EXPECT_EQ(tournament->Id(), testId);
    EXPECT_EQ(tournament->Name(), "Test Tournament");
}

// Validar búsqueda sin resultado: retorna nullptr
TEST_F(TournamentDelegateTest, GetTournament_NonExistentId_ReturnsNotFoundError) {
    // Arrange
    std::string nonExistentId = "550e8400-e29b-41d4-a716-446655440001";

    EXPECT_CALL(*mockRepository, ReadById(testing::Eq(nonExistentId)))
        .WillOnce(testing::Return(nullptr));

    // Act
    auto result = tournamentDelegate->GetTournament(nonExistentId);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// ========== Tests para ReadAll (GetAllTournaments) ==========

// Validar lista con objetos
TEST_F(TournamentDelegateTest, ReadAll_ReturnsMultipleTournaments) {
    // Arrange
    std::vector<std::shared_ptr<domain::Tournament>> tournaments;
    
    auto tournament1 = std::make_shared<domain::Tournament>("Tournament One");
    tournament1->Id() = "550e8400-e29b-41d4-a716-446655440001";
    
    auto tournament2 = std::make_shared<domain::Tournament>("Tournament Two");
    tournament2->Id() = "550e8400-e29b-41d4-a716-446655440002";
    
    auto tournament3 = std::make_shared<domain::Tournament>("Tournament Three");
    tournament3->Id() = "550e8400-e29b-41d4-a716-446655440003";
    
    tournaments.push_back(tournament1);
    tournaments.push_back(tournament2);
    tournaments.push_back(tournament3);

    EXPECT_CALL(*mockRepository, ReadAll())
        .WillOnce(testing::Return(tournaments));

    // Act
    auto result = tournamentDelegate->ReadAll();

    // Assert
    ASSERT_TRUE(result.has_value());
    auto retrievedTournaments = result.value();
    ASSERT_EQ(retrievedTournaments.size(), 3);
    EXPECT_EQ(retrievedTournaments[0]->Id(), "550e8400-e29b-41d4-a716-446655440001");
    EXPECT_EQ(retrievedTournaments[0]->Name(), "Tournament One");
    EXPECT_EQ(retrievedTournaments[1]->Id(), "550e8400-e29b-41d4-a716-446655440002");
    EXPECT_EQ(retrievedTournaments[1]->Name(), "Tournament Two");
    EXPECT_EQ(retrievedTournaments[2]->Id(), "550e8400-e29b-41d4-a716-446655440003");
    EXPECT_EQ(retrievedTournaments[2]->Name(), "Tournament Three");
}

// Validar lista vacía
TEST_F(TournamentDelegateTest, ReadAll_ReturnsEmptyList) {
    // Arrange
    std::vector<std::shared_ptr<domain::Tournament>> emptyTournaments;

    EXPECT_CALL(*mockRepository, ReadAll())
        .WillOnce(testing::Return(emptyTournaments));

    // Act
    auto result = tournamentDelegate->ReadAll();

    // Assert
    ASSERT_TRUE(result.has_value());
    auto retrievedTournaments = result.value();
    EXPECT_TRUE(retrievedTournaments.empty());
}

// ========== Tests para UpdateTournament ==========

// Validar actualización exitosa: búsqueda por ID, transferencia de valor, resultado exitoso
TEST_F(TournamentDelegateTest, UpdateTournament_ValidTournament_ReturnsSuccessfully) {
    // Arrange
    domain::Tournament updatedTournament("Updated Tournament Name");
    updatedTournament.Id() = "550e8400-e29b-41d4-a716-446655440000";
    std::string expectedResult = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_CALL(*mockRepository, Update(testing::AllOf(
        testing::Property(&domain::Tournament::Id, testing::Eq("550e8400-e29b-41d4-a716-446655440000")),
        testing::Property(&domain::Tournament::Name, testing::Eq("Updated Tournament Name"))
    )))
        .WillOnce(testing::Return(expectedResult));

    // Act
    auto result = tournamentDelegate->UpdateTournament(updatedTournament);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expectedResult);
}

// Validar actualización fallida: búsqueda por ID sin resultado y retorna error
TEST_F(TournamentDelegateTest, UpdateTournament_NonExistentTournament_ReturnsNotFoundError) {
    // Arrange
    domain::Tournament nonExistentTournament("Some Tournament");
    nonExistentTournament.Id() = "550e8400-e29b-41d4-a716-446655440001";

    EXPECT_CALL(*mockRepository, Update(testing::_))
        .WillOnce(testing::Return(std::string("")));

    // Act
    auto result = tournamentDelegate->UpdateTournament(nonExistentTournament);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}