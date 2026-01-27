/* Start Header *****************************************************************/
/*!
\file   SystemsPanel.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares the SystemsPanel class which displays registered C# and C++ systems
in a table with metadata like execution group, enable state, and component access.

The systems panel allows users to:
- View all registered systems grouped by execution phase
- Distinguish between C# scripted and C++ native systems
- Toggle system enable/disable states at runtime
- See component read/write access patterns
- Understand system execution order and metadata
*/
/* End Header *******************************************************************/

#ifndef SYSTEMS_PANEL_H
#define SYSTEMS_PANEL_H

#include <imgui.h>
#include <string>
#include <vector>
#include <map>

// Forward declarations
namespace ECS {
    class SystemManager;
    class World;
}

/**
 * @brief Editor panel for displaying registered ECS systems and their metadata.
 * 
 * Shows all systems registered with the SystemManager, including both
 * native C++ systems and scripted C# systems. Allows toggling enable/disable
 * and viewing system metadata like execution group and component access.
 */
class SystemsPanel {
public:
    /**
     * @brief Initialize the panel with fonts.
     * @param mainFont Main font for regular text
     * @param boldFont Bold font for section headers
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont);

    /**
     * @brief Render the systems panel UI.
     * @param systemManager Pointer to SystemManager to query systems (may be nullptr)
     * 
     * Displays all registered systems in a table format, grouped by execution phase.
     * Allows toggling enable/disable states at runtime.
     */
    void Render(ECS::SystemManager* systemManager);

    /**
     * @brief Set the active world (called when scene changes).
     * @param world Pointer to the ECS World (may be nullptr)
     */
    void SetWorld(ECS::World* world);

    /**
     * @brief Shutdown and cleanup panel resources.
     */
    void Shutdown();

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ECS::World* m_world = nullptr;
    ECS::SystemManager* m_systemManager = nullptr;

    /**
     * @brief Cached information about a single system.
     */
    struct SystemInfo {
        std::string name;              ///< System name from metadata
        std::string typeFullName;      ///< Full C# or C++ type name
        std::string group;             ///< Execution group (Update, Physics, etc.)
        bool isEnabled = true;         ///< Is the system currently enabled?
        bool isScripted = false;       ///< Is this a C# scripted system?
        bool executeInEditMode = false;///< Does it run when editor is paused?
        int sortOrder = 0;             ///< Execution order within group
    };

    /// Cache of all discovered systems
    std::vector<SystemInfo> m_systems;

    /// Selected system for detail view (if any)
    std::string m_selectedSystemName;

    /**
     * @brief Update the cached system list from SystemManager.
     * @param systemManager Pointer to SystemManager
     */
    void _updateSystemsList(ECS::SystemManager* systemManager);

    /**
     * @brief Render a header with total system count.
     */
    void _renderHeader();

    /**
     * @brief Render the table of all systems.
     */
    void _renderSystemsTable();

    /**
     * @brief Render system count statistics.
     */
    void _renderStats();
};

#endif
