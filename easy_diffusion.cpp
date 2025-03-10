#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>
#include <future>
#include <chrono>
#include <array> // Include array

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/json.h>

using namespace std;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::json;

// Define the server address as a constant
const string SERVER_ADDRESS = "http://localhost:9000";
const string LLM_SERVER_ADDRESS = "http://localhost:9090/v1"; // Define LLM server address

// Define the default stable diffusion model
const string DEFAULT_STABLE_DIFFUSION_MODEL = "absolutereality_v181";

// Helper function to execute shell commands and return the output
string exec(const char* cmd) {
    std::array<char, 128> buffer; // Use std::array to resolve ambiguity
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), static_cast<int(*)(FILE*)>(pclose));
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Function to generate a random seed
unsigned int generate_seed() {
    unsigned int seed = 0;
    try {
        // Use a simpler seed generation method
        seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    } catch (const std::exception& e) {
        cerr << "Error in generate_seed: " << e.what() << endl;
        return 0; // Return a default seed value
    } catch (...) {
        cerr << "Unknown error in generate_seed." << endl;
        return 0; // Return a default seed value
    }
    return seed;
}

// Function to fetch data from a URL
string fetch_url(const string& url) {
    try {
        http_client client(U(url));
        http_request request(methods::GET);
        pplx::task<http_response> responseTask = client.request(request);
        http_response response = responseTask.get();

        if (response.status_code() == status_codes::OK) {
            pplx::task<string> bodyTask = response.extract_string();
            return bodyTask.get();
        } else {
            cerr << "Error: HTTP request failed with status code " << response.status_code() << endl;
            return "";
        }
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return "";
    }
}

// Function to interact with the LLM server
string call_llm(const string& system_prompt, const string& user_prompt, double temperature) {
    try {
        http_client client(U(LLM_SERVER_ADDRESS + "/chat/completions"));
        http_request request(methods::POST);
        request.headers().add("Content-Type", "application/json");

        // Construct the JSON payload
        json::value payload;
        payload["model"] = json::value::string("llama-3.2-3b-it-q8_0");
        payload["messages"] = json::value::array();
        payload["messages"][0] = json::value::object();
        payload["messages"][0]["role"] = json::value::string("system");
        payload["messages"][0]["content"] = json::value::string(system_prompt);
        payload["messages"][1] = json::value::object();
        payload["messages"][1]["role"] = json::value::string("user");
        payload["messages"][1]["content"] = json::value::string(user_prompt);
        payload["temperature"] = json::value::number(temperature);

        request.set_body(payload.serialize());

        pplx::task<http_response> responseTask = client.request(request);
        http_response response = responseTask.get();

        if (response.status_code() == status_codes::OK) {
            pplx::task<string> bodyTask = response.extract_string();
            string body = bodyTask.get();

            // Parse the JSON response
            json::value json_response = json::value::parse(U(body));
            if (json_response.has_field(U("choices")) && json_response["choices"].is_array() && json_response["choices"].as_array().size() > 0 && json_response["choices"][0].has_field(U("message")) && json_response["choices"][0]["message"].has_field(U("content"))) {
                return json_response["choices"][0]["message"]["content"].as_string();
            } else {
                cerr << "Error: Could not extract content from LLM response." << endl;
                cerr << "Response body: " << body << endl; // Print the response body for debugging
                return "";
            }
        } else {
            cerr << "Error: LLM server returned status code " << response.status_code() << endl;
            return "";
        }
    } catch (const std::exception& e) {
        cerr << "Error communicating with LLM server: " << e.what() << endl;
        return "";
    }
    return "";
}

// Function to generate character
string generate_character() {
    int rand_num = rand() % 2;
    string occu;

    if (rand_num == 0) {
        string system_prompt = "You are a character design expert. You always return JSON lists with no explanation.";
        string user_prompt = "List a random character cosplay. List only the character cosplay and no further description. Do not give URLs.";
        occu = call_llm(system_prompt, user_prompt, 2.0);

        // Parse JSON and extract cosplay
        size_t start_pos = occu.find("\"");
        if (start_pos != string::npos) {
            size_t end_pos = occu.find("\"", start_pos + 1);
            if (end_pos != string::npos) {
                occu = occu.substr(start_pos + 1, end_pos - start_pos - 1);
            }
        }
    } else {
        string system_prompt = "You are a character design expert. You always return JSON lists with no explanation.";
        string user_prompt = "List a random occupation. List only the occupation and no further description.";
        occu = call_llm(system_prompt, user_prompt, 2.0);

        // Parse JSON and extract occupation
        size_t start_pos = occu.find("\"");
        if (start_pos != string::npos) {
            size_t end_pos = occu.find("\"", start_pos + 1);
            if (end_pos != string::npos) {
                occu = occu.substr(start_pos + 1, end_pos - start_pos - 1);
            }
        }
    }
    return occu;
}

