/* Start Header *****************************************************************/
/*!
\file   EditorIcons.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
This header defines a set of icon constants used throughout the editor UI.

\note
Icons are taken from the Material Icons font and represented as UTF-8 strings.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_ICONS_H
#define EDITOR_ICONS_H

namespace EditorIcons {
    // Common actions
    static constexpr const char* Add = "\xEE\x85\x85";             // e145
    static constexpr const char* Save = "\xEE\x85\xA1";            // e161
    static constexpr const char* Clear = "\xEE\x82\xB8";           // e0b8
    static constexpr const char* Options = "\xEE\xA2\xB8";         // e8b8
    static constexpr const char* Reset = "\xEF\x91\xBF";           // f47f
    static constexpr const char* Open = "\xEE\xAB\xB3";            // eaf3
    static constexpr const char* Copy = "\xEE\x85\x8D";            // content_copy
    static constexpr const char* Browse = "\xEE\x8B\x88";          // folder_open
    static constexpr const char* Delete = "\xEE\xA1\xB2";          // delete

    // Asset browser
    static constexpr const char* Folder = "\xEE\x8B\x87";          // folder
    static constexpr const char* File = "\xEE\xA1\xB3";            // insert_drive_file
    static constexpr const char* Drag = "\xEF\x8E\xB2";            // drag_indicator
    static constexpr const char* Import = "\xEF\x82\x9B";          // upload_file
    static constexpr const char* Replace = "\xEE\xA3\x94";         // swap_horiz
    static constexpr const char* NewFolder = "\xEE\x8B\x8C";       // e2cc
    static constexpr const char* Audio = "\xEE\xAE\x82";           // eb82
    static constexpr const char* Scene = "\xEE\x8F\xB7";           // e3f7
    static constexpr const char* Script = "\xEF\xA1\x8D";          // f84d
    static constexpr const char* Texture = "\xEE\x90\xA1";         // e421
    static constexpr const char* Shader = "\xEE\xBE\x8F";          // ef8f
    static constexpr const char* Font = "\xEE\x89\xA2";            // e262

    // Hierarchy
    static constexpr const char* Prefab = "\xEF\x9C\xA0";          // deployed_code
    static constexpr const char* PrefabChanged = "\xEF\x97\xB2";   // deployed_code_alert
    static constexpr const char* PrefabUpdate = "\xEF\x97\xB4";    // deployed_code_update

    // Playback controls
    static constexpr const char* Play = "\xEE\x80\xB7";            // play_arrow
    static constexpr const char* Pause = "\xEE\x80\xB4";           // pause
    static constexpr const char* Stop = "\xEE\x81\x87";            // stop
    static constexpr const char* Step = "\xEE\x81\x84";            // skip_next

    // Scene viewport
    static constexpr const char* Camera3D = "\xEE\xB4\xB8";        // 3d
    static constexpr const char* Camera2D = "\xEE\xBC\xB7";        // 2d
    static constexpr const char* Move = "\xEE\xA2\x9F";            // open_with
    static constexpr const char* Rotate = "\xEE\x90\x9D";          // rotate_right
    static constexpr const char* Scale = "\xEE\x8F\x82";           // crop_free
    static constexpr const char* Local = "\xEE\x95\x9C";           // my_location
    static constexpr const char* World = "\xEE\xA0\x8B";           // public
    static constexpr const char* Overlays = "\xEE\x94\xBB";        // layers
    static constexpr const char* Debug = "\xEE\x90\xA9";           // tune
    static constexpr const char* Layout1 = "\xEE\x8F\x86";         // crop_square
    static constexpr const char* Layout2 = "\xEE\xA3\xB2";         // view_column
    static constexpr const char* Layout4 = "\xEE\xA6\xA9";         // grid_view
    static constexpr const char* Maximize = "\xEE\x97\x90";        // fullscreen
    static constexpr const char* Restore = "\xEE\x97\x91";         // fullscreen_exit

    // Tab icons
    static constexpr const char* TabAssetBrowser = "\xEE\xAA\x85"; // ea85
    static constexpr const char* TabConsole = "\xEE\xAE\x8E";      // eb8e
    static constexpr const char* TabPerformance = "\xEE\xBC\xBE";  // ef3e
    static constexpr const char* TabSystems = "\xEE\x93\xBD";      // e4fd
    static constexpr const char* TabPropertyEditor = "\xEE\x9D\x85"; // e745
    static constexpr const char* TabTilePalette = "\xEF\x8F\x83";  // f3c3
    static constexpr const char* TabLayers = "\xEF\x94\x80";       // f500
    static constexpr const char* TabHierarchy = "\xEE\xA5\xBA";    // e97a
    static constexpr const char* TabPrefabEditor = Prefab;

    // Window titles with icons but unused for now
    static constexpr const char* TitleAssetBrowser = "\xEE\xAA\x85 Asset Browser##Asset Browser";
    static constexpr const char* TitleConsole = "\xEE\xAE\x8E Console##Console";
    static constexpr const char* TitlePerformance = "\xEE\xBC\xBE Performance##Performance";
    static constexpr const char* TitleSystems = "\xEE\x93\xBD Systems##Systems";
    static constexpr const char* TitlePropertyEditor = "\xEE\x9D\x85 Property Editor##Property Editor";
    static constexpr const char* TitleTilePalette = "\xEF\x8F\x83 Tile Palette##Tile Palette";
    static constexpr const char* TitleLayers = "\xEF\x94\x80 Layers##Layers";
    static constexpr const char* TitleHierarchy = "\xEE\xA5\xBA Hierarchy##Hierarchy";
    static constexpr const char* TitlePrefabEditor = "\xEF\x9C\xA0 Prefab Editor##Prefab Editor";
}

#endif
