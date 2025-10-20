#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <crow.h>
#include <nlohmann/json.hpp>
#include <expected>

#include "domain/Team.hpp"
#include "delegate/ITeamDelegate.hpp"
#include "controller/TeamController.hpp"
#include "exception/Error.hpp"

class TeamDelegateMock : public ITeamDelegate {
public:
  MOCK_METHOD((std::expected<std::shared_ptr<domain::Team>, Error>), GetTeam,
              (std::string_view id), (override));
  MOCK_METHOD((std::expected<std::vector<std::shared_ptr<domain::Team>>, Error>), GetAllTeams, (), (override));
  MOCK_METHOD((std::expected<std::string, Error>), CreateTeam, (const domain::Team&), (override));
  MOCK_METHOD((std::expected<std::string, Error>), UpdateTeam, (const domain::Team&), (override));
  MOCK_METHOD((std::expected<void, Error>), DeleteTeam, (std::string_view id), (override));
};

class TeamControllerTest : public ::testing::Test {
protected:
  std::shared_ptr<TeamDelegateMock> teamDelegateMock;
  std::shared_ptr<TeamController> teamController;

  void SetUp() override {
    teamDelegateMock = std::make_shared<TeamDelegateMock>();
    teamController = std::make_shared<TeamController>(teamDelegateMock);
  }
};

// Tests de CreateTeam

// Validacion del JSON y creacion exitosa. Response 201
TEST_F(TeamControllerTest, CreateTeam_Created) {
  domain::Team capturedTeam;
  EXPECT_CALL(*teamDelegateMock, CreateTeam(testing::_))
    .WillOnce(testing::DoAll(testing::SaveArg<0>(&capturedTeam),
                             testing::Return(std::expected<std::string, Error>{std::in_place, "550e8400-e29b-41d4-a716-446655440000"})));

  nlohmann::json teamRequestBody;
  teamRequestBody["name"] = "New Team";
  crow::request teamRequest;
  teamRequest.body = teamRequestBody.dump();

  crow::response response = teamController->createTeam(teamRequest);

  EXPECT_EQ(crow::CREATED, response.code);
  EXPECT_EQ(teamRequestBody.at("name").get<std::string>(), capturedTeam.Name);
  EXPECT_TRUE(capturedTeam.Id.empty());
}

// Validacion del JSON y conflicto en DB. Response 409
TEST_F(TeamControllerTest, CreateTeam_Conflict) {
  EXPECT_CALL(*teamDelegateMock, CreateTeam(testing::_))
    .WillOnce(testing::Return(std::expected<std::string, Error>{std::unexpected(Error::DUPLICATE)}));

  nlohmann::json teamRequestBody;
  teamRequestBody["name"] = "Duplicate Team";
  crow::request teamRequest;
  teamRequest.body = teamRequestBody.dump();

  crow::response response = teamController->createTeam(teamRequest);

  EXPECT_EQ(crow::CONFLICT, response.code);
}

// Tests de GetTeam

// Validar respuesta exitosa y contenido. Response 200
TEST_F(TeamControllerTest, GetTeamById_Ok) {
  std::string teamId = "550e8400-e29b-41d4-a716-446655440000";
  auto expectedTeam = std::make_shared<domain::Team>();
  expectedTeam->Id = teamId;
  expectedTeam->Name = "Team Name";

  EXPECT_CALL(*teamDelegateMock, GetTeam(std::string_view(teamId)))
    .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Team>, Error>{std::in_place, expectedTeam}));

  crow::response response = teamController->getTeam(teamId);
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  EXPECT_EQ(expectedTeam->Id, jsonResponse["id"].get<std::string>());
  EXPECT_EQ(expectedTeam->Name, jsonResponse["name"].get<std::string>());
}

// Validar respuesta NOT_FOUND. Response 404
TEST_F(TeamControllerTest, GetTeamById_NotFound) {
  std::string teamId = "550e8400-e29b-41d4-a716-446655440001";

  EXPECT_CALL(*teamDelegateMock, GetTeam(std::string_view(teamId)))
      .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Team>, Error>{std::unexpected(Error::NOT_FOUND)}));

  crow::response response = teamController->getTeam(teamId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}

// Tests de GetAllTeams

// Validar respuesta exitosa con lista de equipos. Response 200
TEST_F(TeamControllerTest, GetAllTeams_Ok) {
  std::vector<std::shared_ptr<domain::Team>> teams;
  
  auto team1 = std::make_shared<domain::Team>();
  team1->Id = "550e8400-e29b-41d4-a716-446655440001";
  team1->Name = "Team One";
  
  auto team2 = std::make_shared<domain::Team>();
  team2->Id = "550e8400-e29b-41d4-a716-446655440002";
  team2->Name = "Team Two";
  
  teams.push_back(team1);
  teams.push_back(team2);

  EXPECT_CALL(*teamDelegateMock, GetAllTeams())
    .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Team>>, Error>{std::in_place, teams}));

  crow::response response = teamController->getAllTeams();
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  ASSERT_EQ(jsonResponse.size(), teams.size());
  EXPECT_EQ(jsonResponse[0]["id"].get<std::string>(), teams[0]->Id);
  EXPECT_EQ(jsonResponse[0]["name"].get<std::string>(), teams[0]->Name);
  EXPECT_EQ(jsonResponse[1]["id"].get<std::string>(), teams[1]->Id);
  EXPECT_EQ(jsonResponse[1]["name"].get<std::string>(), teams[1]->Name);
}

// Validar respuesta exitosa con lista vacia. Response 200
TEST_F(TeamControllerTest, GetAllTeams_Empty) {
  std::vector<std::shared_ptr<domain::Team>> emptyTeams;

  EXPECT_CALL(*teamDelegateMock, GetAllTeams())
    .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Team>>, Error>{std::in_place, emptyTeams}));

  crow::response response = teamController->getAllTeams();
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  ASSERT_EQ(jsonResponse.size(), 0);
}

// Tests de UpdateTeam

// Validacion del JSON y actualizacion exitosa. Response 200
TEST_F(TeamControllerTest, UpdateTeam_Ok) {
  std::string teamId = "550e8400-e29b-41d4-a716-446655440000";
  domain::Team capturedTeam;
  
  EXPECT_CALL(*teamDelegateMock, UpdateTeam(testing::_))
    .WillOnce(testing::DoAll(testing::SaveArg<0>(&capturedTeam),
                             testing::Return(std::expected<std::string, Error>{std::in_place, teamId})));

  nlohmann::json teamRequestBody;
  teamRequestBody["name"] = "Updated Team";
  crow::request teamRequest;
  teamRequest.body = teamRequestBody.dump();

  crow::response response = teamController->updateTeam(teamRequest, teamId);

  EXPECT_EQ(crow::OK, response.code);
  EXPECT_EQ(teamId, capturedTeam.Id);
  EXPECT_EQ(teamRequestBody.at("name").get<std::string>(), capturedTeam.Name);
}

// Validacion del JSON y equipo no encontrado. Response 404
TEST_F(TeamControllerTest, UpdateTeam_NotFound) {
  std::string teamId = "550e8400-e29b-41d4-a716-446655440001";
  
  EXPECT_CALL(*teamDelegateMock, UpdateTeam(testing::_))
    .WillOnce(testing::Return(std::expected<std::string, Error>{std::unexpected(Error::NOT_FOUND)}));

  nlohmann::json teamRequestBody;
  teamRequestBody["name"] = "Not Found Team";
  crow::request teamRequest;
  teamRequest.body = teamRequestBody.dump();

  crow::response response = teamController->updateTeam(teamRequest, teamId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}