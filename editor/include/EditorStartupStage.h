/* Start Header *****************************************************************/
/*!
\file   EditorStartupStage.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
This header defines the EditorStartupStage enum which represents the different
stages of the editor's startup process.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_STARTUP_STAGE_H
#define EDITOR_STARTUP_STAGE_H

/**
 * @brief 
 * The different stages of the editor's startup process.
 * This is used to control the flow of the startup sequence and determine which UI to show.
 * - SelectProject: The user is prompted to select a project to open or create a new one.
 * - Booting: The editor is loading the selected project and initializing subsystems.
 * - SelectScene: The user is prompted to select a scene to open within the project.
 * - Ready: The editor has finished loading and is ready for user interaction.
 */
enum class EditorStartupStage {
    SelectProject,
    Booting,
    SelectScene,
    Ready
};

#endif
