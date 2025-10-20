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

// ========== Tests para CreateTournament ==========

// Validación del JSON y creación exitosa. Response 201
TEST_F(TournamentControllerTest, CreateTournament_ValidTournament_Returns201) {
  // Arrange
  nlohmann::json jsonBody;
  jsonBody["name"] = "Test Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, CreateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>("tournament-id-123")));

  // Act
  auto response = tournamentController->CreateTournament(request);

  // Assert
  EXPECT_EQ(response.code, crow::CREATED);
  EXPECT_EQ(response.get_header_value("Location"), "tournament-id-123");
}

// Validación del JSON y conflicto en DB. Response 409
TEST_F(TournamentControllerTest, CreateTournament_DbConflict_Returns409) {
  // Arrange
  nlohmann::json jsonBody;
  jsonBody["name"] = "Test Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, CreateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>(std::unexpected(Error::DUPLICATE))));

  // Act
  auto response = tournamentController->CreateTournament(request);

  // Assert
  EXPECT_EQ(response.code, crow::CONFLICT);
}

// ========== Tests para GetTournament ==========

// Validar respuesta exitosa y contenido completo. Response 200
TEST_F(TournamentControllerTest, GetTournamentById_Returns200AndCompleteBody) {
  // Arrange
  std::string tournamentId = "tournament-123";
  auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
  tournament->Id() = tournamentId;

  EXPECT_CALL(*tournamentDelegateMock, GetTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Tournament>, Error>(tournament)));

  // Act
  auto response = tournamentController->getTournament(tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse["id"], tournamentId);
}

// Validar respuesta NOT_FOUND. Response 404
TEST_F(TournamentControllerTest, GetTournamentById_NotFound_Returns404) {
  // Arrange
  std::string tournamentId = "non-existent-id";

  EXPECT_CALL(*tournamentDelegateMock, GetTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<std::shared_ptr<domain::Tournament>, Error>(std::unexpected(Error::NOT_FOUND))));

  // Act
  auto response = tournamentController->getTournament(tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::NOT_FOUND);
}

// ========== Tests para GetAllTournaments ==========

// Validar respuesta exitosa con lista de torneos. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_ReturnsList200) {
  // Arrange
  auto tournament1 = std::make_shared<domain::Tournament>("Tournament 1");
  tournament1->Id() = "tournament-1";
  auto tournament2 = std::make_shared<domain::Tournament>("Tournament 2");
  tournament2->Id() = "tournament-2";
  std::vector<std::shared_ptr<domain::Tournament>> tournaments = {tournament1, tournament2};

  EXPECT_CALL(*tournamentDelegateMock, ReadAll())
      .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Tournament>>, Error>(tournaments)));

  // Act
  auto response = tournamentController->ReadAll();

  // Assert
  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse.size(), 2);
}

// Validar respuesta exitosa con lista vacía. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_ReturnsEmptyList200) {
  // Arrange
  std::vector<std::shared_ptr<domain::Tournament>> emptyTournaments;

  EXPECT_CALL(*tournamentDelegateMock, ReadAll())
      .WillOnce(testing::Return(std::expected<std::vector<std::shared_ptr<domain::Tournament>>, Error>(emptyTournaments)));

  // Act
  auto response = tournamentController->ReadAll();

  // Assert
  EXPECT_EQ(response.code, crow::OK);
  auto jsonResponse = nlohmann::json::parse(response.body);
  EXPECT_EQ(jsonResponse.size(), 0);
}

// ========== Tests para UpdateTournament ==========

// Validación del JSON y actualización exitosa. Response 204
TEST_F(TournamentControllerTest, UpdateTournament_ValidJson_DelegatesAndReturns204) {
  // Arrange
  std::string tournamentId = "tournament-123";
  nlohmann::json jsonBody;
  jsonBody["name"] = "Updated Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, UpdateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>("")));

  // Act
  auto response = tournamentController->updateTournament(request, tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::NO_CONTENT);
}

// Validación del JSON y torneo no encontrado. Response 404
TEST_F(TournamentControllerTest, UpdateTournament_NotFound_Returns404) {
  // Arrange
  std::string tournamentId = "non-existent-id";
  nlohmann::json jsonBody;
  jsonBody["name"] = "Updated Tournament";
  std::string requestBody = jsonBody.dump();
  crow::request request;
  request.body = requestBody;

  EXPECT_CALL(*tournamentDelegateMock, UpdateTournament(testing::_))
      .WillOnce(testing::Return(std::expected<std::string, Error>(std::unexpected(Error::NOT_FOUND))));

  // Act
  auto response = tournamentController->updateTournament(request, tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::NOT_FOUND);
}

// ========== Tests para DeleteTournament ==========

TEST_F(TournamentControllerTest, DeleteTournament_Success_Returns204) {
  // Arrange
  std::string tournamentId = "tournament-123";

  EXPECT_CALL(*tournamentDelegateMock, DeleteTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<void, Error>()));

  // Act
  auto response = tournamentController->deleteTournament(tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::NO_CONTENT);
}

TEST_F(TournamentControllerTest, DeleteTournament_NotFound_Returns404) {
  // Arrange
  std::string tournamentId = "non-existent-id";

  EXPECT_CALL(*tournamentDelegateMock, DeleteTournament(tournamentId))
      .WillOnce(testing::Return(std::expected<void, Error>(std::unexpected(Error::NOT_FOUND))));

  // Act
  auto response = tournamentController->deleteTournament(tournamentId);

  // Assert
  EXPECT_EQ(response.code, crow::NOT_FOUND);
}
