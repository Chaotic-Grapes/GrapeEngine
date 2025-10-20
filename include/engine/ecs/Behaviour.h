#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

class World;
class Behaviour {
public:
    virtual ~Behaviour() = default;

    // **************** Lifecycle **************** //
    /**
	 * @brief Called when the behaviour is created.
     */
    virtual void Awake() {}

    /**
	 * @brief Called when the behaviour is enabled.
     */
    virtual void OnEnable() {}

    /**
	 * @brief Called before the first frame update, if the behaviour is enabled.
     */
    virtual void Start() {}

    /**
	 * @brief Called when the behaviour is disabled.
     */
    virtual void OnDisable() {}

    /**
	 * @brief Called when the behaviour is destroyed.
     */
    virtual void OnDestroy() {}

    // ************** Frame Updates ************** //
    /**
	 * @brief Called every fixed frame-rate frame, if the behaviour is enabled.
     */
    virtual void FixedUpdate() {}

    /**
	 * @brief Called every frame, if the behaviour is enabled.
     */
    virtual void Update() {}

    /**
	 * @brief Called every frame after Update(), if the behaviour is enabled.
     */
    virtual void LateUpdate() {}

    void SetEnabled(const bool enabled) {
        if (_enabled == enabled)
            return;
        _enabled = enabled;

        if (_enabled)
            OnEnable();
        else
            OnDisable();
    }

    bool IsEnabled() const { return _enabled; }

private:
    friend class World;
    bool _enabled = true;
    bool _started = false;
};

#endif
