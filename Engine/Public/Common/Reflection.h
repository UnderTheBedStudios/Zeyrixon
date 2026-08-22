#pragma once

#include <functional>
#include <string>
#include <vector>
#include <glm.hpp>

class BaseComponent;

// What kind of value a reflected field holds. The Editor uses this tag to pick the right
// ImGui widget (DragFloat vs DragFloat3 vs Checkbox) without needing to know the concrete
// component type at all — that's the whole point of doing this generically.
enum class FieldType { Float, Vec3, Bool };

// One reflected property on a component: a display name, its type tag, and type-erased
// getter/setter pairs. The REFLECT_* macros below populate exactly one Get*/Set* pair per
// field, matching Type — the others are left as empty std::function and never called.
struct FieldInfo
{
    std::string Name;
    FieldType Type = FieldType::Float;

    std::function<float(BaseComponent*)> GetFloat;
    std::function<void(BaseComponent*, float)> SetFloat;

    std::function<glm::vec3(BaseComponent*)> GetVec3;
    std::function<void(BaseComponent*, glm::vec3)> SetVec3;

    std::function<bool(BaseComponent*)> GetBool;
    std::function<void(BaseComponent*, bool)> SetBool;
};

// Per-class registry of reflected fields, populated by REFLECT_* macros at static-init time.
// This is a function-local static (lazily initialized on first call), which is what keeps it
// safe regardless of global static-init order across translation units — every REFLECT_*
// registrar only ever touches its own class's registry via this function, never anyone
// else's, so it doesn't matter which TU's globals construct first.
template<typename T>
std::vector<FieldInfo>& GetFieldRegistry()
{
    static std::vector<FieldInfo> fields;
    return fields;
}

// Registers one float-valued field, e.g.:
//   REFLECT_FLOAT(PhysicsComponent, "Mass", GetMass, SetMass)
// Getter/Setter are real method names on ClassName — the macro just wraps them in
// type-erased lambdas so the generic drawing code in the Editor never needs to know
// ClassName exists.
#define REFLECT_FLOAT(ClassName, DisplayName, Getter, Setter) \
    namespace { \
        struct ClassName##_##Getter##_Registrar { \
            ClassName##_##Getter##_Registrar() { \
                FieldInfo info; \
                info.Name = DisplayName; \
                info.Type = FieldType::Float; \
                info.GetFloat = [](BaseComponent* c) { return static_cast<ClassName*>(c)->Getter(); }; \
                info.SetFloat = [](BaseComponent* c, float v) { static_cast<ClassName*>(c)->Setter(v); }; \
                GetFieldRegistry<ClassName>().push_back(info); \
            } \
        } ClassName##_##Getter##_registrar_instance; \
    }

// Registers one glm::vec3-valued field, e.g.:
//   REFLECT_VEC3(TransformComponent, "Position", GetPosition, SetPosition)
#define REFLECT_VEC3(ClassName, DisplayName, Getter, Setter) \
    namespace { \
        struct ClassName##_##Getter##_Registrar { \
            ClassName##_##Getter##_Registrar() { \
                FieldInfo info; \
                info.Name = DisplayName; \
                info.Type = FieldType::Vec3; \
                info.GetVec3 = [](BaseComponent* c) { return static_cast<ClassName*>(c)->Getter(); }; \
                info.SetVec3 = [](BaseComponent* c, glm::vec3 v) { static_cast<ClassName*>(c)->Setter(v); }; \
                GetFieldRegistry<ClassName>().push_back(info); \
            } \
        } ClassName##_##Getter##_registrar_instance; \
    }

// Registers one bool-valued field, e.g.:
//   REFLECT_BOOL(PhysicsComponent, "Is Kinematic", GetKinematic, SetKinematic)
#define REFLECT_BOOL(ClassName, DisplayName, Getter, Setter) \
    namespace { \
        struct ClassName##_##Getter##_Registrar { \
            ClassName##_##Getter##_Registrar() { \
                FieldInfo info; \
                info.Name = DisplayName; \
                info.Type = FieldType::Bool; \
                info.GetBool = [](BaseComponent* c) { return static_cast<ClassName*>(c)->Getter(); }; \
                info.SetBool = [](BaseComponent* c, bool v) { static_cast<ClassName*>(c)->Setter(v); }; \
                GetFieldRegistry<ClassName>().push_back(info); \
            } \
        } ClassName##_##Getter##_registrar_instance; \
    }

// Place inside a BaseComponent-derived class's public section so the Editor can retrieve its
// reflected fields and a display name generically, without ever knowing the concrete type:
//   class PhysicsComponent : public BaseComponent
//   {
//   public:
//       REFLECTABLE(PhysicsComponent, "Physics")
//       ...
//   };
#define REFLECTABLE(ClassName, DisplayName) \
    const std::vector<FieldInfo>* GetReflectedFields() const override { return &GetFieldRegistry<ClassName>(); } \
    const char* GetComponentTypeName() const override { return DisplayName; }
