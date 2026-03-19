/* Start Header *****************************************************************/
/*!
\file    Serializer.h
\author  Daniel Neo Zuo Feng Kay (100%)
\par     k.danielneozuofeng@digipen.edu
\date    26th September 2025
\brief
This header defines the Serializer class, which manages
the conversion of individual entities (and their components)
to and from JSON format. It maintains a registry of supported
component types and their serialization/deserialization logic,
allowing entities to be reconstructed within a World.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
         * @brief Loads JSON from a file after validating the extension.
         * @param filename Path to the JSON file.
         * @param expectedExt Expected file extension (without the period).
         * @param outJson Destination JSON object populated on success.
         * @return true if loading and parsing succeed, false otherwise.
         */
        static bool LoadJson(const std::string& filename, const std::string& expectedExt, json& outJson) {
            // Check file extension
            if (!HasExtension(filename, expectedExt)) {
                LOG_ERROR("Error: File extension must be " << expectedExt);
                return false;
            }
            std::ifstream file(filename); // Open file for reading

            // Check if file opened successfully
            if (!file.is_open()) {
                LOG_ERROR("Error: Cannot open file: " << filename);
                return false;
            }

            try {
                file >> outJson; // Parse JSON from file
                return true;
            }
            catch (const std::exception& e) {
                // Catch any exceptions from parsing the JSON file
                LOG_ERROR("Error parsing JSON: " << e.what());
                return false;
            }
        }

        /**
         * @brief Saves JSON to a file after validating the extension.
         * @param filename Path to the JSON file.
         * @param expectedExt Expected file extension (without the period).
         * @param j JSON object to write.
         * @return true if the file is written successfully, false otherwise.
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
            file << j.dump(4); // Pretty-print with 4-space indentation

            return true;
        }

        /**
         * @brief Checks whether a filename matches the expected extension.
         * @param filename Filename to validate.
         * @param ext Expected file extension (without the period).
         * @return true if the filename ends with the extension, false otherwise.
         */
        static bool HasExtension(const std::string& filename, const std::string& ext) {
            if (filename.length() < ext.length() + 1) // Check if filename is at least as long as ext + 1 (for period)
                return false;

            // Check if the end of the filename matches the expected extension
            return filename.substr(filename.length() - ext.length() - 1) == "." + ext;
        }
    };

}

#endif
