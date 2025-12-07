/* Start Header *****************************************************************/
/*!
\file   EditorState.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Header for EditorState enum defining editor playback states.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_STATE_H
#define EDITOR_STATE_H

// Editor runtime states used by the editor UI and playback system.
enum class EditorState {
    Edit,   // Editing scene, most systems disabled
    Play,   // Playing game in editor, all systems enabled
    Paused, // Game paused, can inspect but systems don't update
    Step    // Step one frame while paused
};

#endif
