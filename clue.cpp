#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <random>
#include <map>
#include <curl/curl.h> // Required for making HTTP requests
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <tuple>
#include <memory> // Include the <memory> header
#include <sys/stat.h> // For creating directories
#include <libgen.h> // For dirname

// INSTRUCTIONS:
// 1. Install libcurl:  sudo apt-get install libcurl4-openssl-dev
// 2. Compile with: g++ clue.cpp -lcurl -o clue

// Function to create a directory (including parent directories)
bool createDirectory(const std::string& path) {
    // Use stat to check if the directory already exists
    struct stat info;
    if (stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR)) {
        // Directory exists
        return true;
    }

    // Use `mkdir -p` to create the directory and any necessary parent directories
    std::string command = "mkdir -p \"" + path + "\"";
    int result = system(command.c_str());

    if (result == 0) {
        std::cout << "Directory created successfully: " << path << std::endl;
        return true;
    } else {
        std::cerr << "Error creating directory " << path << std::endl;
        return false;
    }
}

struct Player {
    std::string name;
    std::string character; // e.g., "Miss Scarlet"
    std::vector<std::string> hand; // Cards in the player's hand
    bool eliminated = false;
    int row;
    int col; // Current location of the player (row and column)
};

struct Card {
    std::string type; // "character", "weapon", or "room"
    std::string name;
};

struct Room {
    std::string name;
    // Add any room-specific properties here, like connections to other rooms
};

// Define a struct to represent each cell on the grid
struct GridCell {
    std::string type; // "empty", "hallway", or "room"
    std::string name; // Name of the room or hallway, if applicable
};

struct Board {
    int rows;
    int cols;
    std::vector<std::vector<GridCell>> grid;

    Board(const std::vector<Card>& rooms) : rows(25), cols(25), grid(rows, std::vector<GridCell>(cols, {"hallway", ""})) {
        // Initialize the board layout with multi-tile rooms

        // Define rooms with their positions and sizes (name, start_row, start_col, height, width)
        std::vector<std::tuple<std::string, int, int, int, int>> room_specs;
        if (rooms.size() >= 9) {
            room_specs = {
                {rooms[0].name, 1, 1, 5, 6},    // Room 1 - Top Left
                {rooms[1].name, 1, 18, 5, 6},   // Room 2 - Top Right
                {rooms[2].name, 19, 1, 5, 6},   // Room 3 - Bottom Left
                {rooms[3].name, 19, 18, 5, 6},    // Room 4 - Bottom Right
                {rooms[4].name, 7, 10, 4, 6},   // Room 5 - Middle
                {rooms[5].name, 1, 8, 5, 4},   // Room 6
                {rooms[6].name, 19, 8, 5, 4},   // Room 7
                {rooms[7].name, 13, 1, 4, 6},   // Room 8
                {rooms[8].name, 13, 18, 4, 6}    // Room 9
            };
        } else {
            std::cerr << "Not enough rooms to initialize the board." << std::endl;
            // It's important to prevent further execution if there aren't enough rooms
            throw std::runtime_error("Not enough rooms to initialize the board.");
            return;
        }

        // Place the rooms on the grid
        for (const auto& room_spec : room_specs) {
            std::string room_name = std::get<0>(room_spec);
            int start_row = std::get<1>(room_spec);
            int start_col = std::get<2>(room_spec);
            int height = std::get<3>(room_spec);
            int width = std::get<4>(room_spec);

            for (int i = start_row; i < start_row + height; ++i) {
                for (int j = start_col; j < start_col + width; ++j) {
                    if (i < rows && j < cols) {
                        grid[i][j] = {"room", room_name};
                    }
                }
            }
        }

        // Assign hallways (adjust positions as needed)
        grid[1][7] = {"hallway", "Hall-1"};
        grid[1][17] = {"hallway", "Hall-2"};
        grid[1][13] = {"hallway", "Hall-3"};
        grid[6][1] = {"hallway", "Hall-4"};
        grid[6][7] = {"hallway", "Hall-5"};
        grid[6][17] = {"hallway", "Hall-6"};
        grid[6][24] = {"hallway", "Hall-7"};
        grid[18][1] = {"hallway", "Hall-8"};
        grid[18][7] = {"hallway", "Hall-9"};
        grid[18][17] = {"hallway", "Hall-10"};
        grid[18][24] = {"hallway", "Hall-11"};
        grid[19][7] = {"hallway", "Hall-12"};
        grid[19][17] = {"hallway", "Hall-13"};
        grid[13][7] = {"hallway", "Hall-14"};
        grid[13][17] = {"hallway", "Hall-15"};
    }

