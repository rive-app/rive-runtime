#ifndef _RIVE_STATE_MACHINE_EVENT_BASE_HPP_
#define _RIVE_STATE_MACHINE_EVENT_BASE_HPP_
#include "rive/animation/state_machine_component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive {
    class StateMachineEventBase : public StateMachineComponent {
    protected:
        typedef StateMachineComponent Super;

    public:
        static const uint16_t typeKey = 114;

        /// Helper to quickly determine if a core object extends another without RTTI
        /// at runtime.
        bool isTypeOf(uint16_t typeKey) const override {
            switch (typeKey) {
                case StateMachineEventBase::typeKey:
                case StateMachineComponentBase::typeKey:
                    return true;
                default:
                    return false;
            }
        }

        uint16_t coreType() const override { return typeKey; }

        static const uint16_t targetIdPropertyKey = 224;
        static const uint16_t eventTypeValuePropertyKey = 225;

    private:
        uint32_t m_TargetId = 0;
        uint32_t m_EventTypeValue = 0;

    public:
        inline uint32_t targetId() const { return m_TargetId; }
        void targetId(uint32_t value) {
            if (m_TargetId == value) {
                return;
            }
            m_TargetId = value;
            targetIdChanged();
        }

        inline uint32_t eventTypeValue() const { return m_EventTypeValue; }
        void eventTypeValue(uint32_t value) {
            if (m_EventTypeValue == value) {
                return;
            }
            m_EventTypeValue = value;
            eventTypeValueChanged();
        }

        Core* clone() const override;
        void copy(const StateMachineEventBase& object) {
            m_TargetId = object.m_TargetId;
            m_EventTypeValue = object.m_EventTypeValue;
            StateMachineComponent::copy(object);
        }

        bool deserialize(uint16_t propertyKey, BinaryReader& reader) override {
            switch (propertyKey) {
                case targetIdPropertyKey:
                    m_TargetId = CoreUintType::deserialize(reader);
                    return true;
                case eventTypeValuePropertyKey:
                    m_EventTypeValue = CoreUintType::deserialize(reader);
                    return true;
            }
            return StateMachineComponent::deserialize(propertyKey, reader);
        }

    protected:
        virtual void targetIdChanged() {}
        virtual void eventTypeValueChanged() {}
    };
} // namespace rive

#endif