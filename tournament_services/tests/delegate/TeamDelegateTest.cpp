#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>

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

// ========== Tests para CreateTeam ==========

// Validar creación exitosa: transferencia de valor y retorno de ID generado
TEST_F(TeamDelegateTest, CreateTeam_ValidTeam_ReturnsGeneratedId) {
    // Arrange
    domain::Team newTeam;
    newTeam.Id = "";
    newTeam.Name = "New Team";
    std::string_view expectedId = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_CALL(*mockRepository, Create(testing::Field(&domain::Team::Name, "New Team")))
        .WillOnce(testing::Return(expectedId));

    // Act
    auto result = teamDelegate->CreateTeam(newTeam);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expectedId);
}

// Validar creación fallida: error de duplicado usando std::expected
TEST_F(TeamDelegateTest, CreateTeam_DuplicateName_ReturnsDuplicateError) {
    // Arrange
    domain::Team duplicateTeam;
    duplicateTeam.Id = "";
    duplicateTeam.Name = "Duplicate Team";

    EXPECT_CALL(*mockRepository, Create(testing::Field(&domain::Team::Name, "Duplicate Team")))
        .WillOnce(testing::Throw(pqxx::unique_violation("23505")));

    // Act
    auto result = teamDelegate->CreateTeam(duplicateTeam);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::DUPLICATE);
}

// ========== Tests para GetTeam (búsqueda por ID) ==========

// Validar búsqueda exitosa: transferencia del ID y retorno del objeto
TEST_F(TeamDelegateTest, GetTeam_ValidId_ReturnsTeamObject) {
    // Arrange
    std::string_view testId = "550e8400-e29b-41d4-a716-446655440000";
    auto expectedTeam = std::make_shared<domain::Team>();
    expectedTeam->Id = "550e8400-e29b-41d4-a716-446655440000";
    expectedTeam->Name = "Test Team";

    EXPECT_CALL(*mockRepository, ReadById(testing::StrEq(testId)))
        .WillOnce(testing::Return(expectedTeam));

    // Act
    auto result = teamDelegate->GetTeam(testId);

    // Assert
    ASSERT_TRUE(result.has_value());
    auto team = result.value();
    EXPECT_EQ(team->Id, "550e8400-e29b-41d4-a716-446655440000");
    EXPECT_EQ(team->Name, "Test Team");
}

// Validar búsqueda sin resultado: retorna nullptr
TEST_F(TeamDelegateTest, GetTeam_NonExistentId_ReturnsNotFoundError) {
    // Arrange
    std::string_view nonExistentId = "550e8400-e29b-41d4-a716-446655440001";

    EXPECT_CALL(*mockRepository, ReadById(testing::StrEq(nonExistentId)))
        .WillOnce(testing::Return(nullptr));

    // Act
    auto result = teamDelegate->GetTeam(nonExistentId);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

// ========== Tests para GetAllTeams ==========

// Validar lista con objetos
TEST_F(TeamDelegateTest, GetAllTeams_ReturnsMultipleTeams) {
    // Arrange
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

    // Act
    auto result = teamDelegate->GetAllTeams();

    // Assert
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

// Validar lista vacía
TEST_F(TeamDelegateTest, GetAllTeams_ReturnsEmptyList) {
    // Arrange
    std::vector<std::shared_ptr<domain::Team>> emptyTeams;

    EXPECT_CALL(*mockRepository, ReadAll())
        .WillOnce(testing::Return(emptyTeams));

    // Act
    auto result = teamDelegate->GetAllTeams();

    // Assert
    ASSERT_TRUE(result.has_value());
    auto retrievedTeams = result.value();
    EXPECT_TRUE(retrievedTeams.empty());
}

// ========== Tests para UpdateTeam ==========

// Validar actualización exitosa: búsqueda por ID, transferencia de valor, resultado exitoso
TEST_F(TeamDelegateTest, UpdateTeam_ValidTeam_ReturnsSuccessfully) {
    // Arrange
    domain::Team updatedTeam;
    updatedTeam.Id = "550e8400-e29b-41d4-a716-446655440000";
    updatedTeam.Name = "Updated Team Name";
    std::string_view expectedResult = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_CALL(*mockRepository, Update(testing::AllOf(
        testing::Field(&domain::Team::Id, "550e8400-e29b-41d4-a716-446655440000"),
        testing::Field(&domain::Team::Name, "Updated Team Name")
    )))
        .WillOnce(testing::Return(expectedResult));

    // Act
    auto result = teamDelegate->UpdateTeam(updatedTeam);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expectedResult);
}

// Validar actualización fallida: búsqueda por ID sin resultado y retorna error
TEST_F(TeamDelegateTest, UpdateTeam_NonExistentTeam_ReturnsNotFoundError) {
    // Arrange
    domain::Team nonExistentTeam;
    nonExistentTeam.Id = "550e8400-e29b-41d4-a716-446655440001";
    nonExistentTeam.Name = "Some Team";

    EXPECT_CALL(*mockRepository, Update(testing::Field(&domain::Team::Id, "550e8400-e29b-41d4-a716-446655440001")))
        .WillOnce(testing::Return(std::string_view("")));

    // Act
    auto result = teamDelegate->UpdateTeam(nonExistentTeam);

    // Assert
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

