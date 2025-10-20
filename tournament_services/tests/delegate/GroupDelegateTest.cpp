#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <expected>
#include <variant>

#include "domain/Group.hpp"
#include "delegate/GroupDelegate.hpp"
#include "persistence/repository/IGroupRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "persistence/repository/TeamRepository.hpp"
#include "exception/Duplicate.hpp"
#include "exception/NotFound.hpp"
#include "exception/InvalidFormat.hpp"
#include "delegate/GroupDelegate.hpp" // Para GroupDelegateError

//Estos using son para no tener que escribir testing:: todo el tiempo
using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Throw;
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::HasSubstr;

//Este es el mockrepo y es para poder mockear las funciones del repositorio de Group

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

class GroupDelegateTest : public ::testing::Test {
    protected:
    std::shared_ptr<MockTournamentRepository> mockTournamentRepository;
    std::shared_ptr<MockGroupRepository> mockGroupRepository;
    std::shared_ptr<MockTeamRepository> mockTeamRepository;
    std::shared_ptr<GroupDelegate> groupDelegate;

    void SetUp() override {
        mockTournamentRepository = std::make_shared<MockTournamentRepository>();
        mockGroupRepository = std::make_shared<MockGroupRepository>();
        mockTeamRepository = std::make_shared<MockTeamRepository>();
        groupDelegate = std::make_shared<GroupDelegate>(mockTournamentRepository, mockGroupRepository, mockTeamRepository);
    }
};

//Estos son los tests para getAllGroups
TEST_F(GroupDelegateTest, GetGroups_ReturnsMultipleGroups) {
    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    std::vector<std::shared_ptr<domain::Group>> groups = {
        std::make_shared<domain::Group>("Group 1", "group-1"),
        std::make_shared<domain::Group>("Group 2", "group-2"),
        std::make_shared<domain::Group>("Group 3", "group-3")
    };
    groups[0]->TournamentId() = "tournament-id";
    groups[1]->TournamentId() = "tournament-id";
    groups[2]->TournamentId() = "tournament-id";

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentId(Eq(std::string("tournament-id"))))
        .WillOnce(Return(groups));

    auto result = groupDelegate->GetGroups("tournament-id");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3);
    EXPECT_EQ((*result)[0]->Id(), "group-1");
    EXPECT_EQ((*result)[1]->Id(), "group-2");
    EXPECT_EQ((*result)[2]->Id(), "group-3");
}

TEST_F(GroupDelegateTest, GetGroups_ReturnsEmptyList) {
    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    std::vector<std::shared_ptr<domain::Group>> emptyGroups;

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentId(Eq(std::string("tournament-id"))))
        .WillOnce(Return(emptyGroups));

    auto result = groupDelegate->GetGroups("tournament-id");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST_F(GroupDelegateTest, GetGroups_NonExistentTournamentId_ReturnsError) {
    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("non-existent-tournament"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->GetGroups("non-existent-tournament");

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<NotFoundException>(result.error()));
    EXPECT_EQ(std::get<NotFoundException>(result.error()).what(), std::string("Tournament not found"));
}

TEST_F(GroupDelegateTest, GetGroups_InvalidTournamentIdFormat_ReturnsError) {
    auto result = groupDelegate->GetGroups("");

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<InvalidFormatException>(result.error()));
    EXPECT_EQ(std::get<InvalidFormatException>(result.error()).what(), std::string("Invalid tournament ID format"));
}

//Estos tests son para getGroup con id
TEST_F(GroupDelegateTest, GetGroup_ValidIds_ReturnsGroup) {
    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    auto group = std::make_shared<domain::Group>("Group 1", "group-1");
    group->TournamentId() = "tournament-id";

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(Eq(std::string("tournament-id")), Eq(std::string("group-1"))))
        .WillOnce(Return(group));

    auto result = groupDelegate->GetGroup("tournament-id", "group-1");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->Id(), "group-1");
    EXPECT_EQ((*result)->Name(), "Group 1");
}

TEST_F(GroupDelegateTest, GetGroup_NonExistentGroupId_ReturnsNull) {
    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(Eq(std::string("tournament-id")), Eq(std::string("non-existent-group"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->GetGroup("tournament-id", "non-existent-group");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), nullptr);
}

TEST_F(GroupDelegateTest, GetGroup_NonExistentTournament_ReturnsError) {
    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("non-existent-tournament"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->GetGroup("non-existent-tournament", "group-1");

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<NotFoundException>(result.error()));
}

TEST_F(GroupDelegateTest, GetGroup_DatabaseError_ReturnsError) {
    auto tournament = std::make_shared<domain::Tournament>("Test Tournament");
    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(_, _))
        .WillOnce(Throw(std::runtime_error("Database connection error")));

    auto result = groupDelegate->GetGroup("tournament-id", "group-1");

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::runtime_error>(result.error()));
    EXPECT_THAT(std::get<std::runtime_error>(result.error()).what(), HasSubstr("Error when reading from DB"));
}

