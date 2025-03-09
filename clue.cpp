#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <map>
#include <curl/curl.h> // Required for making HTTP requests
#include <sstream>
#include <stdexcept>
#include <cstring>

// INSTRUCTIONS:
// 1. Install libcurl:  sudo apt-get install libcurl4-openssl-dev
// 2. Compile with: g++ clue.cpp -lcurl -o clue

struct Player {
    std::string name;
    std::string character; // e.g., "Miss Scarlet"
    std::vector<std::string> hand; // Cards in the player's hand
    bool eliminated = false;
    std::string location; // Current location of the player (room or hallway)
};

struct Card {
    std::string type; // "character", "weapon", or "room"
    std::string name;
};

struct Room {
    std::string name;
    // Add any room-specific properties here, like connections to other rooms
};

struct Board {
    std::map<std::string, std::vector<std::string>> adjacencyList;

    Board(const std::vector<Card>& rooms) {
        // Initialize the board layout (room connections)
        // Hallways are represented as strings like "Hall-Study"

        std::vector<std::string> roomNames;
        for (const auto& room : rooms) {
            roomNames.push_back(room.name);
        }

        // Assuming the first 9 rooms are the main rooms in the Clue layout
        if (roomNames.size() >= 9) {
            adjacencyList = {
                {"Hall", {"Hall-" + roomNames[1], "Hall-" + roomNames[7], "Hall-" + roomNames[8]}},
                {roomNames[1], {"Hall-" + roomNames[1], roomNames[1] + "-" + roomNames[2]}},
                {roomNames[2], {roomNames[1] + "-" + roomNames[2], roomNames[2] + "-" + roomNames[3]}},
                {roomNames[3], {roomNames[2] + "-" + roomNames[3], roomNames[3] + "-" + roomNames[4]}},
                {roomNames[4], {roomNames[3] + "-" + roomNames[4], roomNames[4] + "-" + roomNames[5]}},
                {roomNames[5], {roomNames[4] + "-" + roomNames[5], roomNames[5] + "-" + roomNames[6]}},
                {roomNames[6], {roomNames[5] + "-" + roomNames[6], roomNames[6] + "-" + roomNames[7]}},
                {roomNames[7], {"Hall-" + roomNames[7], roomNames[6] + "-" + roomNames[7], roomNames[7] + "-" + roomNames[8]}},
                {roomNames[8], {"Hall-" + roomNames[8], roomNames[7] + "-" + roomNames[8]}},

                // Hallways connect two rooms
                {"Hall-" + roomNames[1], {"Hall", roomNames[1]}},
                {"Hall-" + roomNames[7], {"Hall", roomNames[7]}},
                {"Hall-" + roomNames[8], {"Hall", roomNames[8]}},
                {roomNames[1] + "-" + roomNames[2], {roomNames[1], roomNames[2]}},
                {roomNames[2] + "-" + roomNames[3], {roomNames[2], roomNames[3]}},
                {roomNames[3] + "-" + roomNames[4], {roomNames[3], roomNames[4]}},
                {roomNames[4] + "-" + roomNames[5], {roomNames[4], roomNames[5]}},
                {roomNames[5] + "-" + roomNames[6], {roomNames[5], roomNames[6]}},
                {roomNames[6] + "-" + roomNames[7], {roomNames[6], roomNames[7]}},
                {roomNames[7] + "-" + roomNames[8], {roomNames[7], roomNames[8]}}
            };
        } else {
            std::cerr << "Not enough rooms to initialize the board." << std::endl;
        }
    }

    // Function to check if a move from one location to another is valid
    bool isValidMove(const std::string& start, const std::string& destination) {
        auto it = adjacencyList.find(start);
        if (it != adjacencyList.end()) {
            const std::vector<std::string>& neighbors = it->second;
            return std::find(neighbors.begin(), neighbors.end(), destination) != neighbors.end();
        }
        return false;
    }

    // Function to display the current board state (player locations)
    void displayBoard(const std::vector<Player>& players) {
        std::cout << "\nCurrent Board State:\n";
        for (const auto& room : adjacencyList) {
            std::cout << room.first << ": ";
            for (const Player& player : players) {
                if (player.location == room.first) {
                    std::cout << player.name << " (" << player.character << ") ";
                }
            }
            std::cout << "\n";
        }
    }
};