// Function to extract the task ID from the JSON response
string extract_task_id(const string& json_response) {
    string task_id;
    size_t task_pos = json_response.find("\"task\":");
    if (task_pos != string::npos) {
        size_t start_pos = task_pos + 7;
        // Find the end of the number
        size_t end_pos = json_response.find_first_not_of("0123456789", start_pos);
        if (end_pos != string::npos) {
            task_id = json_response.substr(start_pos, end_pos - start_pos);
        } else {
            // The task ID is at the end of the string
            task_id = json_response.substr(start_pos);
        }
    } else {
        cerr << "Error: Could not find task ID in JSON response" << endl;
    }
    if (task_id.empty()) {
        cerr << "Error: Task ID is empty." << endl;
    }
    return task_id;
}

// Function to extract the task status from the JSON response
string extract_task_status(const string& json_response, const string& task_id) {
    string status;
    string search_string = "\"" + task_id + "\":\"";
    size_t status_pos = json_response.find(search_string);
    if (status_pos != string::npos) {
        size_t start_pos = status_pos + search_string.length();
        size_t end_pos = json_response.find("\"", start_pos);
        if (end_pos != string::npos) {
            status = json_response.substr(start_pos, end_pos - start_pos);
        }
    }
    return status;
}

// Function to extract a value from the JSON stream
string extract_json_value(const string& json_stream, const string& key) {
    if (key == "data") {
        // Special handling for "data" field
        size_t data_pos = json_stream.find("\"data\":\"");
        if (data_pos != string::npos) {
            size_t start_pos = data_pos + 8;
            size_t end_pos = json_stream.find("\"", start_pos);
            if (end_pos == string::npos) {
                end_pos = json_stream.length(); // If no closing quote, read till the end
            }
            return json_stream.substr(start_pos, end_pos - start_pos);
        }
        return "";
    } else {
        string value;
        size_t pos = 0;
        while (pos < json_stream.length()) {
            try {
                size_t start = json_stream.find('{', pos);
                if (start == string::npos) break;
                size_t end = json_stream.find('}', start);
                if (end == string::npos) break;

                string json_object = json_stream.substr(start, end - start + 1);
                pos = end + 1;

                json::value json_val = value::parse(U(json_object)); // Use json::value
                if (json_val.is_object() && json_val.has_field(U(key))) {
                    const json::value& field_value = json_val.at(U(key)); // Use json::value
                    if (field_value.is_string()) {
                        value = field_value.as_string();
                        return value; // Return on first match
                    } else if (field_value.is_number()) {
                        value = to_string(field_value.as_double());
                        return value; // Return on first match
                    } else if (field_value.is_boolean()) {
                        value = field_value.as_bool() ? "true" : "false";
                        return value; // Return on first match
                    } else {
                        cerr << "Warning: Unsupported JSON value type for key '" << key << "'" << endl;
                    }
                } else {
                    cerr << "Warning: Key '" << key << "' not found in JSON or JSON is not an object." << endl;
                }
            } catch (const json_exception& e) {
                cerr << "Error parsing JSON for key '" << key << "': " << e.what() << endl;
                cerr << "JSON Stream: " << json_stream << endl; // Print the JSON stream for debugging
            } catch (const std::exception& e) {
                cerr << "Error: " << e.what() << endl;
            }
        }
        return value;
    }
}

