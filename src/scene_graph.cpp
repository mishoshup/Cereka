#include "scene_graph.hpp"
#include "cereka_safe_parse.hpp"
#include <algorithm>

namespace cereka {

SceneGraph::SceneGraph()
{
    root_ = std::make_unique<SceneNode>();
    root_->id = "__root__";
}

SceneNode *SceneGraph::createNode(const std::string &id)
{
    if (nodeMap_.count(id))
        return nodeMap_[id];

    auto node = std::make_unique<SceneNode>();
    node->id = id;
    node->parent = root_.get();
    auto *ptr = node.get();
    root_->children.push_back(std::move(node));
    nodeMap_[id] = ptr;
    return ptr;
}

void SceneGraph::removeNode(const std::string &id)
{
    auto it = nodeMap_.find(id);
    if (it == nodeMap_.end())
        return;

    SceneNode *node = it->second;
    if (!node->parent)
        return;

    auto &siblings = node->parent->children;
    auto pos = std::find_if(siblings.begin(), siblings.end(),
                            [id](const auto &child) { return child->id == id; });
    if (pos != siblings.end()) {
        siblings.erase(pos);
    }
    nodeMap_.erase(it);
}

SceneNode *SceneGraph::findNode(const std::string &id)
{
    auto it = nodeMap_.find(id);
    return it != nodeMap_.end() ? it->second : nullptr;
}

bool SceneGraph::hasNode(const std::string &id) const
{
    return nodeMap_.count(id) > 0;
}

void SceneGraph::updateTransforms()
{
    SceneNode::Transform identity;
    identity.x = 0.0f;
    identity.y = 0.0f;
    identity.scaleX = 1.0f;
    identity.scaleY = 1.0f;
    identity.rotationDeg = 0.0f;
    identity.opacity = 1.0f;
    for (auto &child : root_->children) {
        updateNode(*child, identity);
    }
}

void SceneGraph::updateNode(SceneNode &node, const SceneNode::Transform &parentAccum)
{
    node.world.x = parentAccum.x + node.local.x * parentAccum.scaleX;
    node.world.y = parentAccum.y + node.local.y * parentAccum.scaleY;
    node.world.scaleX = parentAccum.scaleX * node.local.scaleX;
    node.world.scaleY = parentAccum.scaleY * node.local.scaleY;
    node.world.rotationDeg = parentAccum.rotationDeg + node.local.rotationDeg;
    node.world.opacity = parentAccum.opacity * node.local.opacity;

    for (auto &child : node.children) {
        updateNode(*child, node.world);
    }
}

void SceneGraph::visit(const std::function<void(const SceneNode &)> &visitor) const
{
    for (const auto &child : root_->children) {
        visitNode(*child, visitor);
    }
}

void SceneGraph::visitNode(const SceneNode &node,
                           const std::function<void(const SceneNode &)> &visitor) const
{
    if (node.visible)
        visitor(node);
    for (const auto &child : node.children) {
        visitNode(*child, visitor);
    }
}

static float parseFloatSafe(const std::string &s)
{
    if (s.empty())
        return 0.5f;
    auto r = safe_stof(s);
    return r.value_or(0.5f);
}

static std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void SceneGraph::setTransform(const std::string &id, const std::string &propStr)
{
    auto *node = findNode(id);
    if (!node) {
        node = createNode(id);
    }

    std::string s = trim(propStr);
    std::string::size_type i = 0;
    while (i < s.size()) {
        // Skip whitespace
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (i >= s.size()) break;

        // Find keyword start
        std::string::size_type kwStart = i;
        while (i < s.size() && s[i] != '(')
            i++;
        if (i >= s.size()) break;
        std::string keyword = s.substr(kwStart, i - kwStart);
        i++; // skip '('

        // Find matching ')'
        std::string::size_type parenStart = i;
        int depth = 1;
        while (i < s.size() && depth > 0) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') depth--;
            if (depth > 0) i++;
        }
        std::string inner = s.substr(parenStart, i - parenStart);
        if (i < s.size()) i++; // skip ')'

        if (keyword == "pos") {
            auto comma = inner.find(',');
            if (comma != std::string::npos) {
                node->local.x = parseFloatSafe(trim(inner.substr(0, comma)));
                node->local.y = parseFloatSafe(trim(inner.substr(comma + 1)));
            }
        } else if (keyword == "scale") {
            auto comma = inner.find(',');
            if (comma != std::string::npos) {
                node->local.scaleX = parseFloatSafe(trim(inner.substr(0, comma)));
                node->local.scaleY = parseFloatSafe(trim(inner.substr(comma + 1)));
            } else {
                float v = parseFloatSafe(inner);
                node->local.scaleX = v;
                node->local.scaleY = v;
            }
        } else if (keyword == "rotate") {
            node->local.rotationDeg = parseFloatSafe(inner);
        } else if (keyword == "opacity") {
            node->local.opacity = parseFloatSafe(inner);
        }
    }
}

void SceneGraph::Clear()
{
    nodeMap_.clear();
    root_->children.clear();
}

} // namespace cereka
