


import uuid
import requests

BASE_URL = "http://localhost:8080"
TOTAL_TEAMS = 32

HEADERS = {"Content-Type": "application/json"}


def extract_id(response):
    """
    Try Location header first; if not present, fall back to plain body.
    Works for your API because team creation returns raw UUID.
    """
    location = response.headers.get("Location") or response.headers.get("location")
    if location:
        return location.strip()
    return response.text.strip()


def create_tournament():
    data = {"name": f"Tournament - {uuid.uuid4()}"}
    print("Creating tournament...")
    resp = requests.post(f"{BASE_URL}/tournaments", json=data, headers=HEADERS)
    resp.raise_for_status()
    tournament_id = extract_id(resp)
    print(f"  -> Tournament created with id: {tournament_id}")
    return tournament_id


def create_group(tournament_id):
    data = {"name": f"Group - {uuid.uuid4()}"}
    print(f"Creating group in {tournament_id}...")
    resp = requests.post(
        f"{BASE_URL}/tournaments/{tournament_id}/groups",
        json=data,
        headers=HEADERS,
    )
    resp.raise_for_status()
    group_id = extract_id(resp)
    print(f"  -> Group created with id: {group_id}")
    return group_id


def create_team(i, group_suffix):
    # ------------------------------
    # NAME FORMAT: <last4_of_group> + "T" + number
    # ------------------------------
    team_name = f"{group_suffix}T{i}"

    data = {"name": team_name}

    print(f"Creating team {i} ({team_name})...")
    resp = requests.post(f"{BASE_URL}/teams", json=data, headers=HEADERS)
    resp.raise_for_status()

    team_id = resp.text.strip()  # plain UUID
    print(f"  -> Team {i} created with id: {team_id}")
    return team_id


def add_team_to_group(tournament_id, group_id, team_id, i):
    payload = [{"id": team_id}]
    print(f"Adding team {i} to group {group_id}...")
    resp = requests.patch(
        f"{BASE_URL}/tournaments/{tournament_id}/groups/{group_id}/teams",
        json=payload,
        headers=HEADERS,
    )
    resp.raise_for_status()
    print(f"  -> Team {i} added.\n")


def main():
    # 1) create tournament
    tournament_id = create_tournament()

    # 2) create group
    group_id = create_group(tournament_id)

    # extract last 4 characters for team naming
    group_suffix = group_id[-4:]
    print(f"\nTeam name prefix will use group suffix: {group_suffix}\n")

    # 3) create and add teams
    for i in range(1, TOTAL_TEAMS + 1):
        team_id = create_team(i, group_suffix)

        if i == TOTAL_TEAMS:
            input("\nLast team created. Press ENTER to add it to the group...\n")

        add_team_to_group(tournament_id, group_id, team_id, i)

    print("All teams created and added!")
    print(f"Tournament ID: {tournament_id}")


if __name__ == "__main__":
    main()