struct Checklist {
    std::map<std::string, bool> items; // Card name -> isKnown

    Checklist(const std::vector<Card>& characters, const std::vector<Card>& weapons, const std::vector<Card>& rooms) {
        for (const auto& card : characters) {
            items[card.name] = false;
        }
        for (const auto& card : weapons) {
            items[card.name] = false;
        }
        for (const auto& card : rooms) {
            items[card.name] = false;
        }
    }

    void display(const std::vector<Card>& characters, const std::vector<Card>& weapons, const std::vector<Card>& rooms) {
        std::cout << "\n--- Checklist ---\n";
        std::cout << "Characters:\n";
        for (const auto& card : characters) {
            std::cout << card.name << ": " << (items[card.name] ? "Known" : "Unknown") << "\n";
        }
        std::cout << "Weapons:\n";
        for (const auto& card : weapons) {
            std::cout << card.name << ": " << (items[card.name] ? "Known" : "Unknown") << "\n";
        }
        std::cout << "Rooms:\n";
        for (const auto& card : rooms) {
            std::cout << card.name << ": " << (items[card.name] ? "Known" : "Unknown") << "\n";
        }
    }

    void markKnown(const std::string& cardName) {
        items[cardName] = true;
    }
};


// Global lists of characters, weapons, and rooms
std::vector<Card> characters;
std::vector<Card> weapons;
std::vector<Card> rooms;

std::vector<Player> players;
Board board(rooms); // Instantiate the board
std::vector<Checklist> checklists;

// Callback function to write the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Function to make a request to the LLM API
std::string getLLMResponse(const std::string& prompt, double temperature) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9090/v1/chat/completions");

        // Prepare the JSON payload
        std::string data = R"({"model": "llama-3.2-3b-it-q8_0", "messages": [{"role": "system", "content": "You are a helpful assistant."}, {"role": "user", "content": ")" + prompt + R"("}], "temperature": )" + std::to_string(temperature) + R"(})";

        // Set the request type to POST
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        // Set the data length
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());

        // Set the callback function to write the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set the content type to application/json
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
        }

        // Always cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    } else {
        throw std::runtime_error("curl_easy_init() failed");
    }
    curl_global_cleanup();

    return response;
}

// Function to get a list of rooms from the LLM
std::vector<std::string> getRoomsFromLLM() {
    std::string prompt = "List 9 random rooms suitable for a clue-like game, but not Hall, Lounge, Dining Room, Kitchen, Ballroom, Conservatory, Billiard Room, Library, or Study, separated by commas. Give me only the comma separated list, nothing else.";
    double temperature = 1.0;
    std::string response = getLLMResponse(prompt, temperature);

    // Extract the content from the JSON response
    size_t contentStart = response.find("\"content\":\"");
    if (contentStart == std::string::npos) {
        throw std::runtime_error("Could not find 'content' in LLM response.");
    }
    contentStart += strlen("\"content\":\"");

    // Look for the end of the content, allowing for variations in the closing sequence
    size_t contentEnd = response.find("\"}]", contentStart);
    if (contentEnd == std::string::npos) {
        contentEnd = response.find("\"", contentStart); // Try to find the next quote
        if (contentEnd == std::string::npos) {
            throw std::runtime_error("Could not find end of 'content' in LLM response.");
        }
    }

    std::string content = response.substr(contentStart, contentEnd - contentStart);

    std::vector<std::string> rooms;
    std::stringstream ss(content);
    std::string room;
    while (std::getline(ss, room, ',')) {
        // Remove leading/trailing whitespace
        room.erase(0, room.find_first_not_of(" \t\n\r"));
        room.erase(room.find_last_not_of(" \t\n\r") + 1);
        rooms.push_back(room);
    }
    return rooms;
}