    // Function to check if a move from one location to another is valid
    bool isValidMove(int startRow, int startCol, int destRow, int destCol) {
        // Check if the destination is within the grid bounds
        if (destRow < 0 || destRow >= rows || destCol < 0 || destCol >= cols) {
            return false;
        }

        // Get the cell types for the start and destination
        std::string startType = grid[startRow][startCol].type;
        std::string destType = grid[destRow][destCol].type;

        // Check if the destination is a valid location (room or hallway)
        if (destType == "empty") {
            return false; // Empty cell, not a valid location
        }

        // Check if the move is adjacent (up, down, left, right)
        if (abs(startRow - destRow) + abs(startCol - destCol) != 1) {
            return false; // Not an adjacent move
        }

        // Allow movement within the same room or between adjacent hallways
        if (startType == destType) {
            if (startType == "room") {
                if (grid[startRow][startCol].name != grid[destRow][destCol].name) {
                    return false; // Moving between different rooms is not allowed
                }
                return true; // Moving within the same room is allowed
            } else if (startType == "hallway") {
                return true; // Moving between adjacent hallways is allowed
            }
        }

        // Allow moving from a room to an adjacent hallway, or vice versa
        if ((startType == "room" && destType == "hallway") || (startType == "hallway" && destType == "room")) {
            return true;
        }

        return false; // Invalid move
    }

