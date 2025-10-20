#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>
#include <pqxx/pqxx>

#include "domain/Group.hpp"
#include "domain/Team.hpp"
#include "domain/Tournament.hpp"
#include "delegate/GroupDelegate.hpp"
#include "persistence/repository/IGroupRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "persistence/repository/TeamRepository.hpp"
#include "cms/IQueueMessageProducer.hpp"
#include "exception/Error.hpp"

class MockGroupRepository : public IGroupRepository {
    public:
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Group>>, FindByTournamentId, (const std::string_view& tournamentId), (override));
    MOCK_METHOD(std::string, Create, (const domain::Group& entity), (override));
    MOCK_METHOD(std::shared_ptr<domain::Group>, ReadById, (std::string id), (override));
    MOCK_METHOD(std::string, Update, (const domain::Group& entity), (override));
    MOCK_METHOD(void, Delete, (std::string id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Group>>, ReadAll, (), (override));
    MOCK_METHOD(std::shared_ptr<domain::Group>, FindByTournamentIdAndGroupId, (const std::string_view& tournamentId, const std::string_view& groupId), (override));
    MOCK_METHOD(std::shared_ptr<domain::Group>, FindByTournamentIdAndTeamId, (const std::string_view& tournamentId, const std::string_view& teamId), (override));   
    MOCK_METHOD(std::shared_ptr<domain::Group>, FindByGroupIdAndTeamId, (const std::string_view& groupId, const std::string_view& teamId), (override));
    MOCK_METHOD(void, UpdateGroupAddTeam, (const std::string_view& groupId, const std::shared_ptr<domain::Team> & team), (override));
};

class MockTournamentRepository : public TournamentRepository {
public:
    MockTournamentRepository() : TournamentRepository(nullptr) {}
    MOCK_METHOD(std::shared_ptr<domain::Tournament>, ReadById, (std::string id), (override));
    MOCK_METHOD(std::string, Create, (const domain::Tournament& entity), (override));
    MOCK_METHOD(std::string, Update, (const domain::Tournament& entity), (override));
    MOCK_METHOD(void, Delete, (std::string id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Tournament>>, ReadAll, (), (override));
};

class MockTeamRepository : public TeamRepository {
public:
    MockTeamRepository() : TeamRepository(nullptr) {}
    MOCK_METHOD(std::shared_ptr<domain::Team>, ReadById, (std::string_view id), (override));
    MOCK_METHOD(std::string_view, Create, (const domain::Team& entity), (override));
    MOCK_METHOD(std::string_view, Update, (const domain::Team& entity), (override));
    MOCK_METHOD(void, Delete, (std::string_view id), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<domain::Team>>, ReadAll, (), (override));
};

class MockQueueMessageProducer : public IQueueMessageProducer {
public:
    MOCK_METHOD(void, SendMessage, (const std::string_view& message, const std::string_view& queue), (override));
};

class GroupDelegateTest : public ::testing::Test {
    protected:
    std::shared_ptr<MockTournamentRepository> mockTournamentRepository;
    std::shared_ptr<MockGroupRepository> mockGroupRepository;
    std::shared_ptr<MockTeamRepository> mockTeamRepository;
    std::shared_ptr<MockQueueMessageProducer> mockMessageProducer;
    std::shared_ptr<GroupDelegate> groupDelegate;
    
    const std::string validTournamentId = "12345678-1234-1234-1234-123456789abc";
    const std::string validGroupId = "87654321-4321-4321-4321-cba987654321";
    const std::string validTeamId = "abcdef01-2345-6789-abcd-ef0123456789";

    void SetUp() override {
        mockTournamentRepository = std::make_shared<MockTournamentRepository>();
        mockGroupRepository = std::make_shared<MockGroupRepository>();
        mockTeamRepository = std::make_shared<MockTeamRepository>();
        mockMessageProducer = std::make_shared<MockQueueMessageProducer>();
        groupDelegate = std::make_shared<GroupDelegate>(mockTournamentRepository, mockGroupRepository, mockTeamRepository);
    }
};

//======================= CreateGroup TESTS ========================

// Validar creacion exitosa de grupo y que se genere evento
TEST_F(GroupDelegateTest, CreateGroup_Id) {
    domain::Group group{"Test Group", "test-group"};
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, Create(testing::_))
        .WillOnce(testing::DoAll(
            testing::WithArg<0>(testing::Invoke([&](const domain::Group& g) {
                EXPECT_EQ(g.TournamentId(), validTournamentId);
                EXPECT_EQ(g.Name(), "Test Group");
            })),
            testing::Return(validGroupId)
        ));

