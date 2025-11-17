#!/bin/bash

# 1. Create tournament
TOURNAMENT=$(curl -s -X POST http://localhost:8080/tournaments \
  -H "Content-Type: application/json" \
  -d '{"name":"Test Tournament"}' \
  -i | grep -i location | awk '{print $2}' | tr -d '\r')

echo "Tournament: $TOURNAMENT"

# 2. Create group
GROUP=$(curl -s -X POST http://localhost:8080$TOURNAMENT/groups \
  -H "Content-Type: application/json" \
  -d '{"name":"Group A"}' \
  -i | grep -i location | awk '{print $2}' | tr -d '\r')

echo "Group: $GROUP"

# 3. Create 32 teams and collect IDs
TEAM_IDS=()
for i in $(seq -f "%02g" 1 32); do
  TEAM=$(curl -s -X POST http://localhost:8080/teams \
    -H "Content-Type: application/json" \
    -d "{\"name\":\"Team $i\"}" \
    -i | grep -i location | awk '{print $2}' | tr -d '\r')
  TEAM_IDS+=("$TEAM")
  echo "Team $i: $TEAM"
done

# 4. Build JSON array for PATCH
JSON="["
for i in "${!TEAM_IDS[@]}"; do
  JSON+="{\"id\":\"${TEAM_IDS[$i]}\"}"
  if [ $i -lt $((${#TEAM_IDS[@]} - 1)) ]; then
    JSON+=","
  fi
done
JSON+="]"

# 5. Add all teams to group
curl -X PATCH http://localhost:8080$TOURNAMENT$GROUP/teams \
  -H "Content-Type: application/json" \
  -d "$JSON"

echo ""
echo "✓ Done! Check consumer logs for match generation."