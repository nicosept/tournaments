#include "delegate/MatchDelegate.hpp"

#include <expected>
#include <iostream>
#include <regex>
#include <utility>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

#include "exception/Error.hpp"
#include "domain/Constants.hpp"

MatchDelegate::MatchDelegate(const std::shared_ptr<IMatchRepository>& matchRepository, const std::shared_ptr<TournamentRepository>& tournamentRepository, const std::shared_ptr<IQueueMessageProducer>& messageProducer)
    : matchRepository(matchRepository), tournamentRepository(tournamentRepository), messageProducer(messageProducer) {}

std::expected<std::vector<std::shared_ptr<domain::Match>>, Error> MatchDelegate::GetMatches(std::string_view tournamentId) {
    if (!std::regex_match(std::string{tournamentId}, ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    if (!tournamentRepository->ReadById(tournamentId.data())) {
        return std::unexpected(Error::NOT_FOUND);
    }
    return matchRepository->FindByTournamentId(tournamentId);
}

std::expected<std::shared_ptr<domain::Match>, Error> MatchDelegate::GetMatch(std::string_view tournamentId, std::string_view matchId) {
    if (!std::regex_match(std::string{tournamentId}, ID_VALUE) || 
        !std::regex_match(std::string{matchId}, ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    if (!tournamentRepository->ReadById(tournamentId.data())) {
        return std::unexpected(Error::NOT_FOUND);
    }
    return matchRepository->FindByTournamentIdAndMatchId(tournamentId, matchId);
}

std::expected<std::string, Error> MatchDelegate::UpdateMatchScore(const domain::Match& match) {
  if (!std::regex_match(std::string{match.TournamentId()}, ID_VALUE) ||
    !std::regex_match(std::string{match.Id()}, ID_VALUE)) {
    return std::unexpected(Error::INVALID_FORMAT);
  }

  auto existingMatch = matchRepository->FindByTournamentIdAndMatchId(match.TournamentId(), match.Id());
  if (!existingMatch) {
    return std::unexpected(Error::NOT_FOUND);
  }


  const auto& score = match.MatchScore();
  // Regla de negocio: No se permiten empates en doble eliminación (0-0)
  if ((score.homeTeamScore == score.visitorTeamScore) ||
      score.homeTeamScore < 0 || score.visitorTeamScore < 0) {
    return std::unexpected(Error::INVALID_FORMAT);
  }

  matchRepository->UpdateMatchScore(match.Id(), score);
  
  // Send message to ActiveMQ for consumer to process
  try {
    std::unique_ptr<nlohmann::json> message = std::make_unique<nlohmann::json>();
    message->emplace("tournamentId", match.TournamentId());
    message->emplace("matchId", match.Id());
    message->emplace("homeTeamScore", score.homeTeamScore);
    message->emplace("visitorTeamScore", score.visitorTeamScore);
    messageProducer->SendMessage(message->dump(), "tournament.score-update");
  } catch (const std::exception& e) {
    std::cout << "[MatchDelegate] ERROR sending message: " << e.what() << std::endl;
  }
  
  return std::string{match.Id()};
}