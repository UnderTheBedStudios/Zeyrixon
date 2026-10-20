#include <pch.h>

#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/Core/Input.h>
#include <Zeyrixon/Core/KeyCodes.h>
#include <Zeyrixon/Core/MouseButtonCodes.h>

#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>
#include <scriptbuilder/scriptbuilder.h>
#include <autowrapper/aswrappedcall.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

namespace Zeyrixon
{
    namespace
    {
        // ------------------------------------------------------------------------------------
        //  Vec3: the vector type scripts see. It is its own small struct (not glm::vec3) so the
        //  script API stays stable even if the math library changes.
        // ------------------------------------------------------------------------------------
        struct ScriptVec3
        {
            float x = 0.f, y = 0.f, z = 0.f;

            ScriptVec3() = default;
            ScriptVec3(float x, float y, float z) : x(x), y(y), z(z) {}
            ScriptVec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
            operator glm::vec3() const { return { x, y, z }; }

            ScriptVec3 Add(const ScriptVec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
            ScriptVec3 Sub(const ScriptVec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
            ScriptVec3 Mul(float s) const { return { x * s, y * s, z * s }; }
            ScriptVec3 Div(float s) const { return { x / s, y / s, z / s }; }
            ScriptVec3 Neg() const { return { -x, -y, -z }; }

            ScriptVec3& AddAssign(const ScriptVec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
            ScriptVec3& SubAssign(const ScriptVec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
            ScriptVec3& MulAssign(float s) { x *= s; y *= s; z *= s; return *this; }

            bool Equals(const ScriptVec3& o) const { return x == o.x && y == o.y && z == o.z; }

            float Dot(const ScriptVec3& o) const { return x * o.x + y * o.y + z * o.z; }
            ScriptVec3 Cross(const ScriptVec3& o) const
            {
                return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
            }
            float LengthSquared() const { return Dot(*this); }
            float Length() const { return std::sqrt(LengthSquared()); }
            float Distance(const ScriptVec3& o) const { return Sub(o).Length(); }
            ScriptVec3 Normalized() const
            {
                const float length = Length();
                return length > 0.f ? Div(length) : ScriptVec3{};
            }
        };

        // ------------------------------------------------------------------------------------
        //  Vec2, the vector type scripts see. It is its own small struct (not glm::vec2) so the
        //  script API stays stable even if the math library changes.
        // ------------------------------------------------------------------------------------
        struct ScriptVec2
        {
            float x = 0.f, y = 0.f;

            ScriptVec2() = default;
            ScriptVec2(float x, float y) : x(x), y(y) {}
            ScriptVec2(const glm::vec2& v) : x(v.x), y(v.y) {}
            operator glm::vec2() const { return { x, y }; }

            ScriptVec2 Add(const ScriptVec2& o) const { return { x + o.x, y + o.y }; }
            ScriptVec2 Sub(const ScriptVec2& o) const { return { x - o.x, y - o.y }; }
            ScriptVec2 Mul(float s) const { return { x * s, y * s }; }
            ScriptVec2 Div(float s) const { return { x / s, y / s }; }
            ScriptVec2 Neg() const { return { -x, -y }; }

            ScriptVec2& AddAssign(const ScriptVec2& o) { x += o.x; y += o.y; return *this; }
            ScriptVec2& SubAssign(const ScriptVec2& o) { x -= o.x; y -= o.y; return *this; }
            ScriptVec2& MulAssign(float s) { x *= s; y *= s; return *this; }

            bool Equals(const ScriptVec2& o) const { return x == o.x && y == o.y; }

            float Dot(const ScriptVec2& o) const { return x * o.x + y * o.y; }
            float Cross(const ScriptVec2& o) const   // 2D cross product is a scalar
            {
                return x * o.y - y * o.x;
            }
            float LengthSquared() const { return Dot(*this); }
            float Length() const { return std::sqrt(LengthSquared()); }
            float Distance(const ScriptVec2& o) const { return Sub(o).Length(); }
            ScriptVec2 Normalized() const
            {
                const float length = Length();
                return length > 0.f ? Div(length) : ScriptVec2{};
            }
        };

        // ------------------------------------------------------------------------------------
        //  Entity: what a script holds to refer to an entity. It is a handle, not the entity
        //  itself: it remembers which entity and which world, and checks that the entity still
        //  exists every time it is used.
        // ------------------------------------------------------------------------------------
        World* s_CurrentWorld = nullptr;

        struct ScriptEntity
        {
            entt::entity Handle = entt::null;
            World* WorldPtr = nullptr;

            bool IsValid() const
            {
                return WorldPtr && WorldPtr == s_CurrentWorld && WorldPtr->GetRegistry().valid(Handle);
            }

            // Returns null (and logs) when the entity is gone or was never set.
            template<typename T>
            T* Get(const char* what) const
            {
                if (!IsValid())
                {
                    Z_ERROR("Script used '{0}' on an invalid Entity (is the 'Entity entity;' member "
                            "declared, and was the entity deleted?)", what);
                    return nullptr;
                }
                return WorldPtr->GetRegistry().try_get<T>(Handle);
            }

            std::string GetName() const
            {
                auto* tag = Get<TagComponent>("name");
                return tag ? tag->Tag : std::string();
            }
            void SetName(const std::string& name)
            {
                if (auto* tag = Get<TagComponent>("name"))
                    tag->Tag = name;
            }

            ScriptVec3 GetPosition() const
            {
                auto* t = Get<TransformComponent>("position");
                return t ? ScriptVec3(t->Position) : ScriptVec3();
            }
            void SetPosition(const ScriptVec3& v)
            {
                if (auto* t = Get<TransformComponent>("position"))
                    t->Position = v;
            }

            // Scripts work in degrees (like the editor's Properties panel); the component stores radians.
            ScriptVec3 GetRotation() const
            {
                auto* t = Get<TransformComponent>("rotation");
                return t ? ScriptVec3(glm::degrees(t->Rotation)) : ScriptVec3();
            }
            void SetRotation(const ScriptVec3& degrees)
            {
                if (auto* t = Get<TransformComponent>("rotation"))
                    t->Rotation = glm::radians(glm::vec3(degrees));
            }

            ScriptVec3 GetScale() const
            {
                auto* t = Get<TransformComponent>("scale");
                return t ? ScriptVec3(t->Scale) : ScriptVec3(1.f, 1.f, 1.f);
            }
            void SetScale(const ScriptVec3& v)
            {
                if (auto* t = Get<TransformComponent>("scale"))
                    t->Scale = v;
            }

            // Which way the entity faces. Same order as TransformComponent::GetMatrix, and like
            // the camera, "forward" is the local -Z axis.
            glm::mat3 RotationMatrix() const
            {
                auto* t = Get<TransformComponent>("Forward/Right/Up");
                if (!t)
                    return glm::mat3(1.f);

                const glm::mat4 identity(1.f);
                return glm::mat3(
                    glm::rotate(identity, t->Rotation.z, glm::vec3(0.f, 0.f, 1.f))
                  * glm::rotate(identity, t->Rotation.y, glm::vec3(0.f, 1.f, 0.f))
                  * glm::rotate(identity, t->Rotation.x, glm::vec3(1.f, 0.f, 0.f)));
            }
            ScriptVec3 Forward() const { return ScriptVec3(RotationMatrix() * glm::vec3(0.f, 0.f, -1.f)); }
            ScriptVec3 Right()   const { return ScriptVec3(RotationMatrix() * glm::vec3(1.f, 0.f, 0.f)); }
            ScriptVec3 Up()      const { return ScriptVec3(RotationMatrix() * glm::vec3(0.f, 1.f, 0.f)); }

            asQWORD GetID() const
            {
                auto* id = Get<IDComponent>("id");
                return id ? id->ID : 0;
            }
        };

        // ------------------------------------------------------------------------------------
        //  Free functions scripts can call
        // ------------------------------------------------------------------------------------
        void LogTrace(const std::string& message) { Z_TRACE("{0}", message); }
        void LogInfo(const std::string& message)  { Z_INFO("{0}", message); }
        void LogWarn(const std::string& message)  { Z_WARN("{0}", message); }
        void LogError(const std::string& message) { Z_ERROR("{0}", message); }

        bool InputIsKeyDown(int key)            { return Input::IsKeyPressed(key); }
        bool InputIsMouseDown(int button)       { return Input::IsMouseButtonPressed(button); }
        float InputMouseX()                     { return Input::GetMouseX(); }
        float InputMouseY()                     { return Input::GetMouseY(); }

        int  InputGetMouseMode()                { return static_cast<int>(Input::GetCursorMode()); }
        void InputSetMouseMode(int mode)        { Input::SetCursorMode(static_cast<CursorMode>(mode)); }

        ScriptEntity FindEntity(const std::string& name)
        {
            if (!s_CurrentWorld)
                return {};

            auto view = s_CurrentWorld->GetRegistry().view<TagComponent>();
            for (auto handle : view)
            {
                if (view.get<TagComponent>(handle).Tag == name)
                    return { handle, s_CurrentWorld };
            }
            return {};
        }

        // ------------------------------------------------------------------------------------
        //  Engine state
        // ------------------------------------------------------------------------------------
        struct Key { const char* Name; int Code; };
        const Key s_Keys[] =
        {
#include "ScriptKeyTable.inl"
        };

        struct ScriptInstance
        {
            entt::entity EntityHandle = entt::null;
            std::string Description;                  // "Player on 'Hero'", for error messages
            asIScriptObject* Object = nullptr;
            asIScriptFunction* OnCreate = nullptr;
            asIScriptFunction* OnUpdate = nullptr;
            asIScriptFunction* OnDestroy = nullptr;
            bool Faulted = false;                     // threw an exception: never called again
        };

        struct Data
        {
            asIScriptEngine* Engine = nullptr;
            asIScriptContext* Context = nullptr;
            asIScriptModule* Module = nullptr;
            std::vector<ScriptInstance> Instances;
            bool Running = false;
        };

        Data* s_Data = nullptr;

        // Kept outside Data so a project can be loaded before the engine exists.
        std::string s_AssetRoot;

        void MessageCallback(const asSMessageInfo* msg, void*)
        {
            const std::string where = std::string(msg->section) + " (" + std::to_string(msg->row) + ", "
                                    + std::to_string(msg->col) + ")";
            switch (msg->type)
            {
                case asMSGTYPE_ERROR:       Z_ERROR("[Script] {0}: {1}", where, msg->message); break;
                case asMSGTYPE_WARNING:     Z_WARN("[Script] {0}: {1}", where, msg->message); break;
                default:                    Z_INFO("[Script] {0}: {1}", where, msg->message); break;
            }
        }

        // Registering is a long list of calls that must all succeed; one bad declaration string
        // otherwise fails silently and shows up much later as "unknown identifier" in a script.
        bool Check(int result, const char* what)
        {
            if (result < 0)
            {
                Z_CORE_ERROR("AngelScript registration failed ({0}): code {1}", what, result);
                return false;
            }
            return true;
        }
#define AS_REG(call) ok &= Check((call), #call)

        bool RegisterVec3(asIScriptEngine* engine)
        {
            bool ok = true;
            AS_REG(engine->RegisterObjectType("Vec3", sizeof(ScriptVec3),
                asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptVec3>()));

            AS_REG(engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(ScriptVec3, x)));
            AS_REG(engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(ScriptVec3, y)));
            AS_REG(engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(ScriptVec3, z)));

            AS_REG(engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()",
                WRAP_CON(ScriptVec3, (void)), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float, float, float)",
                WRAP_CON(ScriptVec3, (float, float, float)), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opAdd(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Add), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opSub(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Sub), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opMul(float) const", WRAP_MFN(ScriptVec3, Mul), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opMul_r(float) const", WRAP_MFN(ScriptVec3, Mul), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opDiv(float) const", WRAP_MFN(ScriptVec3, Div), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 opNeg() const", WRAP_MFN(ScriptVec3, Neg), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 &opAddAssign(const Vec3 &in)", WRAP_MFN(ScriptVec3, AddAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 &opSubAssign(const Vec3 &in)", WRAP_MFN(ScriptVec3, SubAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 &opMulAssign(float)", WRAP_MFN(ScriptVec3, MulAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "bool opEquals(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Equals), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Vec3", "float Dot(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Dot), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 Cross(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Cross), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "float Length() const", WRAP_MFN(ScriptVec3, Length), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "float LengthSquared() const", WRAP_MFN(ScriptVec3, LengthSquared), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "float Distance(const Vec3 &in) const", WRAP_MFN(ScriptVec3, Distance), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec3", "Vec3 Normalized() const", WRAP_MFN(ScriptVec3, Normalized), asCALL_GENERIC));
            return ok;
        }

        bool RegisterVec2(asIScriptEngine* engine)
        {
            bool ok = true;

            AS_REG(engine->RegisterObjectType("Vec2", sizeof(ScriptVec2),
                asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptVec2>()));

            AS_REG(engine->RegisterObjectProperty("Vec2", "float x", asOFFSET(ScriptVec2, x)));
            AS_REG(engine->RegisterObjectProperty("Vec2", "float y", asOFFSET(ScriptVec2, y)));

            AS_REG(engine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f()",
                WRAP_CON(ScriptVec2, (void)), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f(float, float)",
                WRAP_CON(ScriptVec2, (float, float)), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opAdd(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Add), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opSub(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Sub), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opMul(float) const", WRAP_MFN(ScriptVec2, Mul), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opMul_r(float) const", WRAP_MFN(ScriptVec2, Mul), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opDiv(float) const", WRAP_MFN(ScriptVec2, Div), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 opNeg() const", WRAP_MFN(ScriptVec2, Neg), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 &opAddAssign(const Vec2 &in)", WRAP_MFN(ScriptVec2, AddAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 &opSubAssign(const Vec2 &in)", WRAP_MFN(ScriptVec2, SubAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 &opMulAssign(float)", WRAP_MFN(ScriptVec2, MulAssign), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "bool opEquals(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Equals), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Vec2", "float Dot(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Dot), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "float Cross(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Cross), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "float Length() const", WRAP_MFN(ScriptVec2, Length), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "float LengthSquared() const", WRAP_MFN(ScriptVec2, LengthSquared), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "float Distance(const Vec2 &in) const", WRAP_MFN(ScriptVec2, Distance), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Vec2", "Vec2 Normalized() const", WRAP_MFN(ScriptVec2, Normalized), asCALL_GENERIC));
            return ok;
        }

        bool RegisterEntity(asIScriptEngine* engine)
        {
            bool ok = true;
            AS_REG(engine->RegisterObjectType("Entity", sizeof(ScriptEntity),
                asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptEntity>()));

            AS_REG(engine->RegisterObjectBehaviour("Entity", asBEHAVE_CONSTRUCT, "void f()",
                WRAP_CON(ScriptEntity, (void)), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Entity", "bool IsValid() const", WRAP_MFN(ScriptEntity, IsValid), asCALL_GENERIC));

            // "property" accessors let a script write entity.position instead of GetPosition().
            AS_REG(engine->RegisterObjectMethod("Entity", "string get_name() const property", WRAP_MFN(ScriptEntity, GetName), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "void set_name(const string &in) property", WRAP_MFN(ScriptEntity, SetName), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 get_position() const property", WRAP_MFN(ScriptEntity, GetPosition), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "void set_position(const Vec3 &in) property", WRAP_MFN(ScriptEntity, SetPosition), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 get_rotation() const property", WRAP_MFN(ScriptEntity, GetRotation), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "void set_rotation(const Vec3 &in) property", WRAP_MFN(ScriptEntity, SetRotation), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 get_scale() const property", WRAP_MFN(ScriptEntity, GetScale), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "void set_scale(const Vec3 &in) property", WRAP_MFN(ScriptEntity, SetScale), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "uint64 get_id() const property", WRAP_MFN(ScriptEntity, GetID), asCALL_GENERIC));

            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 Forward() const", WRAP_MFN(ScriptEntity, Forward), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 Right() const", WRAP_MFN(ScriptEntity, Right), asCALL_GENERIC));
            AS_REG(engine->RegisterObjectMethod("Entity", "Vec3 Up() const", WRAP_MFN(ScriptEntity, Up), asCALL_GENERIC));

            AS_REG(engine->RegisterGlobalFunction("Entity FindEntity(const string &in)", WRAP_FN(FindEntity), asCALL_GENERIC));
            return ok;
        }

        bool RegisterLogAndInput(asIScriptEngine* engine)
        {
            bool ok = true;

            AS_REG(engine->RegisterGlobalFunction("void Print(const string &in)", WRAP_FN(LogInfo), asCALL_GENERIC));

            AS_REG(engine->SetDefaultNamespace("Log"));
            AS_REG(engine->RegisterGlobalFunction("void Trace(const string &in)", WRAP_FN(LogTrace), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("void Info(const string &in)", WRAP_FN(LogInfo), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("void Warn(const string &in)", WRAP_FN(LogWarn), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("void Error(const string &in)", WRAP_FN(LogError), asCALL_GENERIC));

            // Enums keep a script from passing a random int as a key: Input::IsKeyDown(Key::W).
            AS_REG(engine->SetDefaultNamespace(""));
            AS_REG(engine->RegisterEnum("Key"));
            for (const Key& key : s_Keys)
                AS_REG(engine->RegisterEnumValue("Key", key.Name, key.Code));

            AS_REG(engine->RegisterEnum("MouseButton"));
            AS_REG(engine->RegisterEnumValue("MouseButton", "Left", Z_MOUSE_BUTTON_LEFT));
            AS_REG(engine->RegisterEnumValue("MouseButton", "Right", Z_MOUSE_BUTTON_RIGHT));
            AS_REG(engine->RegisterEnumValue("MouseButton", "Middle", Z_MOUSE_BUTTON_MIDDLE));

            AS_REG(engine->RegisterEnum("CursorMode"));
            AS_REG(engine->RegisterEnumValue("CursorMode", "Normal", static_cast<int>(Zeyrixon::CursorMode::Normal)));
            AS_REG(engine->RegisterEnumValue("CursorMode", "Hidden", static_cast<int>(Zeyrixon::CursorMode::Hidden)));
            AS_REG(engine->RegisterEnumValue("CursorMode", "Locked", static_cast<int>(Zeyrixon::CursorMode::Locked)));

            AS_REG(engine->SetDefaultNamespace("Input"));
            AS_REG(engine->RegisterGlobalFunction("bool IsKeyDown(Key)", WRAP_FN(InputIsKeyDown), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("bool IsMouseDown(MouseButton)", WRAP_FN(InputIsMouseDown), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("float MouseX()", WRAP_FN(InputMouseX), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("float MouseY()", WRAP_FN(InputMouseY), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("CursorMode GetMouseMode()", WRAP_FN(InputGetMouseMode), asCALL_GENERIC));
            AS_REG(engine->RegisterGlobalFunction("void SetMouseMode(CursorMode)", WRAP_FN(InputSetMouseMode), asCALL_GENERIC));

            AS_REG(engine->SetDefaultNamespace(""));
            return ok;
        }

        // ------------------------------------------------------------------------------------
        //  Running scripts
        // ------------------------------------------------------------------------------------
        void LogException(asIScriptContext* ctx, const ScriptInstance& instance)
        {
            const char* section = nullptr;
            const int line = ctx->GetExceptionLineNumber(nullptr, &section);

            Z_ERROR("[Script] Exception in {0}: '{1}' at {2}:{3}. This script is disabled until you press Stop.",
                    instance.Description, ctx->GetExceptionString(), section ? section : "?", line);
        }

        // Calls one script method. Returns false if the script threw (and logs it).
        bool Call(ScriptInstance& instance, asIScriptFunction* function, const float* dt = nullptr)
        {
            asIScriptContext* ctx = s_Data->Context;

            if (ctx->Prepare(function) < 0)
                return false;
            ctx->SetObject(instance.Object);
            if (dt)
                ctx->SetArgFloat(0, *dt);

            const int result = ctx->Execute();
            if (result == asEXECUTION_FINISHED)
                return true;

            if (result == asEXECUTION_EXCEPTION)
                LogException(ctx, instance);
            else
                Z_ERROR("[Script] {0} did not finish normally (code {1})", instance.Description, result);
            return false;
        }

        // Gives the script's `Entity entity;` member its value.
        void InjectEntity(asIScriptObject* object, ScriptEntity entity)
        {
            const int entityTypeId = s_Data->Engine->GetTypeIdByDecl("Entity");

            for (asUINT i = 0; i < object->GetPropertyCount(); ++i)
            {
                if (object->GetPropertyTypeId(i) == entityTypeId && std::string(object->GetPropertyName(i)) == "entity")
                {
                    *static_cast<ScriptEntity*>(object->GetAddressOfProperty(i)) = entity;
                    return;
                }
            }
        }

        std::string ResolvePath(const std::string& scriptPath)
        {
            fs::path path(scriptPath);
            if (path.is_relative())
                path = fs::path(s_AssetRoot) / path;
            return fs::absolute(path).lexically_normal().string();
        }

        void ReleaseInstances(World* worldForDestroyCalls)
        {
            for (ScriptInstance& instance : s_Data->Instances)
            {
                if (!instance.Object)
                    continue;

                // OnDestroy only runs while the world is still around to talk to, and only for
                // scripts that are healthy and whose entity still exists.
                if (worldForDestroyCalls && instance.OnDestroy && !instance.Faulted
                    && worldForDestroyCalls->GetRegistry().valid(instance.EntityHandle))
                {
                    Call(instance, instance.OnDestroy);
                }

                instance.Object->Release();
            }
            s_Data->Instances.clear();

            if (s_Data->Module)
            {
                s_Data->Module->Discard();
                s_Data->Module = nullptr;
            }
            // Script objects can reference each other; the garbage collector frees such cycles.
            s_Data->Engine->GarbageCollect();
        }
    }

    // ----------------------------------------------------------------------------------------
    //  Public API
    // ----------------------------------------------------------------------------------------
    void ScriptEngine::Init()
    {
        if (s_Data)
            return;

        s_Data = new Data();

        asIScriptEngine* engine = asCreateScriptEngine();
        if (!engine)
        {
            Z_CORE_CRITICAL("Failed to create the AngelScript engine");
            delete s_Data;
            s_Data = nullptr;
            return;
        }
        s_Data->Engine = engine;

        engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
        engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true);   // Key::W, never bare W

        RegisterStdString(engine);
        RegisterScriptArray(engine, true);
        RegisterStdStringUtils(engine);
        RegisterScriptMath(engine);

        bool ok = true;
        ok &= RegisterVec3(engine);
        ok &= RegisterVec2(engine);
        ok &= RegisterEntity(engine);
        ok &= RegisterLogAndInput(engine);
        if (!ok)
            Z_CORE_ERROR("Some script API failed to register; see the errors above");

        s_Data->Context = engine->CreateContext();

        Z_CORE_INFO("AngelScript {0} initialized", asGetLibraryVersion());
    }

    void ScriptEngine::Shutdown()
    {
        if (!s_Data)
            return;

        // The world may already be gone, so no script code runs here: objects are just freed.
        ReleaseInstances(nullptr);
        s_CurrentWorld = nullptr;

        s_Data->Context->Release();
        s_Data->Engine->ShutDownAndRelease();

        delete s_Data;
        s_Data = nullptr;
    }

    asIScriptEngine* ScriptEngine::GetEngine()
    {
        return s_Data ? s_Data->Engine : nullptr;
    }

    void ScriptEngine::SetAssetRoot(const std::string& directory)
    {
        s_AssetRoot = directory;
    }

    const std::string& ScriptEngine::GetAssetRoot()
    {
        return s_AssetRoot;
    }

    bool ScriptEngine::IsRunning()
    {
        return s_Data && s_Data->Running;
    }

    bool ScriptEngine::OnRuntimeStart(World& world)
    {
        if (!s_Data)
        {
            Z_CORE_ERROR("ScriptEngine::OnRuntimeStart called before ScriptEngine::Init");
            return false;
        }

        if (s_Data->Running && s_CurrentWorld)
            OnRuntimeStop(*s_CurrentWorld);

        // 1. Collect scripts. Entities are visited oldest-first so OnCreate order is the
        //    order entities appear in the outliner. (EnTT views iterate newest-first.)
        struct Pending { entt::entity Handle; std::string Path; std::string ClassName; std::string EntityName; };
        std::vector<Pending> pending;
        std::set<std::string> files;

        auto view = world.GetRegistry().view<ScriptComponent>();
        std::vector<entt::entity> handles(view.begin(), view.end());
        std::reverse(handles.begin(), handles.end());

        for (entt::entity handle : handles)
        {
            const auto& script = view.get<ScriptComponent>(handle);
            if (script.ScriptPath.empty())
                continue;

            std::string name = "Entity";
            if (auto* tag = world.GetRegistry().try_get<TagComponent>(handle))
                name = tag->Tag;

            const std::string path = ResolvePath(script.ScriptPath);
            files.insert(path);
            pending.push_back({ handle, path, script.GetResolvedClassName(), name });
        }

        s_CurrentWorld = &world;

        if (pending.empty())
        {
            s_Data->Running = true;
            return true;
        }

        // 2. Compile. CScriptBuilder reads each file and also handles `#include "other.as"`.
        CScriptBuilder builder;
        if (builder.StartNewModule(s_Data->Engine, "Game") < 0)
        {
            Z_CORE_ERROR("Failed to start a script module");
            s_CurrentWorld = nullptr;
            return false;
        }

        bool compiled = true;
        for (const std::string& file : files)
            compiled &= builder.AddSectionFromFile(file.c_str()) >= 0;
        compiled &= builder.BuildModule() >= 0;

        if (!compiled)
        {
            Z_ERROR("[Script] Compilation failed, the game was not started. See the errors above.");
            s_Data->Engine->DiscardModule("Game");
            s_CurrentWorld = nullptr;
            return false;
        }

        s_Data->Module = builder.GetModule();

        // 3. Create one object per ScriptComponent and hand it its entity.
        for (const Pending& p : pending)
        {
            ScriptInstance instance;
            instance.EntityHandle = p.Handle;
            instance.Description = p.ClassName + " on '" + p.EntityName + "'";

            asITypeInfo* type = s_Data->Module->GetTypeInfoByName(p.ClassName.c_str());
            if (!type)
            {
                Z_ERROR("[Script] {0}: class '{1}' was not found in {2}", instance.Description, p.ClassName, p.Path);
                continue;
            }

            asIScriptObject* object = static_cast<asIScriptObject*>(s_Data->Engine->CreateScriptObject(type));
            if (!object)
            {
                Z_ERROR("[Script] {0}: could not create the object (does the class have a default constructor?)",
                        instance.Description);
                continue;
            }

            instance.Object = object;
            instance.OnCreate = type->GetMethodByDecl("void OnCreate()");
            instance.OnUpdate = type->GetMethodByDecl("void OnUpdate(float)");
            instance.OnDestroy = type->GetMethodByDecl("void OnDestroy()");
            // These function pointers belong to the module. They stay valid until
            // ReleaseInstances discards the module, which always happens after the instances are gone.

            InjectEntity(object, ScriptEntity{ p.Handle, &world });
            s_Data->Instances.push_back(instance);
        }

        s_Data->Running = true;

        // 4. OnCreate runs only after every object exists, so a script's OnCreate can already
        //    use FindEntity and entities owned by other scripts.
        for (ScriptInstance& instance : s_Data->Instances)
        {
            if (instance.OnCreate && !Call(instance, instance.OnCreate))
                instance.Faulted = true;
        }

        Z_CORE_INFO("Started {0} script(s)", s_Data->Instances.size());
        return true;
    }

    void ScriptEngine::OnRuntimeUpdate(World& world, DeltaTime dt)
    {
        if (!s_Data || !s_Data->Running)
            return;

        const float seconds = dt.GetSeconds();

        for (ScriptInstance& instance : s_Data->Instances)
        {
            if (instance.Faulted || !instance.OnUpdate)
                continue;

            // The entity can be deleted while playing (the editor allows it).
            if (!world.GetRegistry().valid(instance.EntityHandle))
            {
                instance.Faulted = true;
                continue;
            }

            if (!Call(instance, instance.OnUpdate, &seconds))
                instance.Faulted = true;
        }
    }

    void ScriptEngine::OnRuntimeStop(World& world)
    {
        if (!s_Data || !s_Data->Running)
            return;

        ReleaseInstances(&world);
        s_CurrentWorld = nullptr;
        s_Data->Running = false;
    }
}