//Estos tETSst son para createGroup
TEST_F(GroupDelegateTest, CreateGroup_ValidData_ReturnsSuccess) {
    domain::Group group{"Group 1", ""};
    auto tournament = std::make_shared<domain::Tournament>("Tournament Name");
    tournament->Id() = "tournament-id";

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-id"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, Create(_))
        .WillOnce(Return("new-group-id"));

    auto result = groupDelegate->CreateGroup("tournament-id", group);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "new-group-id");
}

TEST_F(GroupDelegateTest, CreateGroup_NonExistentTournamentId_ReturnsError) {
    domain::Group group{"Group 1", "group-1"};
    
    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("non-existent-tournament"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->CreateGroup("non-existent-tournament", group);
    
    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<NotFoundException>(result.error()));
}

TEST_F(GroupDelegateTest, CreateGroup_ValidData_ValidatesGroupPassedToRepository) {
    domain::Group group{"Group A", "group-a"};
    auto tournament = std::make_shared<domain::Tournament>("Tournament 1");
    tournament->Id() = "tournament-123";

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-123"))))
        .WillOnce(Return(tournament));

    EXPECT_CALL(*mockGroupRepository, Create(_))
        .WillOnce(DoAll(
            WithArg<0>(Invoke([](const domain::Group& g) {
                EXPECT_EQ(g.TournamentId(), "tournament-123");
                EXPECT_EQ(g.Name(), "Group A");
            })),
            Return("generated-group-id")
        ));

    auto result = groupDelegate->CreateGroup("tournament-123", group);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "generated-group-id");
}

TEST_F(GroupDelegateTest, CreateGroup_DuplicateGroup_ReturnsError) {
    domain::Group group{"Group A", "group-a"};
    auto tournament = std::make_shared<domain::Tournament>("Tournament 1");
    tournament->Id() = "tournament-123";

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-123"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, Create(_))
        .WillOnce(Throw(DuplicateException("Group already exists")));

    auto result = groupDelegate->CreateGroup("tournament-123", group);

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<DuplicateException>(result.error()));
}

//Estos tests son para updategroup
TEST_F(GroupDelegateTest, UpdateGroup_ValidData_ReturnsSuccess) {
    domain::Group inputGroup{"Updated Group", ""};
    auto tournament = std::make_shared<domain::Tournament>("Tournament 1");
    auto existingGroup = std::make_shared<domain::Group>("Existing Group", "group-789");

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-456"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(Eq(std::string("tournament-456")), Eq(std::string("group-789"))))
        .WillOnce(Return(existingGroup));
    EXPECT_CALL(*mockGroupRepository, Update(_))
        .WillOnce(WithArg<0>(Invoke([](const domain::Group& g) {
            EXPECT_EQ(g.Id(), "group-789");
            EXPECT_EQ(g.TournamentId(), "tournament-456");
            EXPECT_EQ(g.Name(), "Updated Group");
        })));

    auto result = groupDelegate->UpdateGroup("tournament-456", "group-789", inputGroup);

    ASSERT_TRUE(result.has_value());
}

TEST_F(GroupDelegateTest, UpdateGroup_GroupNotFound_ReturnsError) {
    domain::Group inputGroup{"Updated Group", ""};
    auto tournament = std::make_shared<domain::Tournament>("Tournament 1");

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("tournament-456"))))
        .WillOnce(Return(tournament));
    EXPECT_CALL(*mockGroupRepository, FindByTournamentIdAndGroupId(Eq(std::string("tournament-456")), Eq(std::string("non-existent-group"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->UpdateGroup("tournament-456", "non-existent-group", inputGroup);

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<NotFoundException>(result.error()));
    EXPECT_EQ(std::get<NotFoundException>(result.error()).what(), std::string("Group not found"));
}

TEST_F(GroupDelegateTest, UpdateGroup_TournamentNotFound_ReturnsError) {
    domain::Group inputGroup{"Updated Group", "group-id"};

    EXPECT_CALL(*mockTournamentRepository, ReadById(Eq(std::string("non-existent-tournament"))))
        .WillOnce(Return(nullptr));

    auto result = groupDelegate->UpdateGroup("non-existent-tournament", "group-id", inputGroup);

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<NotFoundException>(result.error()));
    EXPECT_EQ(std::get<NotFoundException>(result.error()).what(), std::string("Tournament not found"));
}