int main(int argc, char* argv[]) {
    srand(time(0)); // Seed the random number generator

    int tag_nums = 10;
    unsigned int seed = generate_seed();
    string prompt;
    string neg_prompt = "";
    string char_line;
    vector<string> top_tags;

    // Define LoRAs and their strengths
    vector<string> loras = {"64x3-05:1.0"};
    vector<string> lora_models;
    vector<string> lora_alphas;

    for (const string& lora : loras) {
        size_t pos = lora.find(':');
        if (pos != string::npos) {
            lora_models.push_back(lora.substr(0, pos));
            lora_alphas.push_back(lora.substr(pos + 1));
        } else {
            lora_models.push_back(lora);
            lora_alphas.push_back("1.0");
        }
    }

    // Get prompt from command line or user input
    if (argc > 1) {
        prompt = argv[1];
    } else {
        cout << "Enter prompt: ";
        getline(cin, prompt);
    }

    // Get num_inference_steps from command line or use default value
    int num_inference_steps = 60;
    if (argc > 2) {
        try {
            num_inference_steps = stoi(argv[2]);
        } catch (const invalid_argument& e) {
            cerr << "Error: Invalid argument for num_inference_steps. Using default value of " << num_inference_steps << "." << endl;
        } catch (const out_of_range& e) {
            cerr << "Error: Out of range for num_inference_steps. Using default value of " << num_inference_steps << "." << endl;
        }
    }

    // Get width and height from command line or use default value
    int width = 192;
    int height = 256;
    if (argc > 3) {
        string resolution = argv[3];
        size_t x_pos = resolution.find('x');
        if (x_pos != string::npos) {
            try {
                width = stoi(resolution.substr(0, x_pos));
                height = stoi(resolution.substr(x_pos + 1));
            } catch (const invalid_argument& e) {
                cerr << "Error: Invalid argument for resolution. Using default values of " << width << "x" << height << "." << endl;
            } catch (const out_of_range& e) {
                cerr << "Error: Out of range for resolution. Using default values of " << width << "x" << height << "." << endl;
            }
        } else {
            cerr << "Error: Invalid resolution format. Using default values of " << width << "x" << height << "." << endl;
        }
    }

    // Get output filename from command line or use default value
    string output_filename = "output.png";
    if (argc > 4) {
        output_filename = argv[4];
    }

    char_line = "";
    top_tags.clear();

    string clip = "false";

    cout << "Prompt: " << prompt << " | Negative: " << neg_prompt << " | Inference Steps: " << num_inference_steps << " | Width: " << width << " | Height: " << height << " | Output Filename: " << output_filename << endl;

    // Construct JSON payload manually
    stringstream payload_stream;
    payload_stream << "{"
                   << "\"prompt\":\"" << prompt << "\","
                   << "\"seed\":" << seed << ","
                   << "\"used_random_seed\":true,"
                   << "\"negative_prompt\":\"" << neg_prompt << "\","
                   << "\"num_outputs\":1,"
                   << "\"num_inference_steps\":" << num_inference_steps << ","
                   << "\"guidance_scale\":7.5,"
                   << "\"width\":" << width << ","
                   << "\"height\":" << height << ","
                   << "\"vram_usage_level\":\"balanced\","
                   << "\"sampler_name\":\"dpmpp_3m_sde\","
                   << "\"use_stable_diffusion_model\":\"" << DEFAULT_STABLE_DIFFUSION_MODEL << "\","
                   << "\"clip_skip\":" << clip << ","
                   << "\"use_vae_model\":\"\","
                   << "\"stream_progress_updates\":true,"
                   << "\"stream_image_progress\":false,"
                   << "\"show_only_filtered_image\":true,"
                   << "\"block_nsfw\":false,"
                   << "\"output_format\":\"png\","
                   << "\"output_quality\":75,"
                   << "\"output_lossless\":false,"
                   << "\"metadata_output_format\":\"embed,json\","
                   << "\"original_prompt\":\"" << prompt << "\","
                   << "\"active_tags\":[],"
                   << "\"inactive_tags\":[],"
                   << "\"save_to_disk_path\":\"~/Pictures/stable-diffusion/output/\","
                   << "\"use_lora_model\":[";
    for (size_t i = 0; i < lora_models.size(); ++i) {
        payload_stream << "\"" << lora_models[i] << "\"";
        if (i < lora_models.size() - 1) {
            payload_stream << ",";
        }
    }
    payload_stream << "],"
                   << "\"lora_alpha\":[";
    for (size_t i = 0; i < lora_alphas.size(); ++i) {
        payload_stream << "\"" << lora_alphas[i] << "\"";
        if (i < lora_alphas.size() - 1) {
            payload_stream << ",";
        }
    }
    payload_stream << "],"
                   << "\"enable_vae_tiling\":false,"
                   << "\"scheduler_name\":\"automatic\","
                   << "\"session_id\":\"1337\""
                   << "}";

    string payload = payload_stream.str();

    // Send request
    try {
        http_client client(U(SERVER_ADDRESS + "/render"));
        http_request request(methods::POST);
        request.headers().add("Accept", "*/*");
        request.headers().add("Accept-Language", "en-US,en;q=0.9");
        request.headers().add("Cache-Control", "no-cache");
        request.headers().add("Connection", "keep-alive");
        request.headers().add("Content-Type", "application/json");
        request.headers().add("Origin", U(SERVER_ADDRESS));
        request.headers().add("Pragma", "no-cache");
        request.headers().add("Referer", U(SERVER_ADDRESS + "/"));
        request.headers().add("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/132.0.0.0 Safari/537.36");
        request.set_body(payload);

        pplx::task<http_response> responseTask = client.request(request);
        http_response response = responseTask.get();

        if (response.status_code() == status_codes::OK) {
            pplx::task<string> bodyTask = response.extract_string();
            string body = bodyTask.get();

            string task = extract_task_id(body);

            // Monitor task status
            string status;
            string image_data;
            do {
                string status_url = SERVER_ADDRESS + "/ping?session_id=1337";
                string status_response = fetch_url(status_url);
                if (!status_response.empty()) {
                    status = extract_task_status(status_response, task);
                    if (status.empty()) {
                        status = "unknown";
                    }
                } else {
                    status = "error";
                }

                if (status == "error") {
                    cerr << "\rError during task execution.                                      " << endl;
                    return 1;
                }

                if (status != "completed") {
                    string stream_url = SERVER_ADDRESS + "/image/stream/" + task;
                    string stream_response = fetch_url(stream_url);

                    string steps_value = extract_json_value(stream_response, "step");
                    string total_steps_value = extract_json_value(stream_response, "total_steps");

                    int steps = 0;
                    int total_steps = 1;

                    if (!steps_value.empty()) {
                        try {
                            steps = stoi(steps_value);
                        } catch (const invalid_argument& e) {
                            cerr << "Error: Invalid argument for steps: " << e.what() << endl;
                        } catch (const out_of_range& e) {
                            cerr << "Error: Out of range for steps: " << e.what() << endl;
                        }
                    }

                    if (!total_steps_value.empty()) {
                        try {
                            total_steps = stoi(total_steps_value);
                        } catch (const invalid_argument& e) {
                            cerr << "Error: Invalid argument for total_steps: " << e.what() << endl;
                        } catch (const out_of_range& e) {
                            cerr << "Error: Out of range for total_steps: " << e.what() << endl;
                        }
                    }

                    float percentage = (float)steps / total_steps * 100.0f;

                    cout << "\rStatus: " << status << " | Progress: " << fixed << setprecision(2) << percentage << "%                                      " << flush;

                    if (status == "buffer") {
                        //cout << "Status is buffer, continuing to wait..." << endl;
                    }

                    this_thread::sleep_for(chrono::seconds(5));
                } else {
                    // Extract image data
                    string stream_url = SERVER_ADDRESS + "/image/stream/" + task;
                    string stream_response = fetch_url(stream_url);
                    image_data = extract_json_value(stream_response, "data");
                }
            } while (status != "completed");

            cout << endl;

            // Save image to file
            if (!image_data.empty()) {
                // Decode base64 image data
                string base64_stripped = image_data.substr(image_data.find(',') + 1);
                vector<unsigned char> decoded_image_data;
                
                // Decode base64
                
                // Convert base64 string to byte array
                const std::string base64_chars = 
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "abcdefghijklmnopqrstuvwxyz"
                     "0123456789+/";

                std::vector<unsigned char> in_bytes(base64_stripped.begin(), base64_stripped.end());
                std::vector<unsigned char> out_bytes;

                int val = 0, valb = -8;
                for (unsigned char c : in_bytes) {
                    if (c == '=') break;
                    int index = base64_chars.find(c);
                    if (index == std::string::npos) continue;

                    val = (val << 6) | index;
                    valb += 6;

                    if (valb >= 0) {
                        out_bytes.push_back(char((val >> valb) & 0xFF));
                        valb -= 8;
                    }
                }
                
                // Write image data to file
                ofstream image_file(output_filename, ios::binary);
                image_file.write(reinterpret_cast<const char*>(out_bytes.data()), out_bytes.size());
                image_file.close();
                cout << "Image saved to " << output_filename << endl;
            } else {
                cerr << "Error: Image data is empty." << endl;
            }

        } else {
            cerr << "Error: HTTP request failed with status code " << response.status_code() << endl;
            return 1;
        }
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