    auto result = groupDelegate->CreateGroup(validTournamentId, group);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), validGroupId);
}

// Validar error cuando grupo ya existe
TEST_F(GroupDelegateTest, CreateGroup_Error) {
    domain::Group group{"Test Group", "test-group"};
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    auto create_unique_violation = []() -> pqxx::unique_violation {
        return pqxx::unique_violation("duplicate key value violates unique constraint", "", "23505");
    };
    
    EXPECT_CALL(*mockGroupRepository, Create(testing::_))
        .WillOnce(testing::DoAll(
            testing::WithArg<0>(testing::Invoke([&](const domain::Group& g) {
                EXPECT_EQ(g.TournamentId(), validTournamentId);
                EXPECT_EQ(g.Name(), "Test Group");
            })),
            testing::Throw(create_unique_violation())
        ));

    auto result = groupDelegate->CreateGroup(validTournamentId, group);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::DUPLICATE);
}

// Test 3: Validar error cuando se alcanza numero maximo de equipos
TEST_F(GroupDelegateTest, CreateGroup_MaxTeams) {
    domain::Group group{"Test Group", "test-group"};
    for (int i = 0; i < 32; ++i) {
        domain::Team team;
        team.Id = "team-" + std::to_string(i);
        team.Name = "Team " + std::to_string(i);
        group.Teams().push_back(team);
    }
    
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockGroupRepository, Create(testing::_))
        .Times(0);

    auto result = groupDelegate->CreateGroup(validTournamentId, group);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error() == Error::INVALID_FORMAT || result.error() == Error::NOT_FOUND);
}

//======================= GetGroup TESTS ========================

// Validar busqueda exitosa de grupo por ID y torneo por ID
TEST_F(GroupDelegateTest, GetGroup_Ok) {
    auto group = std::make_shared<domain::Group>(domain::Group{"Test Group", validGroupId});
    group->TournamentId() = validTournamentId;
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(group))
        .WillOnce(testing::Return(group));

    auto result = groupDelegate->GetGroup(validTournamentId, validGroupId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->Id(), validGroupId);
    EXPECT_EQ((*result)->Name(), "Test Group");
    EXPECT_EQ((*result)->TournamentId(), validTournamentId);
}

// Validar busqueda con resultado nulo
TEST_F(GroupDelegateTest, GetGroup_NotFound) {
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(nullptr));

    auto result = groupDelegate->GetGroup(validTournamentId, validGroupId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

//======================= UpdateGroup TESTS ========================

// Validar actualizacion exitosa de grupo
TEST_F(GroupDelegateTest, UpdateGroup_Ok) {
    domain::Group inputGroup{"Updated Group", "original-id"};
    auto existingGroup = std::make_shared<domain::Group>(domain::Group{"Existing Group", validGroupId});
    existingGroup->TournamentId() = validTournamentId;
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(existingGroup));

    EXPECT_CALL(*mockGroupRepository, Update(testing::_))
        .WillOnce(testing::DoAll(
            testing::WithArg<0>(testing::Invoke([&](const domain::Group& g) {
                EXPECT_EQ(g.Id(), validGroupId);
                EXPECT_EQ(g.TournamentId(), validTournamentId);
                EXPECT_EQ(g.Name(), "Updated Group");
            })),
            testing::Return(validGroupId)
        ));

    auto result = groupDelegate->UpdateGroup(validTournamentId, inputGroup, validGroupId);

    ASSERT_TRUE(result.has_value());
}

