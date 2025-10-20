#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <crow.h>
#include <nlohmann/json.hpp>

#include "domain/Group.hpp"
#include "domain/Team.hpp"
#include "domain/Utilities.hpp"
#include "delegate/IGroupDelegate.hpp"
#include "controller/GroupController.hpp"
#include "exception/Duplicate.hpp"
#include "exception/NotFound.hpp"
#include "exception/InvalidFormat.hpp"
#include "exception/Error.hpp"

class GroupDelegateMock : public IGroupDelegate {
    public:
    MOCK_METHOD((std::expected<std::shared_ptr<domain::Group>, Error>), GetGroup, (const std::string_view& tournamentId, const std::string_view& groupId), (override));
    MOCK_METHOD((std::expected<std::string, Error>), CreateGroup, (const std::string_view& tournamentId, const domain::Group& group), (override));
    MOCK_METHOD((std::expected<std::vector<std::shared_ptr<domain::Group>>, Error>), GetGroups, (const std::string_view& tournamentId), (override));
    MOCK_METHOD((std::expected<void, Error>), UpdateGroup, (const std::string_view& tournamentId, const domain::Group& group, const std::string_view& groupId), (override)); 
    MOCK_METHOD((std::expected<void, Error>), RemoveGroup, (const std::string_view& tournamentId, const std::string_view& groupId), (override));
    MOCK_METHOD((std::expected<void, Error>), UpdateTeams, (const std::string_view& tournamentId, const std::string_view& groupId, const std::vector<domain::Team>& teams), (override));
};

class GroupControllerTest : public ::testing::Test {
    protected:
    std::shared_ptr<GroupDelegateMock> groupDelegateMock;
    std::shared_ptr<GroupController> groupController;

    void SetUp() override {
        groupDelegateMock = std::make_shared<GroupDelegateMock>();
        groupController = std::make_shared<GroupController>(groupDelegateMock);
    }
};

// Tests de CreateGroup

// Validar transformacion de JSON al objeto de dominio Group y validar que el valor que se le transfiera a GroupDelegate es el esperado. Validar que la respuesta de esta funcion sea HTTP 201
TEST_F(GroupControllerTest, CreateGroup_Created) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string expectedGroupId = "87654321-4321-4321-4321-123456789012";
    
    nlohmann::json requestJson = {
        {"name", "Test Group"},
        {"teams", nlohmann::json::array({
            {{"id", "team-id-1"}, {"name", "Team One"}},
            {{"id", "team-id-2"}, {"name", "Team Two"}}
        })}
    };
    
    EXPECT_CALL(*groupDelegateMock, CreateGroup(
        testing::Eq(tournamentId),
        testing::AllOf(
            testing::Property(&domain::Group::Name, testing::Eq("Test Group")),
            testing::Property(&domain::Group::Teams, testing::AllOf(
                testing::SizeIs(2),
                testing::Contains(testing::AllOf(
                    testing::Field(&domain::Team::Id, testing::Eq("team-id-1")),
                    testing::Field(&domain::Team::Name, testing::Eq("Team One"))
                )),
                testing::Contains(testing::AllOf(
                    testing::Field(&domain::Team::Id, testing::Eq("team-id-2")),
                    testing::Field(&domain::Team::Name, testing::Eq("Team Two"))
                ))
            ))
        )
    )).WillOnce(testing::Return(std::expected<std::string, Error>{expectedGroupId}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->CreateGroup(request, tournamentId);
    
    EXPECT_EQ(crow::CREATED, response.code);
    EXPECT_EQ(expectedGroupId, response.get_header_value("location"));
}

// Validar transformacion de JSON al objeto de dominio Group y validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular error de insercion en la base de datos y validar la respuesta HTTP 409
TEST_F(GroupControllerTest, CreateGroup_Conflict) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    
    nlohmann::json requestJson = {
        {"name", "Duplicate Group"}
    };
    
    EXPECT_CALL(*groupDelegateMock, CreateGroup(
        testing::Eq(tournamentId),
        testing::Property(&domain::Group::Name, testing::Eq("Duplicate Group"))
    )).WillOnce(testing::Return(std::expected<std::string, Error>{std::unexpected(Error::DUPLICATE)}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->CreateGroup(request, tournamentId);
    
    EXPECT_EQ(crow::CONFLICT, response.code);
    EXPECT_EQ("Error", response.body);
}

// Tests de GetGroup

// Validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular el resultado con un objeto y validar la respuesta HTTP 200
TEST_F(GroupControllerTest, GetGroup_Ok) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789012";
    
    std::shared_ptr<domain::Group> expectedGroup = std::make_shared<domain::Group>("Test Group", groupId);
    expectedGroup->TournamentId() = tournamentId;
    expectedGroup->Teams().push_back(domain::Team{"team-id-1", "Team One"});
    expectedGroup->Teams().push_back(domain::Team{"team-id-2", "Team Two"});
    
    EXPECT_CALL(*groupDelegateMock, GetGroup(
        testing::Eq(tournamentId),
        testing::Eq(groupId)
    )).WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Group>, Error>{expectedGroup}));
    
    crow::response response = groupController->GetGroup(tournamentId, groupId);
    
    EXPECT_EQ(crow::OK, response.code);
    EXPECT_EQ("application/json", response.get_header_value("content-type"));
    
    auto jsonResponse = crow::json::load(response.body);
    EXPECT_EQ(expectedGroup->Id(), jsonResponse["id"]);
    EXPECT_EQ(expectedGroup->Name(), jsonResponse["name"]);
    EXPECT_EQ(expectedGroup->TournamentId(), jsonResponse["tournamentId"]);
    ASSERT_EQ(jsonResponse["teams"].size(), 2);
    EXPECT_EQ("team-id-1", jsonResponse["teams"][0]["id"]);
    EXPECT_EQ("Team One", jsonResponse["teams"][0]["name"]);
}

// Validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular el resultado nulo y validar la respuesta HTTP 404
TEST_F(GroupControllerTest, GetGroup_NotFound) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789999";
    
    EXPECT_CALL(*groupDelegateMock, GetGroup(
        testing::Eq(tournamentId),
        testing::Eq(groupId)
    )).WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Group>, Error>{std::unexpected(Error::NOT_FOUND)}));
    
    crow::response response = groupController->GetGroup(tournamentId, groupId);
    
    EXPECT_EQ(crow::NOT_FOUND, response.code);
    EXPECT_EQ("Error", response.body);
}

// Tests de UpdateGroup

// Validar transformacion de JSON al objeto de dominio Group y validar que el valor que se le transfiera a GroupDelegate es el esperado. Validar que la respuesta de esta funcion sea HTTP 204
TEST_F(GroupControllerTest, UpdateGroup_NoContent) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789012";
    
    nlohmann::json requestJson = {
        {"name", "Updated Group Name"},
        {"teams", nlohmann::json::array({
            {{"id", "team-id-3"}, {"name", "Team Three"}}
        })}
    };
    
    EXPECT_CALL(*groupDelegateMock, UpdateGroup(
        testing::Eq(tournamentId),
        testing::AllOf(
            testing::Property(&domain::Group::Name, testing::Eq("Updated Group Name")),
            testing::Property(&domain::Group::Teams, testing::AllOf(
                testing::SizeIs(1),
                testing::Contains(testing::AllOf(
                    testing::Field(&domain::Team::Id, testing::Eq("team-id-3")),
                    testing::Field(&domain::Team::Name, testing::Eq("Team Three"))
                ))
            ))
        ),
        testing::Eq(groupId)
    )).WillOnce(testing::Return(std::expected<void, Error>{}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->UpdateGroup(request, tournamentId, groupId);
    
    EXPECT_EQ(crow::NO_CONTENT, response.code);
}

// Validar transformacion de JSON al objeto de dominio Group y validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular ID no encontrado en el Delegate y validar que la respuesta de esta funcion sea HTTP 404
TEST_F(GroupControllerTest, UpdateGroup_NotFound) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789999";
    
    nlohmann::json requestJson = {
        {"name", "Non-existent Group"}
    };
    
    EXPECT_CALL(*groupDelegateMock, UpdateGroup(
        testing::Eq(tournamentId),
        testing::Property(&domain::Group::Name, testing::Eq("Non-existent Group")),
        testing::Eq(groupId)
    )).WillOnce(testing::Return(std::expected<void, Error>{std::unexpected(Error::NOT_FOUND)}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->UpdateGroup(request, tournamentId, groupId);
  
    EXPECT_EQ(crow::NOT_FOUND, response.code);
    EXPECT_EQ("Error", response.body);
}

// Tests de AddTeams

// Validar transformacion de JSON al objeto de dominio Team y validar que el valor que se le transfiera a GroupDelegate es el esperado. Validar que la respuesta de esta funcion sea HTTP 204
TEST_F(GroupControllerTest, AddTeams_NoContent) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789012";
    
    nlohmann::json requestJson = nlohmann::json::array({
        {{"id", "team-id-1"}, {"name", "Team One"}},
        {{"id", "team-id-2"}, {"name", "Team Two"}}
    });
    
    EXPECT_CALL(*groupDelegateMock, UpdateTeams(
        testing::Eq(tournamentId),
        testing::Eq(groupId),
        testing::AllOf(
            testing::SizeIs(2),
            testing::Contains(testing::AllOf(
                testing::Field(&domain::Team::Id, testing::Eq("team-id-1")),
                testing::Field(&domain::Team::Name, testing::Eq("Team One"))
            )),
            testing::Contains(testing::AllOf(
                testing::Field(&domain::Team::Id, testing::Eq("team-id-2")),
                testing::Field(&domain::Team::Name, testing::Eq("Team Two"))
            ))
        )
    )).WillOnce(testing::Return(std::expected<void, Error>{}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->AddTeams(request, tournamentId, groupId);
    
    EXPECT_EQ(crow::NO_CONTENT, response.code);
}

// Validar transformacion de JSON al objeto de dominio Team y validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular que el equipo no exista y el resultado sea HTTP 422
TEST_F(GroupControllerTest, AddTeams_UnprocessableEntity) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789012";
    
    nlohmann::json requestJson = nlohmann::json::array({
        {{"id", "non-existent-team"}, {"name", "Non Existent Team"}}
    });
    
    EXPECT_CALL(*groupDelegateMock, UpdateTeams(
        testing::Eq(tournamentId),
        testing::Eq(groupId),
        testing::AllOf(
            testing::SizeIs(1),
            testing::Contains(testing::AllOf(
                testing::Field(&domain::Team::Id, testing::Eq("non-existent-team")),
                testing::Field(&domain::Team::Name, testing::Eq("Non Existent Team"))
            ))
        )
    )).WillOnce(testing::Return(std::expected<void, Error>{std::unexpected(Error::UNPROCESSABLE_ENTITY)}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->AddTeams(request, tournamentId, groupId);
    
    EXPECT_EQ(422, response.code);
    EXPECT_EQ("Error", response.body);
}

// Validar transformacion de JSON al objeto de dominio Team y validar que el valor que se le transfiera a GroupDelegate es el esperado. Simular que el grupo este lleno el resultado sea HTTP 422
TEST_F(GroupControllerTest, AddTeams_GroupFull) {
    std::string tournamentId = "12345678-1234-1234-1234-123456789abc";
    std::string groupId = "87654321-4321-4321-4321-123456789012";
    
    nlohmann::json requestJson = nlohmann::json::array({
        {{"id", "team-id-5"}, {"name", "Team Five"}}
    });
    
    EXPECT_CALL(*groupDelegateMock, UpdateTeams(
        testing::Eq(tournamentId),
        testing::Eq(groupId),
        testing::AllOf(
            testing::SizeIs(1),
            testing::Contains(testing::AllOf(
                testing::Field(&domain::Team::Id, testing::Eq("team-id-5")),
                testing::Field(&domain::Team::Name, testing::Eq("Team Five"))
            ))
        )
    )).WillOnce(testing::Return(std::expected<void, Error>{std::unexpected(Error::UNPROCESSABLE_ENTITY)}));
    
    crow::request request;
    request.body = requestJson.dump();
    
    crow::response response = groupController->AddTeams(request, tournamentId, groupId);
    
    EXPECT_EQ(422, response.code);
    EXPECT_EQ("Error", response.body);
}