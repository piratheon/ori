#include "ori_provider.h"
#include "ori_core.h"
#include <curl/curl.h>
#include <json/json.h>
#include <thread> // Added for std::thread and std::this_thread
#include <chrono> // Added for std::chrono

// Callback function to write response data to a string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

// OpenRouterProvider
OpenRouterProvider::OpenRouterProvider() : model("google/gemini-2.0-flash-exp:free") {} 

void OpenRouterProvider::setModel(const std::string& model_name) {
    model = model_name;
}

void OpenRouterProvider::setApiKey(const std::string& key) {
    api_key = key;
}

std::string OpenRouterProvider::sendQuery(const std::string& prompt, const std::vector<std::pair<std::string, std::string>>& conversation_history) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "Error: Failed to initialize curl";
    }

    Json::Value request_data;
    request_data["model"] = model;

    Json::Value messages(Json::arrayValue);
    for (const auto& msg : conversation_history) {
        Json::Value message;
        message["role"] = msg.first;
        message["content"] = msg.second;
        messages.append(message);
    }
    
    request_data["messages"] = messages;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // Compact output
    std::string json_data = Json::writeString(builder, request_data);

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OriAssistant/1.0");

    const int max_retries = 5;
    const int retry_delay_seconds = 2;
    CURLcode res = CURLE_OK;
    long http_code = 0;

    for (int attempt = 0; attempt < max_retries; ++attempt) {
        std::string spinner_message = "loading...";
        if (attempt > 0) {
            std::string reason;
            if (res != CURLE_OK) {
                reason = curl_easy_strerror(res);
            } else if (http_code == 429) {
                reason = "rate limited";
            } else {
                reason = "connection failed";
            }
            spinner_message = reason + ", retrying...";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
        }

        response_data.clear();
        keep_running = true;
        std::thread spinner_thread;
        if (!g_is_gui_mode) {
            spinner_thread = std::thread(run_spinner, spinner_message);
        }
        
        res = curl_easy_perform(curl);
        keep_running = false;
        if (spinner_thread.joinable()) {
            spinner_thread.join();
        }

        if (res != CURLE_OK) {
            if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST || res == CURLE_OPERATION_TIMEDOUT) {
                if (attempt < max_retries - 1) continue;
            }
            break;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code >= 200 && http_code < 300) {
            break;
        }

        if (http_code == 429) {
            if (attempt < max_retries - 1) continue;
        }
        break;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error: Failed to connect to OpenRouter API - " + std::string(curl_easy_strerror(res));
    }

    Json::Value response_json;
    Json::Reader reader;
    if (!reader.parse(response_data, response_json)) {
        return "Error: Failed to parse API response - " + response_data;
    }

    if (response_json.isMember("error")) {
        std::string error_message = "API Error";
        const Json::Value& error_obj = response_json["error"];

        if (error_obj.isMember("code") && error_obj["code"].isNumeric()) {
            error_message += " (Code: " + error_obj["code"].asString() + ")";
        }
        if (error_obj.isMember("message") && error_obj["message"].isString()) {
            error_message += ": " + error_obj["message"].asString();
        }
        if (error_obj.isMember("metadata") && error_obj["metadata"].isObject() &&
            error_obj["metadata"].isMember("raw") && error_obj["metadata"]["raw"].isString()) {
            error_message += " (Details: " + error_obj["metadata"]["raw"].asString() + ")";
        }
        return error_message;
    }

    if (response_json.isMember("choices") && response_json["choices"].isArray() && response_json["choices"].size() > 0) {
        return response_json["choices"][0]["message"]["content"].asString();
    }

    return "Error: Unexpected API response format - " + response_data;
}


// GoogleProvider
GoogleProvider::GoogleProvider() : model("gemini-2.5-flash") {} 

void GoogleProvider::setModel(const std::string& model_name) {
    model = model_name;
}

void GoogleProvider::setApiKey(const std::string& key) {
    api_key = key;
}