// Validar error cuando ID no se encuentra
TEST_F(GroupDelegateTest, UpdateGroup_NotFound) {
    domain::Group inputGroup{"Updated Group", "original-id"};
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));

    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(nullptr));

    auto result = groupDelegate->UpdateGroup(validTournamentId, inputGroup, validGroupId);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NOT_FOUND);
}

//======================= UpdateTeams (AddTeamToGroup) TESTS ========================

// Validar agregar equipo exitosamente a grupo y que se publique mensaje
TEST_F(GroupDelegateTest, UpdateTeams_Ok) {
    domain::Team team;
    team.Id = validTeamId;
    team.Name = "Test Team";
    std::vector<domain::Team> teams = {team};
    
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;
    
    auto group = std::make_shared<domain::Group>(domain::Group{"Test Group", validGroupId});
    group->TournamentId() = validTournamentId;
    
    auto persistedTeam = std::make_shared<domain::Team>();
    persistedTeam->Id = validTeamId;
    persistedTeam->Name = "Test Team";

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(group));
    
    EXPECT_CALL(*mockGroupRepository, FindByGroupIdAndTeamId(
        testing::Eq(validGroupId), 
        testing::Eq(validTeamId)))
        .WillOnce(testing::Return(nullptr));
    
    EXPECT_CALL(*mockTeamRepository, ReadById(testing::Eq(validTeamId)))
        .WillOnce(testing::Return(persistedTeam));
    
    EXPECT_CALL(*mockGroupRepository, UpdateGroupAddTeam(
        testing::Eq(validGroupId), 
        testing::_))
        .WillOnce(testing::WithArg<1>(testing::Invoke([&](const std::shared_ptr<domain::Team>& t) {
            EXPECT_EQ(t->Id, validTeamId);
            EXPECT_EQ(t->Name, "Test Team");
        })));

    auto result = groupDelegate->UpdateTeams(validTournamentId, validGroupId, teams);

    ASSERT_TRUE(result.has_value());
}

// Validar error cuando equipo no existe
TEST_F(GroupDelegateTest, UpdateTeams_TeamNotFound) {
    domain::Team team;
    team.Id = validTeamId;
    team.Name = "Non-existent Team";
    std::vector<domain::Team> teams = {team};
    
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;
    
    auto group = std::make_shared<domain::Group>(domain::Group{"Test Group", validGroupId});
    group->TournamentId() = validTournamentId;

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(group));
    
    EXPECT_CALL(*mockGroupRepository, FindByGroupIdAndTeamId(
        testing::Eq(validGroupId), 
        testing::Eq(validTeamId)))
        .WillOnce(testing::Return(nullptr));
    
    EXPECT_CALL(*mockTeamRepository, ReadById(testing::Eq(validTeamId)))
        .WillOnce(testing::Return(nullptr));

    auto result = groupDelegate->UpdateTeams(validTournamentId, validGroupId, teams);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::UNPROCESSABLE_ENTITY);
}

// Validar error cuando grupo esta lleno
TEST_F(GroupDelegateTest, UpdateTeams_GroupFull) {
    domain::Team team;
    team.Id = validTeamId;
    team.Name = "Test Team";
    std::vector<domain::Team> teams = {team};
    
    auto tournament = std::make_shared<domain::Tournament>(domain::Tournament{"Tournament Name"});
    tournament->Id() = validTournamentId;
    
    auto group = std::make_shared<domain::Group>(domain::Group{"Test Group", validGroupId});
    group->TournamentId() = validTournamentId;
    
    for (int i = 0; i < 32; ++i) {
        domain::Team existingTeam;
        existingTeam.Id = "team-" + std::to_string(i);
        existingTeam.Name = "Team " + std::to_string(i);
        group->Teams().push_back(existingTeam);
    }

    EXPECT_CALL(*mockTournamentRepository, ReadById(testing::Eq(validTournamentId)))
        .WillOnce(testing::Return(tournament));
    
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(
        testing::Eq(validTournamentId), 
        testing::Eq(validGroupId)))
        .WillOnce(testing::Return(group));

    auto result = groupDelegate->UpdateTeams(validTournamentId, validGroupId, teams);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::UNPROCESSABLE_ENTITY);
}

