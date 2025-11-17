#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <crow.h>
#include <nlohmann/json.hpp>
#include <expected>

#include "domain/Match.hpp"
#include "delegate/IMatchDelegate.hpp"
#include "controller/MatchController.hpp"
#include "exception/Error.hpp"

class MatchDelegateMock : public IMatchDelegate {
public:
  MOCK_METHOD((std::expected<std::shared_ptr<domain::Match>, Error>), GetMatch, (std::string_view tournamentId, std::string_view matchId), (override));
  MOCK_METHOD((std::expected<std::vector<std::shared_ptr<domain::Match>>, Error>), GetMatches,
              (std::string_view tournamentId), (override));
  MOCK_METHOD((std::expected<std::string, Error>), UpdateMatchScore,
              (const domain::Match&), (override));
};

class MatchControllerTest : public ::testing::Test {
protected:
  std::shared_ptr<MatchDelegateMock> matchDelegateMock;
  std::shared_ptr<MatchController> matchController;

  void SetUp() override {
    matchDelegateMock = std::make_shared<MatchDelegateMock>();
    matchController = std::make_shared<MatchController>(matchDelegateMock);
  }
};

// Tests de GetMatch

// Validar respuesta exitosa y contenido del match. Response 200
TEST_F(MatchControllerTest, GetMatch_Ok) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  auto expectedMatch = std::make_shared<domain::Match>();
  expectedMatch->Id() = matchId;
  expectedMatch->TournamentId() = tournamentId;
  expectedMatch->Name() = "W0";
  expectedMatch->HomeTeamId() = "team-home-id";
  expectedMatch->VisitorTeamId() = "team-visitor-id";
  expectedMatch->MatchScore().homeTeamScore = 2;
  expectedMatch->MatchScore().visitorTeamScore = 1;

  EXPECT_CALL(*matchDelegateMock, GetMatch(
      std::string_view(tournamentId),
      std::string_view(matchId)))
    .WillOnce(testing::Return(
        std::expected<std::shared_ptr<domain::Match>, Error>{std::in_place, expectedMatch}));

  crow::response response = matchController->getMatch(tournamentId, matchId);
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  EXPECT_EQ(expectedMatch->Id(), jsonResponse["id"].get<std::string>());
  EXPECT_EQ(expectedMatch->TournamentId(), jsonResponse["tournamentId"].get<std::string>());
  EXPECT_EQ(expectedMatch->Name(), jsonResponse["name"].get<std::string>());
  EXPECT_EQ(expectedMatch->HomeTeamId(), jsonResponse["homeTeamId"].get<std::string>());
  EXPECT_EQ(expectedMatch->VisitorTeamId(), jsonResponse["visitorTeamId"].get<std::string>());
  EXPECT_EQ(expectedMatch->MatchScore().homeTeamScore, 
            jsonResponse["score"]["homeTeamScore"].get<int>());
  EXPECT_EQ(expectedMatch->MatchScore().visitorTeamScore, 
            jsonResponse["score"]["visitorTeamScore"].get<int>());
}

// Validar respuesta NOT_FOUND cuando el match no existe. Response 404
TEST_F(MatchControllerTest, GetMatch_NotFound) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "non-existent-match-id";

  EXPECT_CALL(*matchDelegateMock, GetMatch(
      std::string_view(tournamentId),
      std::string_view(matchId)))
    .WillOnce(testing::Return(
        std::expected<std::shared_ptr<domain::Match>, Error>{std::unexpected(Error::NOT_FOUND)}));

  crow::response response = matchController->getMatch(tournamentId, matchId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}

