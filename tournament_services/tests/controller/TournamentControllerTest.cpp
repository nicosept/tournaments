#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <crow.h>
#include <nlohmann/json.hpp>
#include "domain/Tournament.hpp"
#include "controller/TournamentController.hpp"
#include "delegate/ITournamentDelegate.hpp"

class TournamentDelegateMock : public ITournamentDelegate {
public:
  MOCK_METHOD((std::expected<std::shared_ptr<domain::Tournament>, Error>), GetTournament,
              (std::string_view id), (override));
  MOCK_METHOD((std::expected<std::vector<std::shared_ptr<domain::Tournament>>, Error>), ReadAll, (), (override));
  MOCK_METHOD((std::expected<std::string, Error>), CreateTournament, (const domain::Tournament&), (override));
  MOCK_METHOD((std::expected<std::string, Error>), UpdateTournament, (const domain::Tournament&), (override));
  MOCK_METHOD((std::expected<void, Error>), DeleteTournament, (std::string_view id), (override));
};

class TournamentControllerTest : public ::testing::Test {
protected:
  std::shared_ptr<TournamentDelegateMock> tournamentDelegateMock;
  std::shared_ptr<TournamentController> tournamentController;

  void SetUp() override {
    tournamentDelegateMock = std::make_shared<TournamentDelegateMock>();
    tournamentController = std::make_shared<TournamentController>(tournamentDelegateMock);
  }
};

// Tests de CreateTournament

// Validacion del JSON y creacion exitosa. Response 201
TEST_F(TournamentControllerTest, CreateTournament_Created) {
  nlohmann::json jsonBody;
  jsonBody["name"] = "Test Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, CreateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>("tournament-id-123")));

  auto response = tournamentController->CreateTournament(request);

  EXPECT_EQ(response.code, crow::CREATED);
  EXPECT_EQ(response.get_header_value("Location"), "tournament-id-123");
}

// Validacion del JSON y conflicto en DB. Response 409
TEST_F(TournamentControllerTest, CreateTournament_Conflict) {
  nlohmann::json jsonBody;
  jsonBody["name"] = "Test Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, CreateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>(std::unexpected(Error::DUPLICATE))));

  auto response = tournamentController->CreateTournament(request);

  EXPECT_EQ(response.code, crow::CONFLICT);
}

// Tests de GetTournament

// Validar respuesta exitosa y contenido completo. Response 200
TEST_F(TournamentControllerTest, GetTournamentById_Ok) {
  std::string tournamentId = "tournament-123";
  auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
  tournament->Id() = tournamentId;

  EXPECT_CALL(*tournamentDelegateMock, GetTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Tournament>, Error>(tournament)));

  auto response = tournamentController->getTournament(tournamentId);

  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse["id"], tournamentId);
}

// Validar respuesta NOT_FOUND. Response 404
TEST_F(TournamentControllerTest, GetTournamentById_NotFound) {
  std::string tournamentId = "non-existent-id";

  EXPECT_CALL(*tournamentDelegateMock, GetTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Tournament>, Error>(std::unexpected(Error::NOT_FOUND))));

  auto response = tournamentController->getTournament(tournamentId);

  EXPECT_EQ(response.code, crow::NOT_FOUND);
}

// Tests de GetAllTournaments

// Validar respuesta exitosa con lista de torneos. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_Ok) {
  auto tournament1 = std::make_shared<domain::Tournament>("Tournament 1");
  tournament1->Id() = "tournament-1";
  auto tournament2 = std::make_shared<domain::Tournament>("Tournament 2");
  tournament2->Id() = "tournament-2";
  std::vector<std::shared_ptr<domain::Tournament>> tournaments = {tournament1, tournament2};

  EXPECT_CALL(*tournamentDelegateMock, ReadAll())
      .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Tournament>>, Error>(tournaments)));

  auto response = tournamentController->ReadAll();

  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse.size(), 2);
}

// Validar respuesta exitosa con lista vacia. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_Empty) {
  std::vector<std::shared_ptr<domain::Tournament>> emptyTournaments;

  EXPECT_CALL(*tournamentDelegateMock, ReadAll())
      .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Tournament>>, Error>(emptyTournaments)));

  auto response = tournamentController->ReadAll();

  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse.size(), 0);
}

// Tests de UpdateTournament

// Validacion del JSON y actualizacion exitosa. Response 204
TEST_F(TournamentControllerTest, UpdateTournament_NoContent) {
  std::string tournamentId = "tournament-123";
  nlohmann::json jsonBody;
  jsonBody["name"] = "Updated Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, UpdateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>("")));

  auto response = tournamentController->updateTournament(request, tournamentId);

  EXPECT_EQ(response.code, crow::NO_CONTENT);
}

// Validacion del JSON y torneo no encontrado. Response 404
TEST_F(TournamentControllerTest, UpdateTournament_NotFound) {
  std::string tournamentId = "non-existent-id";
  nlohmann::json jsonBody;
  jsonBody["name"] = "Updated Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, UpdateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>(std::unexpected(Error::NOT_FOUND))));

  auto response = tournamentController->updateTournament(request, tournamentId);

  EXPECT_EQ(response.code, crow::NOT_FOUND);
}

// Tests de DeleteTournament

TEST_F(TournamentControllerTest, DeleteTournament_NoContent) {
  std::string tournamentId = "tournament-123";

  EXPECT_CALL(*tournamentDelegateMock, DeleteTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<void, Error>()));

  auto response = tournamentController->deleteTournament(tournamentId);

  EXPECT_EQ(response.code, crow::NO_CONTENT);
}

TEST_F(TournamentControllerTest, DeleteTournament_NotFound) {
  std::string tournamentId = "non-existent-id";

  EXPECT_CALL(*tournamentDelegateMock, DeleteTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<void, Error>(std::unexpected(Error::NOT_FOUND))));

  auto response = tournamentController->deleteTournament(tournamentId);

  EXPECT_EQ(response.code, crow::NOT_FOUND);
}