// Function to get a list of weapons from the LLM
std::vector<std::string> getWeaponsFromLLM() {
    std::string prompt = "List 6 random weapons suitable for a clue-like game, but not Candlestick, Dagger, Lead Pipe, Revolver, Rope, or Wrench, separated by commas. Give me only the comma separated list, nothing else.";
    double temperature = 1.0;
    std::string response = getLLMResponse(prompt, temperature);

    // Extract the content from the JSON response
    size_t contentStart = response.find("\"content\":\"");
    if (contentStart == std::string::npos) {
        throw std::runtime_error("Could not find 'content' in LLM response.");
    }
    contentStart += strlen("\"content\":\"");

    // Look for the end of the content, allowing for variations in the closing sequence
    size_t contentEnd = response.find("\"}]", contentStart);
    if (contentEnd == std::string::npos) {
        contentEnd = response.find("\"", contentStart); // Try to find the next quote
        if (contentEnd == std::string::npos) {
            throw std::runtime_error("Could not find end of 'content' in LLM response.");
        }
    }

    std::string content = response.substr(contentStart, contentEnd - contentStart);

    std::vector<std::string> weapons;
    std::stringstream ss(content);
    std::string weapon;
    while (std::getline(ss, weapon, ',')) {
        // Remove leading/trailing whitespace
        weapon.erase(0, weapon.find_first_not_of(" \t\n\r"));
        weapon.erase(weapon.find_last_not_of(" \t\n\r") + 1);
        weapons.push_back(weapon);
    }
    return weapons;
}

// Function to get a list of characters from the LLM
std::vector<std::string> getCharactersFromLLM() {
    std::string prompt = "List 6 random characters suitable for a clue-like game, but not Miss Scarlet, Colonel Mustard, Mrs. White, Mr. Green, Mrs. Peacock, or Professor Plum, separated by commas. Give me only the comma separated list, nothing else.";
    double temperature = 1.0;
    std::string response = getLLMResponse(prompt, temperature);

    // Extract the content from the JSON response
    size_t contentStart = response.find("\"content\":\"");
    if (contentStart == std::string::npos) {
        throw std::runtime_error("Could not find 'content' in LLM response.");
    }
    contentStart += strlen("\"content\":\"");

    // Look for the end of the content, allowing for variations in the closing sequence
    size_t contentEnd = response.find("\"}]", contentStart);
    if (contentEnd == std::string::npos) {
        contentEnd = response.find("\"", contentStart); // Try to find the next quote
        if (contentEnd == std::string::npos) {
            throw std::runtime_error("Could not find end of 'content' in LLM response.");
        }
    }

    std::string content = response.substr(contentStart, contentEnd - contentStart);

    std::vector<std::string> characters;
    std::stringstream ss(content);
    std::string character;
    while (std::getline(ss, character, ',')) {
        // Remove leading/trailing whitespace
        character.erase(0, character.find_first_not_of(" \t\n\r"));
        character.erase(character.find_last_not_of(" \t\n\r") + 1);
        characters.push_back(character);
    }
    return characters;
}

void initializeGame(int numPlayers) {
    // Get lists of rooms, weapons, and characters from LLM
    std::vector<std::string> llmRooms = getRoomsFromLLM();
    std::vector<std::string> llmWeapons = getWeaponsFromLLM();
    std::vector<std::string> llmCharacters = getCharactersFromLLM();

    // Create the card vectors using the LLM-generated lists
    rooms.clear();
    for (const auto& roomName : llmRooms) {
        rooms.push_back({"room", roomName});
    }

    weapons.clear();
    for (const auto& weaponName : llmWeapons) {
        weapons.push_back({"weapon", weaponName});
    }

    characters.clear();
    for (const auto& characterName : llmCharacters) {
        characters.push_back({"character", characterName});
    }

    // Create the deck of cards
    std::vector<Card> deck;
    deck.insert(deck.end(), characters.begin(), characters.end());
    deck.insert(deck.end(), weapons.begin(), weapons.end());
    deck.insert(deck.end(), rooms.begin(), rooms.end());

    // Shuffle the deck (you'll need <algorithm> for std::random_shuffle or std::shuffle)
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);

    // Select the solution (one character, one weapon, one room)
    Card solutionCharacter = deck[0];
    Card solutionWeapon = deck[1];
    Card solutionRoom = deck[2];

    deck.erase(deck.begin(), deck.begin() + 3); // Remove solution cards from the deck

    // Create players
    players.clear();
    checklists.clear();
    for (int i = 0; i < numPlayers; ++i) {
        Player newPlayer;
        std::cout << "Enter name for player " << i + 1 << ": ";
        std::cin >> newPlayer.name;
        newPlayer.character = characters[i % characters.size()].name; // Assign characters in order for now

        // Assign initial locations (starting rooms) - use the first 3 rooms
        if (llmRooms.size() >= 3) {
            newPlayer.location = "Hall-" + llmRooms[i % 3]; // Assign to a hallway
        } else {
            newPlayer.location = "Hall"; // Default if not enough rooms
        }
        
        players.push_back(newPlayer);
        checklists.emplace_back(characters, weapons, rooms); // Initialize checklist for each player
    }

    board = Board(rooms); // Re-initialize the board with the new rooms

    // Deal the remaining cards to the players
    int playerIndex = 0;
    for (Card& card : deck) {
        players[playerIndex].hand.push_back(card.name);
        checklists[playerIndex].markKnown(card.name); // Mark cards in hand as known
        playerIndex = (playerIndex + 1) % numPlayers;
    }

    board.displayBoard(players); // Display initial board state
}

