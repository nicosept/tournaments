#include "delegate/GroupDelegate.hpp"
#include "exception/NotFound.hpp"
#include "exception/InvalidFormat.hpp"
#include "exception/Duplicate.hpp"
#include "exception/Error.hpp"
#include <nlohmann/json.hpp>

#include <utility>
#include <sstream>
#include <iostream>
#include <format>
#include <pqxx/pqxx>

GroupDelegate::GroupDelegate(const std::shared_ptr<TournamentRepository>& tournamentRepository, const std::shared_ptr<IGroupRepository>& groupRepository, const std::shared_ptr<TeamRepository>& teamRepository, const std::shared_ptr<IQueueMessageProducer>& messageProducer)
    : tournamentRepository(tournamentRepository), groupRepository(groupRepository), teamRepository(teamRepository), messageProducer(messageProducer){}

std::expected<std::vector<std::shared_ptr<domain::Group>>, Error> GroupDelegate::GetGroups(const std::string_view& tournamentId) {
    // Validacion de formato de UUID para tournamentId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion extra
    try {
        return this->groupRepository->FindByTournamentId(tournamentId);
    } catch (const std::exception& e) {
        // Error general al leer la base de datos
        return std::unexpected(Error::UNKNOWN_ERROR);
    }
}

std::expected<std::shared_ptr<domain::Group>, Error> GroupDelegate::GetGroup(const std::string_view& tournamentId, const std::string_view& groupId) {
    // Validacion de formato de UUID para tournamentId y groupId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE) || !std::regex_match(std::string(groupId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo 
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de existencia del grupo
    auto group = groupRepository->FindByTournamentIdAndGroupId(tournamentId, groupId);
    if (group == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion extra
    try {
        return groupRepository->FindByTournamentIdAndGroupId(tournamentId, groupId);
    } catch (const std::exception& e) {
        return std::unexpected(Error::UNKNOWN_ERROR);
    }
}

std::expected<std::string, Error> GroupDelegate::CreateGroup(const std::string_view& tournamentId, const domain::Group& group) {
    // Validacion de formato de UUID para tournamentId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de formato del grupo
    if (group.Name().empty()) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de cantidad maxima de equipos en el grupo
    if (group.Teams().size() > 32) {
        return std::unexpected(Error::UNPROCESSABLE_ENTITY);
    }
    // Validacion de equipos si los hay
    if (!group.Teams().empty()) {
        for (auto& t : group.Teams()) {
            // Validacion de formato UUID de cada equipo
            if (!std::regex_match(t.Id, ID_VALUE)) {
                return std::unexpected(Error::INVALID_FORMAT);
            }
            // Validacion de existencia de cada equipo
            auto team = teamRepository->ReadById(t.Id);
            if (team == nullptr) {
                return std::unexpected(Error::NOT_FOUND);
            }
        }
    }
    
    // Crear el grupo solo con el nombre (sin equipos)
    domain::Group g = group;
    g.TournamentId() = tournament->Id();
    g.Teams().clear(); // No se agregan equipos en la creación
    
    try {
        auto id = groupRepository->Create(g);
        
        // Si hay equipos, agregarlos usando UpdateTeams
        if (!group.Teams().empty()) {
            auto updateResult = UpdateTeams(tournamentId, id, group.Teams());
            if (!updateResult) {
                // Si falla agregar equipos, eliminar el grupo creado
                groupRepository->Delete(id);
                return std::unexpected(updateResult.error());
            }
        }
        
        return id;
    } catch (const pqxx::unique_violation& e) {
        // Validacion de duplicado
        if (e.sqlstate() == "23505") {
            return std::unexpected(Error::DUPLICATE);
        }
        return std::unexpected(Error::UNKNOWN_ERROR);
    } catch (const std::exception& e) {
        // Validacion extra
        return std::unexpected(Error::UNKNOWN_ERROR);
    }
}

std::expected<void, Error> GroupDelegate::UpdateGroup(const std::string_view& tournamentId, const domain::Group& group, const std::string_view& groupId) {
    // Validacion de formato de UUID para tournamentId y groupId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE) || !std::regex_match(std::string(groupId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de formato del grupo
    if (group.Name().empty()) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de existencia del grupo
    auto group1 = groupRepository->FindByTournamentIdAndGroupId(tournamentId, groupId);
    if (group1 == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }

    domain::Group updatedGroup = group;
    updatedGroup.Id() = groupId;
    updatedGroup.TournamentId() = tournamentId;

    try {
        groupRepository->Update(updatedGroup);
        return {};
    } catch (const pqxx::unique_violation& e) {
        // Validacion de duplicado
        if (e.sqlstate() == "23505") {
            return std::unexpected(Error::DUPLICATE);
        }
        return std::unexpected(Error::UNKNOWN_ERROR);
    } catch (const std::exception& e) {
        // Validacion extra
        return std::unexpected(Error::UNKNOWN_ERROR);
    }
}
std::expected<void, Error> GroupDelegate::RemoveGroup(const std::string_view& tournamentId, const std::string_view& groupId) {
    // Validacion de formato de UUID para tournamentId y groupId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE) || !std::regex_match(std::string(groupId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de existencia del grupo
    auto group = groupRepository->FindByTournamentIdAndGroupId(tournamentId, groupId);
    if (group == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    try {
        groupRepository->Delete(groupId.data());
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(Error::UNKNOWN_ERROR);
    }
}

std::expected<void, Error> GroupDelegate::UpdateTeams(const std::string_view& tournamentId, const std::string_view& groupId, const std::vector<domain::Team>& teams) {
    // Validacion de formato de UUID para tournamentId y groupId
    if (!std::regex_match(std::string(tournamentId), ID_VALUE) || !std::regex_match(std::string(groupId), ID_VALUE)) {
        return std::unexpected(Error::INVALID_FORMAT);
    }
    // Validacion de existencia del torneo
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de existencia del grupo
    auto group = groupRepository->FindByTournamentIdAndGroupId(tournamentId, groupId);
    if (group == nullptr) {
        return std::unexpected(Error::NOT_FOUND);
    }
    // Validacion de cantidad maxima de equipos en el grupo
    if (group->Teams().size() + teams.size() > 32) {
        return std::unexpected(Error::UNPROCESSABLE_ENTITY);
    }
    for (const auto& team : teams) {
        // Validacion de formato UUID de cada equipo
        if (!std::regex_match(team.Id, ID_VALUE)) {
            return std::unexpected(Error::INVALID_FORMAT);
        }
        // Validacion de duplicados
        if (groupRepository->FindByGroupIdAndTeamId(groupId, team.Id) != nullptr) {
            return std::unexpected(Error::DUPLICATE);
        }
        // Validacion de existencia de cada equipo
        const auto persistedTeam = teamRepository->ReadById(team.Id);
        if (persistedTeam == nullptr) {
            return std::unexpected(Error::UNPROCESSABLE_ENTITY);
        }
        try {
            groupRepository->UpdateGroupAddTeam(groupId, persistedTeam);
            std::unique_ptr<nlohmann::json> message = std::make_unique<nlohmann::json>();
            message->emplace("tournamentId", tournamentId);
            message->emplace("groupId", groupId);
            message->emplace("teamId", team.Id);
            messageProducer->SendMessage(message->dump(), "tournament.team-add");
        } catch (const std::exception& e) {
            return std::unexpected(Error::UNKNOWN_ERROR);
        }
    }
    return {};
}
