#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>
#include <pqxx/pqxx>

#include "domain/Team.hpp"
#include "delegate/TeamDelegate.hpp"
#include "persistence/repository/IRepository.hpp"
#include "exception/Error.hpp"

// Mock del repositorio
class MockTeamRepository : public IRepository<domain::Team, std::string_view> {
public:
    MOCK_METHOD(std::shared_ptr<domain::Team>, ReadById, (std::string_view id), (override));
    MOCK_METHOD(std::string_view, Create, (const domain::Team& entity), (override));
    MOCK_METHOD(std::string_view, Update, (const domain::Team& entity), (override));
    MOCK_METHOD(void, Delete, (std::string_view id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Team>>, ReadAll, (), (override));
};

class TeamDelegateTest : public ::testing::Test {
protected:
    std::shared_ptr<MockTeamRepository> mockRepository;
    std::shared_ptr<TeamDelegate> teamDelegate;

    void SetUp() override {
        mockRepository = std::make_shared<MockTeamRepository>();
        teamDelegate = std::make_shared<TeamDelegate>(mockRepository);
    }
};

// Tests de CreateTeam

// Validar creacion exitosa: transferencia de valor y retorno de ID generado
TEST_F(TeamDelegateTest, CreateTeam_Id) {
  domain::Team newTeam;
  newTeam.Id = "";
  newTeam.Name = "New Team";
  std::string_view expectedId = "550e8400-e29b-41d4-a716-446655440000";

  EXPECT_CALL(*mockRepository, Create(testing::Field(&domain::Team::Name, "New Team")))
    .WillOnce(testing::Return(expectedId));

  auto result = teamDelegate->CreateTeam(newTeam);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), expectedId);
}

// Validar creacion fallida: error de duplicado usando std::expected
TEST_F(TeamDelegateTest, CreateTeam_Error) {
  domain::Team duplicateTeam;
  duplicateTeam.Id = "";
  duplicateTeam.Name = "Duplicate Team";

  EXPECT_CALL(*mockRepository, Create(testing::Field(&domain::Team::Name, "Duplicate Team")))
    .WillOnce(testing::Throw(pqxx::unique_violation("duplicate key value violates unique constraint", "", "23505")));

  auto result = teamDelegate->CreateTeam(duplicateTeam);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::DUPLICATE);
}

// Tests de GetTeam (busqueda por ID)

// Validar busqueda exitosa: transferencia del ID y retorno del objeto
TEST_F(TeamDelegateTest, GetTeam_Ok) {
  std::string_view testId = "550e8400-e29b-41d4-a716-446655440000";
  auto expectedTeam = std::make_shared<domain::Team>();
  expectedTeam->Id = "550e8400-e29b-41d4-a716-446655440000";
  expectedTeam->Name = "Test Team";

  EXPECT_CALL(*mockRepository, ReadById(testing::StrEq(testId)))
    .WillOnce(testing::Return(expectedTeam));

  auto result = teamDelegate->GetTeam(testId);

  ASSERT_TRUE(result.has_value());
  auto team = result.value();
  EXPECT_EQ(team->Id, "550e8400-e29b-41d4-a716-446655440000");
  EXPECT_EQ(team->Name, "Test Team");
}

// Validar busqueda sin resultado: retorna nullptr
TEST_F(TeamDelegateTest, GetTeam_NotFound) {
  std::string_view nonExistentId = "550e8400-e29b-41d4-a716-446655440001";

  EXPECT_CALL(*mockRepository, ReadById(testing::StrEq(nonExistentId)))
    .WillOnce(testing::Return(nullptr));

  auto result = teamDelegate->GetTeam(nonExistentId);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// Tests de GetAllTeams

// Validar lista con objetos
TEST_F(TeamDelegateTest, GetAllTeams_Ok) {
  std::vector<std::shared_ptr<domain::Team>> teams;
  
  auto team1 = std::make_shared<domain::Team>();
  team1->Id = "550e8400-e29b-41d4-a716-446655440001";
  team1->Name = "Team One";
  
  auto team2 = std::make_shared<domain::Team>();
  team2->Id = "550e8400-e29b-41d4-a716-446655440002";
  team2->Name = "Team Two";
  
  auto team3 = std::make_shared<domain::Team>();
  team3->Id = "550e8400-e29b-41d4-a716-446655440003";
  team3->Name = "Team Three";
  
  teams.push_back(team1);
  teams.push_back(team2);
  teams.push_back(team3);

  EXPECT_CALL(*mockRepository, ReadAll())
    .WillOnce(testing::Return(teams));

  auto result = teamDelegate->GetAllTeams();

  ASSERT_TRUE(result.has_value());
  auto retrievedTeams = result.value();
  ASSERT_EQ(retrievedTeams.size(), 3);
  EXPECT_EQ(retrievedTeams[0]->Id, "550e8400-e29b-41d4-a716-446655440001");
  EXPECT_EQ(retrievedTeams[0]->Name, "Team One");
  EXPECT_EQ(retrievedTeams[1]->Id, "550e8400-e29b-41d4-a716-446655440002");
  EXPECT_EQ(retrievedTeams[1]->Name, "Team Two");
  EXPECT_EQ(retrievedTeams[2]->Id, "550e8400-e29b-41d4-a716-446655440003");
  EXPECT_EQ(retrievedTeams[2]->Name, "Team Three");
}

// Validar lista vacia
TEST_F(TeamDelegateTest, GetAllTeams_Empty) {
  std::vector<std::shared_ptr<domain::Team>> emptyTeams;

  EXPECT_CALL(*mockRepository, ReadAll())
    .WillOnce(testing::Return(emptyTeams));

  auto result = teamDelegate->GetAllTeams();

  ASSERT_TRUE(result.has_value());
  auto retrievedTeams = result.value();
  EXPECT_TRUE(retrievedTeams.empty());
}

// Tests de UpdateTeam

// Validar actualizacion exitosa: busqueda por ID, transferencia de valor, resultado exitoso
TEST_F(TeamDelegateTest, UpdateTeam_Ok) {
  domain::Team updatedTeam;
  updatedTeam.Id = "550e8400-e29b-41d4-a716-446655440000";
  updatedTeam.Name = "Updated Team Name";
  std::string_view expectedResult = "550e8400-e29b-41d4-a716-446655440000";

  EXPECT_CALL(*mockRepository, Update(testing::AllOf(
    testing::Field(&domain::Team::Id, "550e8400-e29b-41d4-a716-446655440000"),
    testing::Field(&domain::Team::Name, "Updated Team Name")
  )))
    .WillOnce(testing::Return(expectedResult));

  auto result = teamDelegate->UpdateTeam(updatedTeam);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), expectedResult);
}

// Validar actualizacion fallida: busqueda por ID sin resultado y retorna error
TEST_F(TeamDelegateTest, UpdateTeam_NotFound) {
  domain::Team nonExistentTeam;
  nonExistentTeam.Id = "550e8400-e29b-41d4-a716-446655440001";
  nonExistentTeam.Name = "Some Team";

  EXPECT_CALL(*mockRepository, Update(testing::Field(&domain::Team::Id, "550e8400-e29b-41d4-a716-446655440001")))
    .WillOnce(testing::Return(std::string_view("")));

  auto result = teamDelegate->UpdateTeam(nonExistentTeam);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

