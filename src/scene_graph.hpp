#pragma once
#include "renderer/irecture.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cereka {

struct SceneNode {
    std::string id;
    struct Transform {
        float x = 0.5f, y = 0.5f;
        float scaleX = 1.0f, scaleY = 1.0f;
        float rotationDeg = 0.0f;
        float opacity = 1.0f;
    } local;
    Transform world;
    std::shared_ptr<ITexture> texture;
    bool visible = true;
    SceneNode *parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
};

class SceneGraph {
public:
    SceneGraph();
    SceneNode *createNode(const std::string &id);
    void removeNode(const std::string &id);
    SceneNode *findNode(const std::string &id);
    bool hasNode(const std::string &id) const;

    void updateTransforms();
    void visit(const std::function<void(const SceneNode &)> &visitor) const;
    void setTransform(const std::string &id, const std::string &propStr);

    void Clear();

private:
    std::unique_ptr<SceneNode> root_;
    std::unordered_map<std::string, SceneNode *> nodeMap_;
    void updateNode(SceneNode &node, const SceneNode::Transform &parentAccum);
    void visitNode(const SceneNode &node,
                   const std::function<void(const SceneNode &)> &visitor) const;
};

} // namespace cereka
