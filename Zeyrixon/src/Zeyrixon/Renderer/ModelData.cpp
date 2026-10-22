#include <pch.h>

#include <Zeyrixon/Renderer/ModelData.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Zeyrixon
{
    namespace
    {
        // The value of one channel at `time`. Translation and scale use xyz; rotation uses xyzw.
        glm::vec4 SampleChannel(const AnimationChannel& channel, float time)
        {
            const bool rotation = channel.Path == AnimationPath::Rotation;
            const size_t components = rotation ? 4 : 3;
            const bool cubic = channel.Interpolation == AnimationInterpolation::CubicSpline;
            const size_t floatsPerKey = (cubic ? 3 : 1) * components;

            // part: 0 = in-tangent, 1 = value, 2 = out-tangent (only cubic keys have the tangents)
            auto read = [&](size_t key, size_t part)
            {
                const float* p = &channel.Values[key * floatsPerKey + (cubic ? part * components : 0)];
                return glm::vec4(p[0], p[1], p[2], rotation ? p[3] : 0.0f);
            };

            const size_t keyCount = channel.Times.size();
            if (time <= channel.Times.front())
                return read(0, 1);
            if (time >= channel.Times.back())
                return read(keyCount - 1, 1);

            // The first key after `time`; the key before it starts the segment we are in.
            const size_t next = std::upper_bound(channel.Times.begin(), channel.Times.end(), time) - channel.Times.begin();
            const size_t previous = next - 1;

            const float segment = channel.Times[next] - channel.Times[previous];
            if (segment <= 0.0f)
                return read(previous, 1);
            const float t = (time - channel.Times[previous]) / segment;

            switch (channel.Interpolation)
            {
                case AnimationInterpolation::Step:
                    return read(previous, 1);

                case AnimationInterpolation::Linear:
                {
                    const glm::vec4 a = read(previous, 1);
                    const glm::vec4 b = read(next, 1);
                    if (rotation)
                    {
                        // slerp takes the short way round the sphere, which is what we want.
                        const glm::quat q = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), t);
                        return glm::vec4(q.x, q.y, q.z, q.w);
                    }
                    return glm::mix(a, b, t);
                }

                case AnimationInterpolation::CubicSpline:
                {
                    // A cubic Hermite spline. The tangents are stored per second, so they're scaled by the
                    // segment length to match the 0..1 t.
                    const glm::vec4 v0 = read(previous, 1);
                    const glm::vec4 out0 = read(previous, 2) * segment;
                    const glm::vec4 v1 = read(next, 1);
                    const glm::vec4 in1 = read(next, 0) * segment;

                    const float t2 = t * t;
                    const float t3 = t2 * t;
                    glm::vec4 result = (2.0f * t3 - 3.0f * t2 + 1.0f) * v0
                                     + (t3 - 2.0f * t2 + t) * out0
                                     + (-2.0f * t3 + 3.0f * t2) * v1
                                     + (t3 - t2) * in1;
                    if (rotation)
                    {
                        const glm::quat q = glm::normalize(glm::quat(result.w, result.x, result.y, result.z));
                        result = glm::vec4(q.x, q.y, q.z, q.w);
                    }
                    return result;
                }
            }
            return read(previous, 1);
        }
    }

    void ModelData::Finalize()
    {
        const int nodeCount = static_cast<int>(Nodes.size());

        // Order the nodes so a parent always comes before its children. glTF doesn't promise the
        // file lists them that way, and the pose is computed parent-first.
        m_Order.clear();
        m_Order.reserve(Nodes.size());
        std::vector<int> stack;
        for (int root = 0; root < nodeCount; ++root)
        {
            if (Nodes[root].Parent != -1)
                continue;

            stack.push_back(root);
            while (!stack.empty())
            {
                const int node = stack.back();
                stack.pop_back();
                m_Order.push_back(node);
                for (int child : Nodes[node].Children)
                    stack.push_back(child);
            }
        }

        // Animated = targeted by a channel, or below a node that is.
        m_NodeAnimated.assign(Nodes.size(), 0);
        for (const AnimationClip& clip : Animations)
            for (const AnimationChannel& channel : clip.Channels)
                if (channel.Node >= 0 && channel.Node < nodeCount)
                    m_NodeAnimated[channel.Node] = 1;
        for (int node : m_Order)
            if (Nodes[node].Parent >= 0 && m_NodeAnimated[Nodes[node].Parent])
                m_NodeAnimated[node] = 1;

        Evaluate(-1, 0.0f, RestPose);

        InverseRestMatrices.resize(Nodes.size());
        for (size_t i = 0; i < Nodes.size(); ++i)
            InverseRestMatrices[i] = glm::inverse(RestPose.NodeMatrices[i]);
    }

    void ModelData::Evaluate(int clipIndex, float time, ModelPose& pose) const
    {
        const size_t nodeCount = Nodes.size();

        // Start every node at its rest transform, then let the clip overwrite what it animates.
        std::vector<glm::vec3> translation(nodeCount), scale(nodeCount);
        std::vector<glm::quat> rotation(nodeCount);
        for (size_t i = 0; i < nodeCount; ++i)
        {
            translation[i] = Nodes[i].Translation;
            rotation[i] = Nodes[i].Rotation;
            scale[i] = Nodes[i].Scale;
        }

        if (clipIndex >= 0 && clipIndex < static_cast<int>(Animations.size()))
        {
            for (const AnimationChannel& channel : Animations[clipIndex].Channels)
            {
                if (channel.Node < 0 || channel.Node >= static_cast<int>(nodeCount) || channel.Times.empty())
                    continue;

                const glm::vec4 value = SampleChannel(channel, time);
                switch (channel.Path)
                {
                    case AnimationPath::Translation: translation[channel.Node] = glm::vec3(value); break;
                    case AnimationPath::Scale:       scale[channel.Node] = glm::vec3(value); break;
                    case AnimationPath::Rotation:    rotation[channel.Node] = glm::normalize(glm::quat(value.w, value.x, value.y, value.z)); break;
                }
            }
        }

        // Local transforms -> model-space matrices, parents first.
        pose.NodeMatrices.resize(nodeCount);
        for (int node : m_Order)
        {
            const glm::mat4 local = glm::translate(glm::mat4(1.0f), translation[node])
                                  * glm::mat4_cast(rotation[node])
                                  * glm::scale(glm::mat4(1.0f), scale[node]);

            const int parent = Nodes[node].Parent;
            pose.NodeMatrices[node] = parent >= 0 ? pose.NodeMatrices[parent] * local : local;
        }

        // What the vertex shader needs: joint's current matrix * inverse bind matrix.
        pose.JointMatrices.resize(Skins.size());
        for (size_t s = 0; s < Skins.size(); ++s)
        {
            const ModelSkin& skin = Skins[s];
            auto& matrices = pose.JointMatrices[s];
            matrices.resize(skin.Joints.size());
            for (size_t j = 0; j < skin.Joints.size(); ++j)
                matrices[j] = pose.NodeMatrices[skin.Joints[j]] * skin.InverseBindMatrices[j];
        }
    }

    int ModelData::FindAnimation(const std::string& name) const
    {
        for (size_t i = 0; i < Animations.size(); ++i)
            if (Animations[i].Name == name)
                return static_cast<int>(i);
        return -1;
    }
}