std::string GoogleProvider::sendQuery(const std::string& prompt, const std::vector<std::pair<std::string, std::string>>& conversation_history) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "Error: Failed to initialize curl";
    }

    Json::Value request_data;
    Json::Value contents(Json::arrayValue);
    
    for (const auto& msg : conversation_history) {
        if (msg.first == "system") {
            Json::Value sys_part;
            sys_part["text"] = msg.second;
            request_data["system_instruction"]["parts"].append(sys_part);
            continue;
        }

        Json::Value content;
        Json::Value part;
        part["text"] = msg.second;
        content["parts"].append(part);
        content["role"] = (msg.first == "assistant") ? "model" : msg.first;
        contents.append(content);
    }

    request_data["contents"] = contents;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // Compact output
    std::string json_data = Json::writeString(builder, request_data);

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent?key=" + api_key;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OriAssistant/1.0");

    const int max_retries = 5;
    const int retry_delay_seconds = 2;
    CURLcode res = CURLE_OK;
    long http_code = 0;

    for (int attempt = 0; attempt < max_retries; ++attempt) {
        std::string spinner_message = "loading...";
        if (attempt > 0) {
            std::string reason;
            if (res != CURLE_OK) {
                reason = curl_easy_strerror(res);
            } else if (http_code == 429) {
                reason = "rate limited";
            } else {
                reason = "connection failed";
            }
            spinner_message = reason + ", retrying...";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
        }

        response_data.clear();
        keep_running = true;
        std::thread spinner_thread;
        if (!g_is_gui_mode) {
            spinner_thread = std::thread(run_spinner, spinner_message);
        }
        
        res = curl_easy_perform(curl);
        keep_running = false;
        if (spinner_thread.joinable()) {
            spinner_thread.join();
        }

        if (res != CURLE_OK) {
            if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST || res == CURLE_OPERATION_TIMEDOUT) {
                if (attempt < max_retries - 1) continue;
            }
            break;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code >= 200 && http_code < 300) {
            break;
        }

        if (http_code == 429) {
            if (attempt < max_retries - 1) continue;
        }
        break;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error: Failed to connect to Google API - " + std::string(curl_easy_strerror(res));
    }

    Json::Value response_json;
    Json::Reader reader;
    if (!reader.parse(response_data, response_json)) {
        return "Error: Failed to parse API response - " + response_data;
    }
    
    if (response_json.isMember("candidates") && response_json["candidates"].isArray() && response_json["candidates"].size() > 0) {
        const auto& candidate = response_json["candidates"][0];
        if (candidate.isMember("content") && candidate["content"].isMember("parts") && candidate["content"]["parts"].isArray() && candidate["content"]["parts"].size() > 0) {
            return candidate["content"]["parts"][0]["text"].asString();
        }
    }

    return "Error: Unexpected API response format - " + response_data;
}


// HuggingFaceProvider
HuggingFaceProvider::HuggingFaceProvider() : model("gpt2") {} 

void HuggingFaceProvider::setModel(const std::string& model_name) {
    model = model_name;
}

void HuggingFaceProvider::setApiKey(const std::string& key) {
    api_key = key;
}

std::string HuggingFaceProvider::sendQuery(const std::string& prompt, const std::vector<std::pair<std::string, std::string>>& conversation_history) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "Error: Failed to initialize curl";
    }

    Json::Value request_data;
    
    std::string inputs;
    for (const auto& msg : conversation_history) {
        inputs += msg.second + "\n";
    }
    request_data["inputs"] = inputs;
    

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // Compact output
    std::string json_data = Json::writeString(builder, request_data);

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    std::string url = "https://api-inference.huggingface.co/models/" + model;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OriAssistant/1.0");

    const int max_retries = 5;
    const int retry_delay_seconds = 2;
    CURLcode res = CURLE_OK;
    long http_code = 0;

    for (int attempt = 0; attempt < max_retries; ++attempt) {
        std::string spinner_message = "loading...";
        if (attempt > 0) {
            std::string reason;
            if (res != CURLE_OK) {
                reason = curl_easy_strerror(res);
            } else if (http_code == 429) {
                reason = "rate limited";
            } else {
                reason = "connection failed";
            }
            spinner_message = reason + ", retrying...";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
        }

        response_data.clear();
        keep_running = true;
        std::thread spinner_thread;
        if (!g_is_gui_mode) {
            spinner_thread = std::thread(run_spinner, spinner_message);
        }
        
        res = curl_easy_perform(curl);
        keep_running = false;
        if (spinner_thread.joinable()) {
            spinner_thread.join();
        }

        if (res != CURLE_OK) {
            if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST || res == CURLE_OPERATION_TIMEDOUT) {
                if (attempt < max_retries - 1) continue;
            }
            break;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code >= 200 && http_code < 300) {
            break;
        }

        if (http_code == 429) {
            if (attempt < max_retries - 1) continue;
        }
        break;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error: Failed to connect to Hugging Face API - " + std::string(curl_easy_strerror(res));
    }

    Json::Value response_json;
    Json::Reader reader;
    if (!reader.parse(response_data, response_json)) {
        return "Error: Failed to parse API response - " + response_data;
    }

    if (response_json.isArray() && response_json.size() > 0 && response_json[0].isMember("generated_text")) {
        return response_json[0]["generated_text"].asString();
    }

    return "Error: Unexpected API response format - " + response_data;
}