// Validar respuesta NOT_FOUND cuando el torneo no existe. Response 404
TEST_F(MatchControllerTest, GetMatch_TournamentNotFound) {
  std::string tournamentId = "non-existent-tournament-id";
  std::string matchId = "match-id-001";

  EXPECT_CALL(*matchDelegateMock, GetMatch(
      std::string_view(tournamentId),
      std::string_view(matchId)))
    .WillOnce(testing::Return(
        std::expected<std::shared_ptr<domain::Match>, Error>{std::unexpected(Error::NOT_FOUND)}));

  crow::response response = matchController->getMatch(tournamentId, matchId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}

// Tests de GetMatches

// Validar respuesta exitosa con lista de matches. Response 200
TEST_F(MatchControllerTest, GetMatches_Ok) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::vector<std::shared_ptr<domain::Match>> matches;
  
  auto match1 = std::make_shared<domain::Match>();
  match1->Id() = "match-id-001";
  match1->TournamentId() = tournamentId;
  match1->Name() = "W0";
  match1->HomeTeamId() = "team-home-1";
  match1->VisitorTeamId() = "team-visitor-1";
  match1->MatchScore().homeTeamScore = 3;
  match1->MatchScore().visitorTeamScore = 1;
  
  auto match2 = std::make_shared<domain::Match>();
  match2->Id() = "match-id-002";
  match2->TournamentId() = tournamentId;
  match2->Name() = "W1";
  match2->HomeTeamId() = "team-home-2";
  match2->VisitorTeamId() = "team-visitor-2";
  match2->MatchScore().homeTeamScore = 2;
  match2->MatchScore().visitorTeamScore = 1;
  
  matches.push_back(match1);
  matches.push_back(match2);

  EXPECT_CALL(*matchDelegateMock, GetMatches(std::string_view(tournamentId)))
    .WillOnce(testing::Return(
        std::expected<std::vector<std::shared_ptr<domain::Match>>, Error>{std::in_place, matches}));

  crow::response response = matchController->getMatches(tournamentId);
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  ASSERT_EQ(jsonResponse.size(), matches.size());
  
  EXPECT_EQ(jsonResponse[0]["id"].get<std::string>(), matches[0]->Id());
  EXPECT_EQ(jsonResponse[0]["tournamentId"].get<std::string>(), matches[0]->TournamentId());
  EXPECT_EQ(jsonResponse[0]["name"].get<std::string>(), matches[0]->Name());
  EXPECT_EQ(jsonResponse[0]["homeTeamId"].get<std::string>(), matches[0]->HomeTeamId());
  EXPECT_EQ(jsonResponse[0]["visitorTeamId"].get<std::string>(), matches[0]->VisitorTeamId());
  EXPECT_EQ(jsonResponse[0]["score"]["homeTeamScore"].get<int>(), 3);
  EXPECT_EQ(jsonResponse[0]["score"]["visitorTeamScore"].get<int>(), 1);
  
  EXPECT_EQ(jsonResponse[1]["id"].get<std::string>(), matches[1]->Id());
  EXPECT_EQ(jsonResponse[1]["name"].get<std::string>(), matches[1]->Name());
}

// Validar respuesta exitosa con lista vacia de matches. Response 200
TEST_F(MatchControllerTest, GetMatches_Empty) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::vector<std::shared_ptr<domain::Match>> emptyMatches;

  EXPECT_CALL(*matchDelegateMock, GetMatches(std::string_view(tournamentId)))
    .WillOnce(testing::Return(
        std::expected<std::vector<std::shared_ptr<domain::Match>>, Error>{std::in_place, emptyMatches}));

  crow::response response = matchController->getMatches(tournamentId);
  auto jsonResponse = nlohmann::json::parse(response.body);

  EXPECT_EQ(crow::OK, response.code);
  ASSERT_EQ(jsonResponse.size(), 0);
}

// Validar respuesta NOT_FOUND cuando el torneo no existe. Response 404
TEST_F(MatchControllerTest, GetMatches_TournamentNotFound) {
  std::string tournamentId = "non-existent-tournament-id";

  EXPECT_CALL(*matchDelegateMock, GetMatches(std::string_view(tournamentId)))
    .WillOnce(testing::Return(
        std::expected<std::vector<std::shared_ptr<domain::Match>>, Error>{std::unexpected(Error::NOT_FOUND)}));

  crow::response response = matchController->getMatches(tournamentId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}

// Tests de UpdateMatchScore

// Validar actualizacion exitosa del score. Response 200
TEST_F(MatchControllerTest, UpdateMatchScore_Ok) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  domain::Match capturedMatch;
  
  EXPECT_CALL(*matchDelegateMock, UpdateMatchScore(
      testing::AllOf(
          testing::ResultOf([](const domain::Match& m) { return m.TournamentId(); }, testing::Eq(tournamentId)),
          testing::ResultOf([](const domain::Match& m) { return m.Id(); }, testing::Eq(matchId)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().homeTeamScore; }, testing::Eq(3)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().visitorTeamScore; }, testing::Eq(2)))))
    .WillOnce(testing::DoAll(
        testing::SaveArg<0>(&capturedMatch),
        testing::Return(std::expected<std::string, Error>{std::in_place, matchId})));

  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", 3},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::OK, response.code);
  EXPECT_EQ(tournamentId, capturedMatch.TournamentId());
  EXPECT_EQ(matchId, capturedMatch.Id());
  EXPECT_EQ(3, capturedMatch.MatchScore().homeTeamScore);
  EXPECT_EQ(2, capturedMatch.MatchScore().visitorTeamScore);
}

