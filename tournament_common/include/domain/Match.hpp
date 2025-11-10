#ifndef DOMAIN_MATCH_HPP
#define DOMAIN_MATCH_HPP

#include <string>
namespace domain {
    enum class Winner { HOME, VISITOR  };
    struct Score {
        int homeTeamScore;
        int visitorTeamScore;
        [[nodiscard]] Winner GetWinner() const {
            if (visitorTeamScore < homeTeamScore) {
                return Winner::HOME;
            }
            return Winner::VISITOR;
        }
    };
    class Match {
        /* data */
        std::string id;
        std::string tournamentId;
        std::string homeTeamId;
        std::string visitorTeamId;
        Score score;

        //winner's next match
        //loser's next match

    public:
        Match(/* args */){}

        [[nodiscard]] std::string Id() const {
            return  id;
        }

        std::string& Id() {
            return  id;
        }

        [[nodiscard]] std::string TournamentId() const {
            return  tournamentId;
        }

        [[nodiscard]] std::string & TournamentId() {
            return  tournamentId;
        }

        [[nodiscard]] std::string HomeTeamId() const {
            return homeTeamId;
        }
        std::string & HomeTeamId() {
            return homeTeamId;
        }

        [[nodiscard]] std::string VisitorTeamId() const {
            return visitorTeamId;
        }

        std::string & VisitorTeamId() {
            return visitorTeamId;
        }

        Score & MatchScore() {
            return score;
        }

        [[nodiscard]] Score MatchScore() const {
            return score;
        }
    };
    
}
#endif