// Function to get the player's move
void getPlayerMove(Player& player, int playerIndex) {
    std::cout << "\n" << player.name << ", what would you like to do?\n";
    std::cout << "1. Move\n";
    std::cout << "2. Make a suggestion\n";
    std::cout << "3. Make an accusation\n";
    std::cout << "4. Show Checklist\n";
    std::cout << "5. End turn\n";

    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Consume the newline character

    switch (choice) {
        case 1: { // Move
            std::cout << "Available moves: ";
            if (board.adjacencyList.count(player.location)) {
                for (const std::string& neighbor : board.adjacencyList[player.location]) {
                    std::cout << neighbor << ", ";
                }
            } else {
                std::cout << "No available moves from this location.";
            }
            std::cout << std::endl;

            std::cout << "Enter the location you want to move to: ";
            std::string newLocation;
            std::getline(std::cin, newLocation);

            if (board.isValidMove(player.location, newLocation)) {
                player.location = newLocation;
                std::cout << "You moved to " << newLocation << std::endl;
            } else {
                std::cout << "Invalid move.\n";
            }
            break;
        }
        case 2: { // Suggestion
            std::cout << "Make a suggestion (Character, Weapon, Room):\n";
            std::string character, weapon, room;

            std::cout << "Character: ";
            std::getline(std::cin, character);
            std::cout << "Weapon: ";
            std::getline(std::cin, weapon);
            std::cout << "Room: ";
            std::getline(std::cin, room);

            std::cout << "You suggested it was " << character << " with the " << weapon << " in the " << room << std::endl;
            // TODO: Implement disproving the suggestion
            break;
        }
        case 3: { // Accusation
            std::cout << "Accusation not implemented yet.\n";
            break;
        }
        case 4: { // Show Checklist
            checklists[playerIndex].display(characters, weapons, rooms);
            break;
        }
        case 5: { // End turn
            std::cout << "Ending turn.\n";
            break;
        }
        default: {
            std::cout << "Invalid choice.\n";
            break;
        }
    }
}

void playGame() {
    bool gameWon = false;
    while (!gameWon) {
        for (size_t i = 0; i < players.size(); ++i) {
            if (!players[i].eliminated) {
                std::cout << "\n" << players[i].name << "'s turn (" << players[i].character << "):\n";
                std::cout << "Current location: " << players[i].location << "\n";

                // Display player's hand (for debugging)
                std::cout << "Your hand: ";
                for (const std::string& card : players[i].hand) {
                    std::cout << card << ", ";
                }
                std::cout << std::endl;

                getPlayerMove(players[i], i); // Get the player's move
            }
            board.displayBoard(players); // Display board state after each turn
        }

        // Check for a winner (if only one player is not eliminated)
        int activePlayers = 0;
        for (const Player& player : players) {
            if (!player.eliminated) {
                activePlayers++;
            }
        }
        if (activePlayers <= 1) {
            gameWon = true;
            std::cout << "Game over!\n";
            // Determine the winner (or declare a tie)
        }
    }
}

int main() {
    int numPlayers;
    std::cout << "Enter the number of players (2-6): ";
    std::cin >> numPlayers;

    initializeGame(numPlayers);
    playGame();

    return 0;
}