    // Function to display the current board state (player locations)
    void displayBoard(const std::vector<Player>& players) {
        const int cellWidth = 10; // Fixed width for each cell
        std::cout << "\nCurrent Board State:\n";
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                bool playerPresent = false;
                for (size_t k = 0; k < players.size(); ++k) {
                    if (players[k].row == i && players[k].col == j) {
                        std::cout << "[" << (char)('A' + k) << std::string(cellWidth - 2, ' ') << "]";
                        playerPresent = true;
                        break;
                    }
                }
                if (!playerPresent) {
                    if (grid[i][j].type == "empty") {
                        std::cout << " . "; // Empty cell
                    } else if (grid[i][j].type == "hallway") {
                        std::cout << "[   ]"; // Hallway
						//std::cout << std::string(cellWidth - 5, ' '); // REMOVE THIS LINE
                    } else {
                        std::string roomName = grid[i][j].name;

                        // Abbreviate room name if it's too long
                        if (roomName.length() > cellWidth - 2) {
                            roomName = roomName.substr(0, cellWidth - 2);
                        }

                        // Pad the room name with spaces to center it
                        int padding = (cellWidth - 2 - roomName.length()) / 2;
                        std::string paddedRoomName = roomName;
                        paddedRoomName.insert(0, padding, ' ');
                        paddedRoomName.resize(cellWidth - 2, ' ');

                        // Print the padded room name within brackets
                        std::cout << "[" << paddedRoomName << "]";
                    }
                }
				std::cout << std::string(cellWidth - 2, ' '); // ADD THIS LINE
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
std::vector<Card> rooms = {
    {"room", "Cellar"},
    {"room", "Observatory"},
    {"room", "Theater"},
    {"room", "Garage"},
    {"room", "Studio"},
    {"room", "Pantry"},
    {"room", "Attic"},
    {"room", "Gazebo"},
    {"room", "Courtyard"}
};

Board board(rooms);

std::vector<Player> players;
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

// Helper function to validate the number of items returned from the LLM
void validateLLMResponseCount(const std::vector<std::string>& items, size_t expectedCount, const std::string& itemType) {
    if (items.size() != expectedCount) {
        throw std::runtime_error("LLM returned " + std::to_string(items.size()) + " " + itemType + ", but expected " + std::to_string(expectedCount));
    }
}

// Function to get a list of rooms from the LLM
std::vector<std::string> getRoomsFromLLM() {
    const int maxRetries = 3;
    int retryCount = 0;
    std::vector<std::string> rooms;

    while (retryCount < maxRetries) {
        std::string prompt = "List 9 random rooms suitable for a clue-like game, but not Hall, Lounge, Dining Room, Kitchen, Ballroom, Conservatory, Billiard Room, Library, or Study, separated by commas. Give me only the comma separated list, nothing else.";
        double temperature = 1.0;
        std::string response = getLLMResponse(prompt, temperature);

        // Extract the content from the JSON response
        size_t contentStart = response.find("\"content\":\"");
        if (contentStart == std::string::npos) {
            std::cerr << "Could not find 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
            continue;
        }
        contentStart += strlen("\"content\":\"");

        // Look for the end of the content, allowing for variations in the closing sequence
        size_t contentEnd = response.find("\"}]", contentStart);
        if (contentEnd == std::string::npos) {
            contentEnd = response.find("\"", contentStart); // Try to find the next quote
            if (contentEnd == std::string::npos) {
                std::cerr << "Could not find end of 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
                retryCount++;
                continue;
            }
        }

        std::string content = response.substr(contentStart, contentEnd - contentStart);

        rooms.clear();
        std::stringstream ss(content);
        std::string room;
        while (std::getline(ss, room, ',')) {
            // Remove leading/trailing whitespace
            room.erase(0, room.find_first_not_of(" \t\n\r"));
            room.erase(room.find_last_not_of(" \t\n\r") + 1);
            rooms.push_back(room);
        }

        try {
            validateLLMResponseCount(rooms, 9, "rooms");
            // If validation succeeds, break out of the loop
            break;
        } catch (const std::exception& e) {
            std::cerr << "Error validating LLM response: " << e.what() << " (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
        }
    }

    // If max retries reached, return a default set of rooms
    if (retryCount == maxRetries) {
        std::cerr << "Max retries reached for getting rooms. Using default rooms." << std::endl;
        return {"Cellar", "Observatory", "Theater", "Garage", "Studio", "Pantry", "Attic", "Gazebo", "Courtyard"};
    }

    return rooms;
}

// Helper function to execute shell commands and return the output
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

// Function to generate images for the rooms
void generateRoomImages(const std::vector<std::string>& rooms) {
    std::string rooms_dir = "images/rooms/";
    if (!createDirectory(rooms_dir)) {
        std::cerr << "Could not create rooms directory!" << std::endl;
        return;
    }

    for (const auto& room : rooms) {
        // Generate a prompt for the room description
        std::string prompt = "Describe the interior of a " + room + " in a Clue-like game setting. Be descriptive and include details about the furniture, decor, and atmosphere. Start with the room name, '" + room + ", ' and then use short, concise language punctuated with commas to describe the things that should be in the image.";
        double temperature = 1.0;
        std::string response = getLLMResponse(prompt, temperature);

        // Extract the content from the JSON response
        size_t contentStart = response.find("\"content\":\"");
        if (contentStart == std::string::npos) {
            std::cerr << "Could not find 'content' in LLM response." << std::endl;
            continue;
        }
        contentStart += strlen("\"content\":\"");

        // Look for the end of the content, allowing for variations in the closing sequence
        size_t contentEnd = response.find("\"}]", contentStart);
        if (contentEnd == std::string::npos) {
            contentEnd = response.find("\"", contentStart); // Try to find the next quote
            if (contentEnd == std::string::npos) {
                std::cerr << "Could not find end of 'content' in LLM response." << std::endl;
                continue;
            }
        }

        std::string room_description = response.substr(contentStart, contentEnd - contentStart);

        // Remove quotes and newlines from the room description
        room_description.erase(std::remove(room_description.begin(), room_description.end(), '\"'), room_description.end());
        room_description.erase(std::remove(room_description.begin(), room_description.end(), '\n'), room_description.end());
        room_description.erase(std::remove(room_description.begin(), room_description.end(), '\\'), room_description.end());

        std::string filename = rooms_dir + room + ".png";
        // Escape the quotes in the room description
        std::string escaped_room_description = room_description;
        size_t pos = 0;
        while ((pos = escaped_room_description.find("\"", pos)) != std::string::npos) {
            escaped_room_description.replace(pos, 1, "\\\"");
            pos += 2;
        }
        std::string command = "./easy_diffusion \"" + escaped_room_description + "\" \"25\" \"512x512\" \"" + filename + "\"";
        std::cout << "Generating image for " << room << "..." << std::endl;
        std::cout << "Command: " << command << std::endl; // Print the command
        std::cout << "Room description: " << room_description << std::endl; // Print the room description
        std::string output = exec(command.c_str());
		if (output.empty()) {
			std::cerr << "Command failed: " << command << std::endl;
		}
        std::cout << output << std::endl;
    }
}

// Function to get a list of weapons from the LLM
std::vector<std::string> getWeaponsFromLLM() {
    const int maxRetries = 3;
    int retryCount = 0;
    std::vector<std::string> weapons;

    while (retryCount < maxRetries) {
        std::string prompt = "List 6 random weapons suitable for a clue-like game, but not Candlestick, Dagger, Lead Pipe, Revolver, Rope, or Wrench, separated by commas. Give me only the comma separated list, nothing else.";
        double temperature = 1.0;
        std::string response = getLLMResponse(prompt, temperature);

        // Extract the content from the JSON response
        size_t contentStart = response.find("\"content\":\"");
        if (contentStart == std::string::npos) {
            std::cerr << "Could not find 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
            continue;
        }
        contentStart += strlen("\"content\":\"");

        // Look for the end of the content, allowing for variations in the closing sequence
        size_t contentEnd = response.find("\"}]", contentStart);
        if (contentEnd == std::string::npos) {
            contentEnd = response.find("\"", contentStart); // Try to find the next quote
            if (contentEnd == std::string::npos) {
                std::cerr << "Could not find end of 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
                retryCount++;
                continue;
            }
        }

        std::string content = response.substr(contentStart, contentEnd - contentStart);

        weapons.clear();
        std::stringstream ss(content);
        std::string weapon;
        while (std::getline(ss, weapon, ',')) {
            // Remove leading/trailing whitespace
            weapon.erase(0, weapon.find_first_not_of(" \t\n\r"));
            weapon.erase(weapon.find_last_not_of(" \t\n\r") + 1);
            weapons.push_back(weapon);
        }

        try {
            validateLLMResponseCount(weapons, 6, "weapons");
            // If validation succeeds, break out of the loop
            break;
        } catch (const std::exception& e) {
            std::cerr << "Error validating LLM response: " << e.what() << " (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
        }
    }

    // If max retries reached, return an empty vector
    if (retryCount == maxRetries) {
        std::cerr << "Max retries reached for getting weapons. Returning empty list." << std::endl;
        return {};
    }

    // Create the weapons directory
    std::string weapons_dir = "images/weapons/";
    if (!createDirectory(weapons_dir))
    {
        std::cerr << "Could not create weapons directory!" << std::endl;
    }

    for (const auto& weapon : weapons) {
		// Generate a prompt for the weapon description
		std::string prompt = "Describe the physical appearance of a " + weapon + ". Start with '" + weapon + ", ' and then use short, concise language punctuated with commas to describe the things that should be in the image. For example, if the item was a baseball bat the description could be as simple as 'baseball bat, wooden'";
		double temperature = 1.0;
		std::string response = getLLMResponse(prompt, temperature);

		// Extract the content from the JSON response
		size_t contentStart = response.find("\"content\":\"");
		if (contentStart == std::string::npos) {
			std::cerr << "Could not find 'content' in LLM response." << std::endl;
			continue;
		}
		contentStart += strlen("\"content\":\"");

		// Look for the end of the content, allowing for variations in the closing sequence
		size_t contentEnd = response.find("\"}]", contentStart);
		if (contentEnd == std::string::npos) {
			contentEnd = response.find("\"", contentStart); // Try to find the next quote
			if (contentEnd == std::string::npos) {
				std::cerr << "Could not find end of 'content' in LLM response." << std::endl;
				continue;
			}
		}

		std::string weapon_description = response.substr(contentStart, contentEnd - contentStart);

		// Remove quotes and newlines from the weapon description
		weapon_description.erase(std::remove(weapon_description.begin(), weapon_description.end(), '\"'), weapon_description.end());
		weapon_description.erase(std::remove(weapon_description.begin(), weapon_description.end(), '\n'), weapon_description.end());
		weapon_description.erase(std::remove(weapon_description.begin(), weapon_description.end(), '\\'), weapon_description.end());

        std::string filename = weapons_dir + weapon + ".png";
        // Escape the quotes in the weapon description
        std::string escaped_weapon_description = weapon_description;
        size_t pos = 0;
        while ((pos = escaped_weapon_description.find("\"", pos)) != std::string::npos) {
            escaped_weapon_description.replace(pos, 1, "\\\"");
            pos += 2;
        }
        std::string command = "./easy_diffusion \"" + escaped_weapon_description + "\" \"25\" \"512x512\" \"" + filename + "\"";
        std::cout << "Generating image for " << weapon << "..." << std::endl;
        std::cout << "Command: " << command << std::endl; // Print the command
        std::cout << "Weapon description: " << weapon_description << std::endl; // Print the weapon description
        std::string output = exec(command.c_str());
		if (output.empty()) {
			std::cerr << "Command failed: " << command << std::endl;
		}
        std::cout << output << std::endl;
    }

    return weapons;
}

// Function to get a list of characters from the LLM
std::vector<std::string> getCharactersFromLLM() {
    const int maxRetries = 3;
    int retryCount = 0;
    std::vector<std::string> characters;

    while (retryCount < maxRetries) {
        std::string prompt = "List 6 random characters suitable for a clue-like game, but not Miss Scarlet, Colonel Mustard, Mrs. White, Mr. Green, Mrs. Peacock, or Professor Plum, separated by commas. Give me only the comma separated list, nothing else.";
        double temperature = 1.0;
        std::string response = getLLMResponse(prompt, temperature);

        // Extract the content from the JSON response
        size_t contentStart = response.find("\"content\":\"");
        if (contentStart == std::string::npos) {
            std::cerr << "Could not find 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
            continue;
        }
        contentStart += strlen("\"content\":\"");

        // Look for the end of the content, allowing for variations in the closing sequence
        size_t contentEnd = response.find("\"}]", contentStart);
        if (contentEnd == std::string::npos) {
            contentEnd = response.find("\"", contentStart); // Try to find the next quote
            if (contentEnd == std::string::npos) {
                std::cerr << "Could not find end of 'content' in LLM response (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
                retryCount++;
                continue;
            }
        }

        std::string content = response.substr(contentStart, contentEnd - contentStart);

        characters.clear();
        std::stringstream ss(content);
        std::string character;
        while (std::getline(ss, character, ',')) {
            // Remove leading/trailing whitespace
            character.erase(0, character.find_first_not_of(" \t\n\r"));
            character.erase(character.find_last_not_of(" \t\n\r") + 1);
            characters.push_back(character);
        }

        try {
            validateLLMResponseCount(characters, 6, "characters");
            // If validation succeeds, break out of the loop
            break;
        } catch (const std::exception& e) {
            std::cerr << "Error validating LLM response: " << e.what() << " (retry " << retryCount + 1 << "/" << maxRetries << ")." << std::endl;
            retryCount++;
        }
    }

    // If max retries reached, return an empty vector
    if (retryCount == maxRetries) {
        std::cerr << "Max retries reached for getting characters. Returning empty list." << std::endl;
        return {};
    }

    // Call easy_diffusion.cpp for each character
    std::string characters_dir = "images/characters/";
    if (!createDirectory(characters_dir))
    {
        std::cerr << "Could not create characters directory!" << std::endl;
    }

    for (const auto &character : characters)
    {
        // Generate a prompt for the character description
        std::string prompt = "Describe the physical appearance of " + character + ". Describe them in short concise language as if you were describing them to a painter.  Such as  'A woman, sunglasses, a hat, brown coat.'  Only output your description and nothing else, no preamble or further explanation.";
        double temperature = 1.0;
        std::string response = getLLMResponse(prompt, temperature);

        // Extract the content from the JSON response
        size_t contentStart = response.find("\"content\":\"");
        if (contentStart == std::string::npos)
        {
            std::cerr << "Could not find 'content' in LLM response." << std::endl;
            continue;
        }
        contentStart += strlen("\"content\":\"");

        // Look for the end of the content, allowing for variations in the closing sequence
        size_t contentEnd = response.find("\"}]", contentStart);
        if (contentEnd == std::string::npos)
        {
            contentEnd = response.find("\"", contentStart); // Try to find the next quote
            if (contentEnd == std::string::npos)
            {
                std::cerr << "Could not find end of 'content' in LLM response." << std::endl;
                continue;
            }
        }

        std::string character_description = response.substr(contentStart, contentEnd - contentStart);

        // Remove quotes and newlines from the character description
        character_description.erase(std::remove(character_description.begin(), character_description.end(), '\"'), character_description.end());
        character_description.erase(std::remove(character_description.begin(), character_description.end(), '\n'), character_description.end());
        character_description.erase(std::remove(character_description.begin(), character_description.end(), '\\'), character_description.end());

        std::string filename = characters_dir + character + ".png";
        // Escape the quotes in the character description
        std::string escaped_character_description = character_description;
        size_t pos = 0;
        while ((pos = escaped_character_description.find("\"", pos)) != std::string::npos)
        {
            escaped_character_description.replace(pos, 1, "\\\"");
            pos += 2;
        }
        std::string command = "./easy_diffusion \"" + escaped_character_description + "\" \"25\" \"512x512\" \"" + filename + "\"";
        std::cout << "Generating image for " << character << "..." << std::endl;
        std::cout << "Command: " << command << std::endl; // Print the command
        std::cout << "Character description: " << character_description << std::endl; // Print the character description
        std::string output = exec(command.c_str());
		if (output.empty()) {
			std::cerr << "Command failed: " << command << std::endl;
		}
        std::cout << output << std::endl;
    }

    return characters;
}

void initializeGame(int numPlayers) {
    // Get lists of rooms, weapons, and characters from LLM
    std::vector<std::string> llmRooms = getRoomsFromLLM();

    // Generate images for the rooms
    generateRoomImages(llmRooms);
    std::vector<std::string> llmWeapons = getWeaponsFromLLM();
    std::vector<std::string> llmCharacters = getCharactersFromLLM();

    // Check if the LLM calls were successful
    if (llmRooms.empty() || llmWeapons.empty() || llmCharacters.empty()) {
        std::cerr << "Failed to initialize game due to LLM call failure." << std::endl;
        //throw std::runtime_error("Failed to initialize game due to LLM call failure.");
		std::cerr << "Using default rooms, weapons, and characters." << std::endl;
    } else {

		// Check if enough rooms were retrieved
		if (llmRooms.size() < 9) {
			std::cerr << "Not enough rooms retrieved from LLM. Game cannot start." << std::endl;
			//throw std::runtime_error("Not enough rooms retrieved from LLM.");
			std::cerr << "Using default rooms, weapons, and characters." << std::endl;
		} else {

			// Create the card vectors using the LLM-generated lists
			rooms.clear();
			for (const auto& roomName : llmRooms) {
				rooms.push_back({"room", roomName});
			}
		}

		weapons.clear();
		for (const auto& weaponName : llmWeapons) {
			weapons.push_back({"weapon", weaponName});
		}

		characters.clear();
		for (const auto& characterName : llmCharacters) {
			characters.push_back({"character", characterName});
		}
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

        // Check if characters is empty before accessing it
        if (!characters.empty()) {
            newPlayer.character = characters[i % characters.size()].name; // Assign characters in order for now
        } else {
            std::cerr << "Error: No characters available." << std::endl;
            throw std::runtime_error("No characters available.");
        }

        // Assign initial locations (starting rooms) - use the first 3 rooms
        if (llmRooms.size() >= 3) {
            newPlayer.row = 0;
            newPlayer.col = i;
            //newPlayer.location = "Hall-" + llmRooms[i % 3]; // Assign to a hallway
        } else {
            newPlayer.row = 0;
            newPlayer.col = 0;
            //newPlayer.location = "Hall"; // Default if not enough rooms
        }
        
        players.push_back(newPlayer);
        checklists.emplace_back(characters, weapons, rooms); // Initialize checklist for each player
    }

	std::cout << "Number of rooms: " << rooms.size() << std::endl; // ADDED DEBUGGING

    // Deal the remaining cards to the players
    int playerIndex = 0;
    for (Card& card : deck) {
        players[playerIndex].hand.push_back(card.name);
		checklists[playerIndex].markKnown(card.name);
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
            std::cout << "Enter the row you want to move to: ";
            int newRow;
            std::cin >> newRow;
            std::cout << "Enter the column you want to move to: ";
            int newCol;
            std::cin >> newCol;

            if (board.isValidMove(player.row, player.col, newRow, newCol)) {
                player.row = newRow;
                player.col = newCol;
                std::cout << "You moved to row " << newRow << ", column " << newCol << std::endl;
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
                std::cout << "Current location: Row " << players[i].row << ", Column " << players[i].col << "\n";

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
    // Get lists of rooms from LLM *before* initializing board
    std::vector<std::string> llmRooms = getRoomsFromLLM();
    rooms.clear();
    for (const auto& roomName : llmRooms) {
        rooms.push_back({"room", roomName});
    }

    int numPlayers;
    std::cout << "Enter the number of players (2-6): ";
    std::cin >> numPlayers;

    try {
        initializeGame(numPlayers);
        playGame();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1; // Exit the program with an error code
    }

    return 0;
}
