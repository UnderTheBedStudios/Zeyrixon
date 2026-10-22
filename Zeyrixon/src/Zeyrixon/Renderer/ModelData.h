#pragma once

#include <Zeyrixon/Core/Core.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace Zeyrixon
{
    /* One node of a model's hierarchy (a glTF "node"). Its transform is stored as translation,
       rotation and scale rather than a matrix, because animations change those three separately. */
    struct ModelNode
    {
        std::string Name;
        int Parent = -1;                                // -1 = a root node
        std::vector<int> Children;

        glm::vec3 Translation{ 0.0f };
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };   // glm order is (w, x, y, z)
        glm::vec3 Scale{ 1.0f };
    };

    /* A skeleton: which nodes are the joints, and each joint's inverse bind matrix (it moves a
       vertex from the mesh's bind-pose space into that joint's own space). */
    struct ModelSkin
    {
        std::string Name;
        std::vector<int> Joints;                        // node indices; position = joint index in the vertices
        std::vector<glm::mat4> InverseBindMatrices;     // same length as Joints
    };

    enum class AnimationPath { Translation, Rotation, Scale };
    enum class AnimationInterpolation { Step, Linear, CubicSpline };

    /* The keyframes for one property (translation, rotation or scale) of one node. */
    struct AnimationChannel
    {
        int Node = -1;
        AnimationPath Path = AnimationPath::Translation;
        AnimationInterpolation Interpolation = AnimationInterpolation::Linear;

        std::vector<float> Times;    // seconds, ascending
        /* 3 floats per key (translation, scale) or 4 (rotation, as x y z w). For CubicSpline
           each key holds three of those: in-tangent, value, out-tangent. */
        std::vector<float> Values;
    };

    struct AnimationClip
    {
        std::string Name;
        float Duration = 0.0f;       // seconds: the last keyframe time of any channel
        std::vector<AnimationChannel> Channels;
    };

    /* The result of evaluating a model at one moment. */
    struct ModelPose
    {
        std::vector<glm::mat4> NodeMatrices;                    // per node, in the model's own space
        std::vector<std::vector<glm::mat4>> JointMatrices;      // per skin, per joint: ready for the vertex shader
    };

    /* Everything a model has besides triangles: the node hierarchy, the skeletons and the
       animations. Filled in by the glTF loader, then Finalize() is called once. Read-only after that. */
    class Z_API ModelData
    {
    public:
        // The vertex shader's joint matrix array is this long. Skins with more joints are clamped.
        static constexpr int MaxJoints = 128;

        std::vector<ModelNode> Nodes;
        std::vector<ModelSkin> Skins;
        std::vector<AnimationClip> Animations;

        // Filled in by Finalize():
        ModelPose RestPose;                                 // the pose with no animation applied
        std::vector<glm::mat4> InverseRestMatrices;         // inverse of RestPose.NodeMatrices, per node

        // Call once after Nodes, Skins and Animations are filled in.
        void Finalize();

        /* Computes the pose at `time` seconds into animation `clipIndex` (time is clamped to the
           clip, so wrap it yourself to loop). A clipIndex outside the list gives the rest pose. */
        void Evaluate(int clipIndex, float time, ModelPose& pose) const;

        // True if any animation moves this node, or one of its ancestors.
        bool IsNodeAnimated(int node) const { return node >= 0 && node < static_cast<int>(m_NodeAnimated.size()) && m_NodeAnimated[node]; }

        int FindAnimation(const std::string& name) const;

    private:
        std::vector<int> m_Order;           // every node, parents before children
        std::vector<char> m_NodeAnimated;
    };
}
