/* Start Header *****************************************************************/
/*!
\file   AnimationController2D.h
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Defines data structures for 2D animation controllers loaded from .animctrl assets.
*/
/* End Header *******************************************************************/

#pragma once

#include <string>
#include <vector>

namespace Animation {

    /**
     * @brief Represents the type of a parameter used in animation controller conditions, which can be a boolean, integer, or float.
     */
    enum class ParamType : uint8_t {
        Bool = 0,
        Int = 1,
        Float = 2
    };

    /**
     * @brief Represents a comparison operator used in animation transition conditions, such as equal, not equal, less than, or greater than.
     */
    enum class CompareOp : uint8_t {
        Equal = 0,
        NotEqual = 1,
        Less = 2,
        Greater = 3
    };

    /**
     * @brief Represents the definition of a parameter used in an animation controller, including its name, type, and default value.
     */
    struct AnimationParamDef2D {
        std::string Name;
        ParamType Type = ParamType::Float;
        bool DefaultBool = false;
        int DefaultInt = 0;
        float DefaultFloat = 0.0f;
    };

    /**
     * @brief Represents a state in an animation controller, which references an animation clip and defines playback properties such as speed and looping.
     */
    struct AnimationState2D {
        std::string Name;
        std::string ClipPath;
        float Speed = 1.0f;
        bool Loop = true;
    };

    /**
     * @brief Represents a condition for transitioning between animation states, which compares a parameter to a value using a specified operator.
     */
    struct AnimationTransitionCondition2D {
        std::string Param;
        CompareOp Op = CompareOp::Equal;
        float Value = 0.0f;
    };

    /**
     * @brief Represents a transition between two animation states, including the source and destination states, transition conditions, exit time, and duration.
     */
    struct AnimationTransition2D {
        std::string FromState;
        std::string ToState;
        std::vector<AnimationTransitionCondition2D> Conditions;
        float ExitTime = 0.0f;
        float Duration = 0.0f;
    };

    /**
     * @brief Represents the data for a 2D animation controller, including its parameters, states, transitions, and entry state.
     */
    struct AnimationController2DData {
        std::vector<AnimationParamDef2D> Parameters;
        std::vector<AnimationState2D> States;
        std::vector<AnimationTransition2D> Transitions;
        std::string EntryState;
    };
}
