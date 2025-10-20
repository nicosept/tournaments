#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "domain/Team.hpp"
#include "delegate/TeamDelegate.hpp"
#include "persistence/repository/IRepository.hpp"
#include "exception/Duplicate.hpp"
#include "exception/NotFound.hpp"
#include "exception/InvalidFormat.hpp"
#include "exception/Error.hpp"

//Antes se usaban los delegate con try catch y para poder usarlos con los expected-unexpected se tuvo que 
//quitar lo de throw ya que los expected-unexpected ya manejan los errores de otra forma y no con excepciones

//Esta clase lo que hace es que crea un mock del repositorio de Team para poder usarlo en las pruebas del delegate
class MockTeamRepository : public IRepository<domain::Team, std::string_view> {
public:
    MOCK_METHOD(std::shared_ptr<domain::Team>, ReadById, (std::string_view id), (override));
    MOCK_METHOD(std::string_view, Create, (const domain::Team& entity), (override));
    MOCK_METHOD(std::string_view, Update, (const domain::Team& entity), (override));
    MOCK_METHOD(void, Delete, (std::string_view id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Team>>, ReadAll, (), (override));
};

//Este es el fixture y hace que el mock y el delegate estén disponibles para cada test ya que lo que hace es que crea una instancia de cada uno antes de cada prueba
class TeamDelegateTest : public ::testing::Test {
protected:
    std::shared_ptr<MockTeamRepository> mockRepository;
    std::shared_ptr<TeamDelegate> teamDelegate;

    void SetUp() override {
        mockRepository = std::make_shared<MockTeamRepository>();
        teamDelegate = std::make_shared<TeamDelegate>(mockRepository);
    }
};

//Esto es de google mock y sirve para no tener que escribir testing:: todo el tiempo:p
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::StrEq;

//Estos son los tests para createtEAM

TEST_F(TeamDelegateTest, CreateTeam_ValidValue_ReturnsExpectedId) {
    domain::Team newTeam{"", "New Team"};
    std::string_view expectedId = "generated-id";

    EXPECT_CALL(*mockRepository, Create(Field(&domain::Team::Name, "New Team")))
        .WillOnce(testing::Return(expectedId));

    auto result = teamDelegate->CreateTeam(newTeam);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedId);
}

TEST_F(TeamDelegateTest, CreateTeam_DuplicateName_ReturnsDuplicateError) {
    domain::Team duplicateTeam{"", "Duplicate Team"};

    EXPECT_CALL(*mockRepository, Create(Field(&domain::Team::Name, "Duplicate Team")))
        .WillOnce(testing::Throw(DuplicateException("A team with the same name already exists.")));

    auto result = teamDelegate->CreateTeam(duplicateTeam);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::DUPLICATE);
}

//Aqui se hacen los tests para getteam con id

TEST_F(TeamDelegateTest, GetTeam_ValidId_ReturnsTeamObject) {
    auto expectedTeam = std::make_shared<domain::Team>(domain::Team{"test-id", "Test Team"});
    
    EXPECT_CALL(*mockRepository, ReadById(StrEq("test-id")))
        .WillOnce(testing::Return(expectedTeam));

    auto result = teamDelegate->GetTeam("test-id");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->Id, "test-id");
    EXPECT_EQ(result->Name, "Test Team");
}

TEST_F(TeamDelegateTest, GetTeam_NonExistentId_ReturnsNullptr) {
    EXPECT_CALL(*mockRepository, ReadById(StrEq("non-existent-id")))
        .WillOnce(testing::Return(nullptr));

    auto result = teamDelegate->GetTeam("non-existent-id");

    EXPECT_EQ(result, nullptr);
}

//Estos son los tests para getALlTeams

TEST_F(TeamDelegateTest, GetAllTeams_ReturnsMultipleTeams) {
    std::vector<std::shared_ptr<domain::Team>> teams = {
        std::make_shared<domain::Team>(domain::Team{"id1", "Team One"}),
        std::make_shared<domain::Team>(domain::Team{"id2", "Team Two"}),
        std::make_shared<domain::Team>(domain::Team{"id3", "Team Three"})
    };

    EXPECT_CALL(*mockRepository, ReadAll())
        .WillOnce(testing::Return(teams));

    auto result = teamDelegate->GetAllTeams();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0]->Id, "id1");
    EXPECT_EQ(result[1]->Id, "id2");
    EXPECT_EQ(result[2]->Id, "id3");
}