// Validar error empate con scores en cero. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_ZeroScores) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  EXPECT_CALL(*matchDelegateMock, UpdateMatchScore(
      testing::AllOf(
          testing::ResultOf([](const domain::Match& m) { return m.TournamentId(); }, testing::Eq(tournamentId)),
          testing::ResultOf([](const domain::Match& m) { return m.Id(); }, testing::Eq(matchId)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().homeTeamScore; }, testing::Eq(0)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().visitorTeamScore; }, testing::Eq(0)))))
    .WillOnce(testing::Return(std::expected<std::string, Error>{std::unexpected(Error::INVALID_FORMAT)}));

  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", 0},
      {"visitorTeamScore", 0}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
}

// Validar error cuando el JSON es invalido. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_InvalidJson) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  crow::request request;
  request.body = "{invalid json";

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Invalid JSON format", response.body);
}

// Validar error cuando falta el objeto score. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_MissingMatchScore) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"homeTeamScore", 3},
    {"visitorTeamScore", 2}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Missing or invalid score object", response.body);
}

// Validar error cuando score no es un objeto. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_MatchScoreNotObject) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"score", "not an object"}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Missing or invalid score object", response.body);
}

// Validar error cuando faltan campos de scores. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_MissingScoreFields) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", 3}
      // falta visitorTeamScore
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("score must contain integer homeTeamScore and visitorTeamScore", response.body);
}

// Validar error cuando los scores no son enteros. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_ScoresNotInteger) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", "three"},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("score must contain integer homeTeamScore and visitorTeamScore", response.body);
}

// Validar error cuando los scores son negativos. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_NegativeScores) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", -1},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Scores must be non-negative", response.body);
}

// Validar error cuando el tournamentId del body no coincide con el path. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_TournamentIdMismatch) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"tournamentId", "different-tournament-id"},
    {"score", {
      {"homeTeamScore", 3},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Tournament ID in body does not match path", response.body);
}

// Validar error cuando el matchId del body no coincide con el path. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_MatchIdMismatch) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  nlohmann::json requestBody = {
    {"id", "different-match-id"},
    {"score", {
      {"homeTeamScore", 3},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
  EXPECT_EQ("Match ID in body does not match path", response.body);
}

// Validar error NOT_FOUND cuando el match no existe. Response 404
TEST_F(MatchControllerTest, UpdateMatchScore_MatchNotFound) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "non-existent-match-id";
  
  EXPECT_CALL(*matchDelegateMock, UpdateMatchScore(testing::_))
    .WillOnce(testing::Return(
        std::expected<std::string, Error>{std::unexpected(Error::NOT_FOUND)}));

  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", 3},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::NOT_FOUND, response.code);
}

// Validar error de formato invalido. Response 400
TEST_F(MatchControllerTest, UpdateMatchScore_InvalidFormat) {
  std::string tournamentId = "550e8400-e29b-41d4-a716-446655440000";
  std::string matchId = "match-id-001";
  
  EXPECT_CALL(*matchDelegateMock, UpdateMatchScore(
      testing::AllOf(
          testing::ResultOf([](const domain::Match& m) { return m.TournamentId(); }, testing::Eq(tournamentId)),
          testing::ResultOf([](const domain::Match& m) { return m.Id(); }, testing::Eq(matchId)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().homeTeamScore; }, testing::Eq(3)),
          testing::ResultOf([](const domain::Match& m) { return m.MatchScore().visitorTeamScore; }, testing::Eq(2)))))
    .WillOnce(testing::Return(
        std::expected<std::string, Error>{std::unexpected(Error::INVALID_FORMAT)}));

  nlohmann::json requestBody = {
    {"score", {
      {"homeTeamScore", 3},
      {"visitorTeamScore", 2}
    }}
  };
  
  crow::request request;
  request.body = requestBody.dump();

  crow::response response = matchController->updateMatchScore(request, tournamentId, matchId);

  EXPECT_EQ(crow::BAD_REQUEST, response.code);
}
