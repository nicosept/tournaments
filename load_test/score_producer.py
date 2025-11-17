import sys
import time
import requests

BASE_URL = "http://localhost:8080"
HEADERS = {"Content-Type": "application/json"}

# If True, don't touch matches that already have a non-zero score
SKIP_IF_ALREADY_SCORED = True

# Optional safety cap (avoid infinite loops if something is wrong)
MAX_ROUNDS = 50


def fetch_matches(tournament_id):
    resp = requests.get(
        f"{BASE_URL}/tournaments/{tournament_id}/matches",
        headers=HEADERS,
    )
    resp.raise_for_status()
    return resp.json()


def is_playable(match):

    score = match.get("score") or {}
    home_score = score.get("homeTeamScore", 0)
    visitor_score = score.get("visitorTeamScore", 0)

    if SKIP_IF_ALREADY_SCORED and (home_score != 0 or visitor_score != 0):
        return False

    # For initial W0–W15 you already have these IDs.
    # Later rounds only get them once the tree advances.
    has_teams = "homeTeamId" in match and "visitorTeamId" in match
    if not has_teams:
        return False

    return True


def generate_score(match):
    return 0, 1


def update_match_score(tournament_id, match_id, home_score, visitor_score):
    url = f"http://localhost:8080/tournaments/{tournament_id}/matches/{match_id}"
    payload = {
        "score": {
            "homeTeamScore": home_score,
            "visitorTeamScore": visitor_score,
        }
    }
    resp = requests.patch(url, json=payload, headers=HEADERS)
    if resp.status_code >= 400:
        print(f"  Error updating: {resp.status_code} {resp.text}")
    else:
        print(f"  Updated to {home_score}-{visitor_score}")
    resp.raise_for_status()
    return resp


def main():
    # Change this if you want a hard-coded default
    tournament_id = "b3c26fcf-e92d-4bd5-9e41-9a8e6405ad18"
    print(f"Simulating tournament {tournament_id}")


    round_idx = 1
    while True:
        matches = fetch_matches(tournament_id)
        playable = [m for m in matches if is_playable(m)]
        if not playable:
            print("\nNo more playable unscored matches. Stopping simulation.")
            
            # Show matches waiting for teams
            waiting = [m for m in matches if not m.get("homeTeamId") or not m.get("visitorTeamId")]
            unscored_waiting = [m for m in waiting if m.get("score", {}).get("homeTeamScore", 0) == 0 and m.get("score", {}).get("visitorTeamScore", 0) == 0]
            if unscored_waiting:
                print(f"\n{len(unscored_waiting)} matches waiting for teams:")
                for m in sorted(unscored_waiting, key=lambda x: x.get("name", ""))[:10]:
                    name = m.get("name", "?")
                    h = "Set" if m.get("homeTeamId") else "TBD"
                    v = "Set" if m.get("visitorTeamId") else "TBD"
                    print(f"  {name}: home={h}, visitor={v}")
            break

        def sort_key(m):
            name = m.get("name", "")
            prefix = name[0] if name else "?"
            digits = "".join(ch for ch in name if ch.isdigit())
            idx = int(digits) if digits else 0
            order_map = {"W": 0, "L": 1, "F": 2}
            return (order_map.get(prefix, 9), idx)

        playable.sort(key=sort_key)
        print(f"\nROUND {round_idx}: {len(playable)} playable matches")
        print("-" * 30)
        for m in playable:
            name = m.get("name", "?")
            match_id = m.get("id", "?")
            score = m.get("score") or {}
            curr_h = score.get("homeTeamScore", 0)
            curr_v = score.get("visitorTeamScore", 0)
            home_id = m.get("homeTeamId", "?")[:8]
            visitor_id = m.get("visitorTeamId", "?")[:8]
            print(f"Match {name:4} (ID: {match_id}) [H:{home_id} vs V:{visitor_id}] current score {curr_h}-{curr_v}", end="")
            home_score, visitor_score = generate_score(m)
            update_match_score(tournament_id, m["id"], home_score, visitor_score)
            time.sleep(0.1)
        round_idx += 1
        
        if round_idx > MAX_ROUNDS:
            print(f"\nHit MAX_ROUNDS safety limit ({MAX_ROUNDS}). Stopping.")
            break

    print("\n" + "="*60)
    print("TOURNAMENT COMPLETE")
    print("="*60)
    
    # Show final summary
    final_matches = fetch_matches(tournament_id)
    finals = [m for m in final_matches if m.get("name", "").startswith("F")]
    if finals:
        print("\nFINAL MATCHES SUMMARY:")
        for m in finals:
            name = m["name"]
            score = m.get("score", {})
            h_score = score.get("homeTeamScore", 0)
            v_score = score.get("visitorTeamScore", 0)
            h_id = m.get("homeTeamId", "TBD")
            v_id = m.get("visitorTeamId", "TBD")
            if h_id != "TBD":
                h_id = h_id[:8]
            if v_id != "TBD":
                v_id = v_id[:8]
            print(f"  {name}: [Home: {h_id}] {h_score}-{v_score} [Visitor: {v_id}]")
    
    print("\nSimulation finished.")


if __name__ == "__main__":
    main()