TEST_F(TeamDelegateTest, GetAllTeams_ReturnsEmptyList) {
    std::vector<std::shared_ptr<domain::Team>> emptyTeams;

    EXPECT_CALL(*mockRepository, ReadAll())
        .WillOnce(testing::Return(emptyTeams));

    auto result = teamDelegate->GetAllTeams();

    EXPECT_TRUE(result.empty());
}

//Aqui estan los tests para updDateTeaam


TEST_F(TeamDelegateTest, UpdateTeam_ReadsThenUpdates_Success) {
    domain::Team updatedTeam{"existing-id", "Updated Team Name"};
    auto existing = std::make_shared<domain::Team>(domain::Team{"existing-id", "Old Name"});

    EXPECT_CALL(*mockRepository, ReadById(StrEq("existing-id")))
        .WillOnce(testing::Return(existing));

    EXPECT_CALL(*mockRepository, Update(AllOf(
        Field(&domain::Team::Id, "existing-id"),
        Field(&domain::Team::Name, "Updated Team Name")
    )))
        .WillOnce(testing::Return(std::string_view{"existing-id"}));

    auto result = teamDelegate->UpdateTeam(updatedTeam);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::string_view{"existing-id"});
}

TEST_F(TeamDelegateTest, UpdateTeam_ReadNotFound_ReturnsUnexpectedNotFound) {
    domain::Team input{"missing-id", "Whatever"};

    EXPECT_CALL(*mockRepository, ReadById(StrEq("missing-id")))
        .WillOnce(testing::Return(nullptr));

    // No debe intentar Update si no existe
    EXPECT_CALL(*mockRepository, Update(_)).Times(0);

    auto result = teamDelegate->UpdateTeam(input);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::NOT_FOUND);
}

// Estos tests son extras por si falla algo con un throw
TEST_F(TeamDelegateTest, UpdateTeam_DirectUpdateNotFound_ReturnsNotFoundError) {
    domain::Team nonExistentTeam{"non-existent-id", "Some Team"};

    EXPECT_CALL(*mockRepository, ReadById(StrEq("non-existent-id")))
        .WillOnce(testing::Return(std::shared_ptr<domain::Team>{new domain::Team{"non-existent-id", "Prev"}}));

    EXPECT_CALL(*mockRepository, Update(Field(&domain::Team::Id, "non-existent-id")))
        .WillOnce(testing::Throw(NotFoundException("Team not found for update.")));

    auto result = teamDelegate->UpdateTeam(nonExistentTeam);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::NOT_FOUND);
}

TEST_F(TeamDelegateTest, UpdateTeam_InvalidFormat_ReturnsInvalidFormatError) {
    domain::Team invalidTeam{"invalid-format-id", "Team Name"};

    EXPECT_CALL(*mockRepository, ReadById(StrEq("invalid-format-id")))
        .WillOnce(testing::Return(std::make_shared<domain::Team>(domain::Team{"invalid-format-id", "Prev"})));

    EXPECT_CALL(*mockRepository, Update(Field(&domain::Team::Id, "invalid-format-id")))
        .WillOnce(testing::Throw(InvalidFormatException("Invalid ID format.")));

    auto result = teamDelegate->UpdateTeam(invalidTeam);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::INVALID_FORMAT);
}

//Estos son los tests para deleteTEam
TEST_F(TeamDelegateTest, DeleteTeam_Success_ReturnsVoidExpected) {
    EXPECT_CALL(*mockRepository, Delete(StrEq("team-to-delete")))
        .Times(1);

    auto result = teamDelegate->DeleteTeam("team-to-delete");
    EXPECT_TRUE(result.has_value());
}

TEST_F(TeamDelegateTest, DeleteTeam_NotFound_ReturnsNotFoundError) {
    EXPECT_CALL(*mockRepository, Delete(StrEq("non-existent-id")))
        .WillOnce(testing::Throw(NotFoundException("Team not found for deletion.")));

    auto result = teamDelegate->DeleteTeam("non-existent-id");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::NOT_FOUND);
}

TEST_F(TeamDelegateTest, DeleteTeam_InvalidFormat_ReturnsInvalidFormatError) {
    EXPECT_CALL(*mockRepository, Delete(StrEq("invalid-id")))
        .WillOnce(testing::Throw(InvalidFormatException("Invalid ID format.")));

    auto result = teamDelegate->DeleteTeam("invalid-id");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, exception::ErrorCode::INVALID_FORMAT);