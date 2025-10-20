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

// Tests de CreateTournament

// Validar creacion exitosa: transferencia de valor y retorno de ID generado
TEST_F(TournamentDelegateTest, CreateTournament_Id) {
  domain::Tournament newTournament("Test Tournament");
  std::string expectedId = "550e8400-e29b-41d4-a716-446655440000";

  EXPECT_CALL(*mockRepository, Create(testing::Property(&domain::Tournament::Name, testing::Eq("Test Tournament"))))
    .WillOnce(testing::Return(expectedId));

  auto result = tournamentDelegate->CreateTournament(newTournament);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), expectedId);
}

// Validar creacion fallida: error de duplicado usando std::expected
TEST_F(TournamentDelegateTest, CreateTournament_Error) {
  domain::Tournament duplicateTournament("Duplicate Tournament");

  EXPECT_CALL(*mockRepository, Create(testing::_))
    .WillOnce(testing::Throw(pqxx::unique_violation("duplicate key value violates unique constraint", "", "23505")));

  auto result = tournamentDelegate->CreateTournament(duplicateTournament);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::DUPLICATE);
}

// Tests de GetTournament (busqueda por ID)

// Validar busqueda exitosa: transferencia del ID y retorno del objeto con validacion de valores
TEST_F(TournamentDelegateTest, GetTournament_Ok) {
  std::string testId = "550e8400-e29b-41d4-a716-446655440000";
  auto expectedTournament = std::make_shared<domain::Tournament>("Test Tournament");
  expectedTournament->Id() = testId;

  EXPECT_CALL(*mockRepository, ReadById(testing::Eq(testId)))
    .WillOnce(testing::Return(expectedTournament));

  auto result = tournamentDelegate->GetTournament(testId);

  ASSERT_TRUE(result.has_value());
  auto tournament = result.value();
  EXPECT_EQ(tournament->Id(), testId);
  EXPECT_EQ(tournament->Name(), "Test Tournament");
}

// Validar busqueda sin resultado: retorna nullptr
TEST_F(TournamentDelegateTest, GetTournament_NotFound) {
  std::string nonExistentId = "550e8400-e29b-41d4-a716-446655440001";

  EXPECT_CALL(*mockRepository, ReadById(testing::Eq(nonExistentId)))
    .WillOnce(testing::Return(nullptr));

  auto result = tournamentDelegate->GetTournament(nonExistentId);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// Tests de ReadAll (GetAllTournaments)

// Validar lista con objetos
TEST_F(TournamentDelegateTest, ReadAll_Ok) {
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

  auto result = tournamentDelegate->ReadAll();

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

// Validar lista vacia
TEST_F(TournamentDelegateTest, ReadAll_Empty) {
  std::vector<std::shared_ptr<domain::Tournament>> emptyTournaments;

  EXPECT_CALL(*mockRepository, ReadAll())
    .WillOnce(testing::Return(emptyTournaments));

  auto result = tournamentDelegate->ReadAll();

  ASSERT_TRUE(result.has_value());
  auto retrievedTournaments = result.value();
  EXPECT_TRUE(retrievedTournaments.empty());
}

// Tests de UpdateTournament

// Validar actualizacion exitosa: busqueda por ID, transferencia de valor, resultado exitoso
TEST_F(TournamentDelegateTest, UpdateTournament_Ok) {
  domain::Tournament updatedTournament("Updated Tournament Name");
  updatedTournament.Id() = "550e8400-e29b-41d4-a716-446655440000";
  std::string expectedResult = "550e8400-e29b-41d4-a716-446655440000";

  EXPECT_CALL(*mockRepository, Update(testing::AllOf(
    testing::Property(&domain::Tournament::Id, testing::Eq("550e8400-e29b-41d4-a716-446655440000")),
    testing::Property(&domain::Tournament::Name, testing::Eq("Updated Tournament Name"))
  )))
    .WillOnce(testing::Return(expectedResult));

  auto result = tournamentDelegate->UpdateTournament(updatedTournament);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), expectedResult);
}

// Validar actualizacion fallida: busqueda por ID sin resultado y retorna error
TEST_F(TournamentDelegateTest, UpdateTournament_NotFound) {
  domain::Tournament nonExistentTournament("Some Tournament");
  nonExistentTournament.Id() = "550e8400-e29b-41d4-a716-446655440001";

  EXPECT_CALL(*mockRepository, Update(testing::_))
    .WillOnce(testing::Return(std::string("")));

  auto result = tournamentDelegate->UpdateTournament(nonExistentTournament);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::NOT_FOUND);
}