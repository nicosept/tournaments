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
  // Test body for CreateTournament success will go here
}

// Validación del JSON y conflicto en DB. Response 409
TEST_F(TournamentControllerTest, CreateTournament_DbConflict_Returns409) {
  // Test body for CreateTournament conflict will go here
}

// ========== Tests para GetTournament ==========

// Validar respuesta exitosa y contenido completo. Response 200
TEST_F(TournamentControllerTest, GetTournamentById_Returns200AndCompleteBody) {
  // Test body for GetTournament by ID success will go here
}

// Validar respuesta NOT_FOUND. Response 404
TEST_F(TournamentControllerTest, GetTournamentById_NotFound_Returns404) {
  // Test body for GetTournament Not Found will go here
}

// ========== Tests para GetAllTournaments ==========

// Validar respuesta exitosa con lista de torneos. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_ReturnsList200) {
  // Test body for GetAllTournaments with list will go here
}

// Validar respuesta exitosa con lista vacía. Response 200
TEST_F(TournamentControllerTest, GetAllTournaments_ReturnsEmptyList200) {
  // Test body for empty list will go here
}

// ========== Tests para UpdateTournament ==========

// Validación del JSON y actualización exitosa. Response 204
TEST_F(TournamentControllerTest, UpdateTournament_ValidJson_DelegatesAndReturns204) {
  // Test body for UpdateTournament success will go here
}

// Validación del JSON y torneo no encontrado. Response 404
TEST_F(TournamentControllerTest, UpdateTournament_NotFound_Returns404) {
  // Test body for UpdateTournament Not Found will go here
}

// ========== Tests para DeleteTournament ==========

TEST_F(TournamentControllerTest, DeleteTournament_Success_Returns204) {
  // Test body for DeleteTournament success will go here
}

TEST_F(TournamentControllerTest, DeleteTournament_NotFound_Returns404) {
  // Test body for DeleteTournament Not Found will go here
}
