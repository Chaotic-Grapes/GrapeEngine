#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "core/Logger.h"

using json = nlohmann::json;

namespace Serialization {
    class Serializer {
    public:
        /**
		 * @brief Loads JSON from file, enforcing extension
		 * @param filename The path to the JSON file
		 * @param expectedExt The expected file extension (without dot)
		 * @param outJson Reference to json object to populate
		 * @return true if successful, false otherwise
         */
        static bool LoadJson(const std::string& filename, const std::string& expectedExt, json& outJson) {
            if (!HasExtension(filename, expectedExt)) {
                LOG_ERROR("Error: File extension must be " << expectedExt);
                return false;
            }
            std::ifstream file(filename);

            if (!file.is_open()) {
                LOG_ERROR("Error: Cannot open file: " << filename);
                return false;
            }

            try {
                file >> outJson;
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("Error parsing JSON: " << e.what());
                return false;
            }
        }

        /** 
		 * @brief Saves JSON to file, enforcing extension
		 * @param filename The path to the JSON file
		 * @param expectedExt The expected file extension (without dot)
		 * @param j The json object to save
		 * @return true if successful, false otherwise
         */
        static bool SaveJson(const std::string& filename, const std::string& expectedExt, const json& j) {
            if (!HasExtension(filename, expectedExt)) {
                LOG_ERROR("Error: File extension must be " << expectedExt);
                return false;
            }
            std::ofstream file(filename);

            if (!file.is_open()) {
                LOG_ERROR("Error: Cannot open file for writing: " << filename);
                return false;
            }
            file << j.dump(4);

            return true;
        }

        /** 
		 * @brief Checks if a filename has the specified extension
		 * @param filename The filename to check
		 * @param ext The expected file extension (without dot)
		 * @return true if the filename has the specified extension, false otherwise
         */
        static bool HasExtension(const std::string& filename, const std::string& ext) {
            if (filename.length() < ext.length() + 1)
                return false;

            return filename.substr(filename.length() - ext.length() - 1) == "." + ext;
        }
    };

}

#endif
