//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../../Foundation/InspectorTab/NodeComponentInspector.h"

#include "../../Project/CreateComponentMenu.h"

#include <Urho3D/Container/TransformedSpan.h>
#include <Urho3D/Resource/ResourceCache.h>

#include <IconFontCppHeaders/IconsFontAwesome6.h>

namespace Urho3D
{

namespace
{

template <class T>
struct WeakToRawStaticCaster
{
    template <class U>
    T operator() (const U& x) const { return static_cast<T>(x.Get()); }
};

template <class T>
auto CastVectorTo(const WeakSerializableVector& objects)
{
    ea::span<const WeakPtr<Serializable>> objectsSpan{objects};
    return TransformedSpan<const WeakPtr<Serializable>, T, WeakToRawStaticCaster<T>>(objectsSpan);
}

NodeInspectorWidget::NodeVector GetSortedTopmostNodes(const WeakSerializableVector& objects)
{
    using NodeAndIndex = ea::pair<Node*, unsigned>;

    ea::unordered_set<Node*> nodeSet;
    for (Serializable* obj : objects)
    {
        if (auto node = dynamic_cast<Node*>(obj))
            nodeSet.insert(node);
        else if (auto component = dynamic_cast<Component*>(obj))
            nodeSet.insert(component->GetNode());
    }

    ea::vector<NodeAndIndex> nodeVector;
    for (Node* node : nodeSet)
    {
        Node* parentNode = node->GetParent();
        nodeVector.emplace_back(node, node->GetIndexInParent());
    }

    for (auto& [node, _] : nodeVector)
    {
        if (!node)
            continue;

        const auto isParentOfNode = [node = node](Node* other) { return node && node->IsChildOf(other); };
        const auto parentIter = ea::find_if(nodeSet.begin(), nodeSet.end(), isParentOfNode);
        if (parentIter != nodeSet.end())
            node = nullptr;
    }
    ea::erase_if(nodeVector, [](const NodeAndIndex& nodeAndIndex) { return !nodeAndIndex.first; });
    ea::sort(nodeVector.begin(), nodeVector.end());

    NodeInspectorWidget::NodeVector result;
    for (const auto& [node, _] : nodeVector)
        result.emplace_back(node);
    return result;
}

bool HasScene(const NodeInspectorWidget::NodeVector& nodes)
{
    for (Node* node : nodes)
    {
        if (node->GetScene() == node)
            return true;
    }
    return false;
}

}

void Foundation_NodeComponentInspector(Context* context, InspectorTab* inspectorTab)
{
    inspectorTab->RegisterAddon<NodeComponentInspector>(inspectorTab->GetProject());
}

NodeComponentInspector::NodeComponentInspector(Project* project)
    : Object(project->GetContext())
    , project_(project)
{
    project_->OnRequest.SubscribeWithSender(this, &NodeComponentInspector::OnProjectRequest);
}

void NodeComponentInspector::OnProjectRequest(RefCounted* senderTab, ProjectRequest* request)
{
    auto inspectedTab = dynamic_cast<EditorTab*>(senderTab);
    if (!inspectedTab)
        return;

    auto inspectNodeComponentRequest = dynamic_cast<InspectNodeComponentRequest*>(request);
    if (!inspectNodeComponentRequest || inspectNodeComponentRequest->IsEmpty())
        return;

    Scene* commonScene = inspectNodeComponentRequest->GetCommonScene();
    if (!commonScene)
        return;

    request->QueueProcessCallback([=]()
    {
        if (nodes_ != inspectNodeComponentRequest->GetNodes()
            || components_ != inspectNodeComponentRequest->GetComponents()
            || inspectedTab_ != inspectedTab)
        {
            nodes_ = inspectNodeComponentRequest->GetNodes();
            components_ = inspectNodeComponentRequest->GetComponents();
            inspectedTab_ = inspectedTab;
            scene_ = commonScene;
            InspectObjects();
        }
        OnActivated(this);
    });
}

NodeComponentInspector::NodeVector NodeComponentInspector::CollectNodes() const
{
    const auto sceneIter = ea::find_if(nodes_.begin(), nodes_.end(),
        [](const Node* node) { return node->GetType() == Scene::GetTypeStatic(); });

    // If scene is selected, inspect only it
    if (sceneIter != nodes_.end())
        return {*sceneIter};

    // If nodes are present, inspect nodes
    if (!nodes_.empty())
        return {nodes_.begin(), nodes_.end()};

    return {};
}

WeakSerializableVector NodeComponentInspector::CollectComponents() const
{
    // If components are of the same type, inspect components
    const bool sameType = ea::all_of(components_.begin(), components_.end(),
        [&](const Component* component) { return component->GetType() == components_[0]->GetType(); });
    if (sameType)
        return {components_.begin(), components_.end()};

    return {};
}

void NodeComponentInspector::InspectObjects()
{
    if (const auto nodes = CollectNodes(); !nodes.empty())
    {
        nodeWidget_ = MakeShared<NodeInspectorWidget>(context_, nodes);
        componentWidget_ = nullptr;
        componentSummary_.clear();

        nodeWidget_->OnEditNodeAttributeBegin.Subscribe(this, &NodeComponentInspector::BeginEditNodeAttribute);
        nodeWidget_->OnEditNodeAttributeEnd.Subscribe(this, &NodeComponentInspector::EndEditNodeAttribute);
        nodeWidget_->OnEditComponentAttributeBegin.Subscribe(this, &NodeComponentInspector::BeginEditComponentAttribute);
        nodeWidget_->OnEditComponentAttributeEnd.Subscribe(this, &NodeComponentInspector::EndEditComponentAttribute);
        nodeWidget_->OnActionBegin.Subscribe(this, &NodeComponentInspector::BeginAction);
        nodeWidget_->OnActionEnd.Subscribe(this, &NodeComponentInspector::EndAction);
        nodeWidget_->OnComponentRemoved.Subscribe(this, &NodeComponentInspector::RemoveComponent);
    }
    else if (const auto components = CollectComponents(); !components.empty())
    {
        nodeWidget_ = nullptr;
        componentWidget_ = MakeShared<SerializableInspectorWidget>(context_, components);
        componentSummary_.clear();

        componentWidget_->OnEditAttributeBegin.Subscribe(this, &NodeComponentInspector::BeginEditComponentAttribute);
        componentWidget_->OnEditAttributeEnd.Subscribe(this, &NodeComponentInspector::EndEditComponentAttribute);
        componentWidget_->OnActionBegin.Subscribe(this, &NodeComponentInspector::BeginAction);
        componentWidget_->OnActionEnd.Subscribe(this, &NodeComponentInspector::EndAction);
    }
    else
    {
        nodeWidget_ = nullptr;
        componentWidget_ = nullptr;
        componentSummary_.clear();

        for (const Component* component : components_)
            ++componentSummary_[component->GetTypeName()];
    }
}

void NodeComponentInspector::BeginEditNodeAttribute(
    const WeakSerializableVector& objects, const AttributeInfo* attribute)
{
    if (objects.empty())
        return;

    // For nodes, attributes are known and almost never change.
    // Currently they all have the smallest scope.
    scopeHint_ = attribute->scopeHint_;
    URHO3D_ASSERT(scopeHint_ == AttributeScopeHint::Attribute);

    const auto nodes = CastVectorTo<Node*>(objects);
    oldState_.values_.clear();
    for (Node* node : nodes)
        oldState_.values_.push_back(node->GetAttribute(attribute->name_));
}

void NodeComponentInspector::EndEditNodeAttribute(const WeakSerializableVector& objects, const AttributeInfo* attribute)
{
    if (objects.empty())
        return;

    const auto nodes = CastVectorTo<Node*>(objects);
    newState_.values_.clear();
    for (Node* node : nodes)
        newState_.values_.push_back(node->GetAttribute(attribute->name_));

    inspectedTab_->PushAction<ChangeNodeAttributesAction>(
        scene_, attribute->name_, nodes, oldState_.values_, newState_.values_);
}

void NodeComponentInspector::BeginEditComponentAttribute(
    const WeakSerializableVector& objects, const AttributeInfo* attribute)
{
    if (objects.empty())
        return;

    scopeHint_ = attribute->scopeHint_;

    const auto components = CastVectorTo<Component*>(objects);
    const auto nodes = Node::GetNodes({components.Begin(), components.End()});
    const auto parentNodes = Node::GetParentNodes(nodes);

    switch (scopeHint_)
    {
    case AttributeScopeHint::Attribute:
    {
        oldState_.values_.clear();
        for (Component* component : components)
            oldState_.values_.push_back(component->GetAttribute(attribute->name_));
        break;
    }

    case AttributeScopeHint::Serializable:
    case AttributeScopeHint::Node:
    {
        oldState_.nodes_.clear();
        for (Node* node : parentNodes)
            oldState_.nodes_.emplace_back(node);
        break;
    }

    case AttributeScopeHint::Scene:
    {
        oldState_.scene_.FromScene(scene_);
        break;
    }

    default: break;
    }
}

void NodeComponentInspector::EndEditComponentAttribute(
    const WeakSerializableVector& objects, const AttributeInfo* attribute)
{
    if (objects.empty())
        return;

    const auto components = CastVectorTo<Component*>(objects);
    const auto nodes = Node::GetNodes({components.Begin(), components.End()});
    const auto parentNodes = Node::GetParentNodes(nodes);

    switch (scopeHint_)
    {
    case AttributeScopeHint::Attribute:
    {
        newState_.values_.clear();
        for (Component* component : components)
            newState_.values_.push_back(component->GetAttribute(attribute->name_));

        inspectedTab_->PushAction<ChangeComponentAttributesAction>(
            scene_, attribute->name_, components, oldState_.values_, newState_.values_);
        break;
    }

    case AttributeScopeHint::Serializable:
    case AttributeScopeHint::Node:
    {
        newState_.nodes_.clear();
        for (Node* node : parentNodes)
            newState_.nodes_.emplace_back(node);

        for (unsigned index = 0; index < parentNodes.size(); ++index)
        {
            inspectedTab_->PushAction<ChangeNodeSubtreeAction>(
                scene_, oldState_.nodes_[index], newState_.nodes_[index]);
        }
        break;
    }

    case AttributeScopeHint::Scene:
    {
        newState_.scene_.FromScene(scene_);

        inspectedTab_->PushAction<ChangeSceneAction>(scene_, oldState_.scene_, newState_.scene_);
        break;
    }

    default: break;
    }
}

void NodeComponentInspector::BeginAction(const WeakSerializableVector& objects)
{
    oldData_.clear();
    changedNodes_ = GetSortedTopmostNodes(objects);
    if (HasScene(changedNodes_))
    {
        // Disable undo/redo for scene actions for simplicity and performance
        // TODO: Implement it?
        changedNodes_.clear();
        return;
    }

    for (Node* node : changedNodes_)
    {
        oldData_.push_back(PackedNodeData{node});
    }
}

void NodeComponentInspector::EndAction(const WeakSerializableVector& objects)
{
    for (unsigned i = 0; i < changedNodes_.size(); ++i)
    {
        inspectedTab_->PushAction<ChangeNodeSubtreeAction>(scene_, oldData_[i], changedNodes_[i]);
    }
}

void NodeComponentInspector::AddComponentToNodes(StringHash componentType)
{
    if (!nodeWidget_)
        return;

    for (Node* node : nodeWidget_->GetNodes())
    {
        const CreateComponentActionFactory factory(node, componentType);
        if (auto component = node->CreateComponent(componentType))
            inspectedTab_->PushAction(factory.Cook(component));
    }
}

void NodeComponentInspector::RemoveComponent(Component* component)
{
    const RemoveComponentActionFactory factory(component);
    component->Remove();
    inspectedTab_->PushAction(factory.Cook());
}

void NodeComponentInspector::RenderContent()
{
    if (nodeWidget_)
    {
        nodeWidget_->RenderTitle();
        ui::Separator();
        nodeWidget_->RenderContent();
        ui::Separator();
        RenderAddComponent();
    }
    else if (componentWidget_)
    {
        componentWidget_->RenderTitle();
        ui::Separator();
        componentWidget_->RenderContent();
    }
    else
    {
        RenderComponentSummary();
    }
}

void NodeComponentInspector::RenderComponentSummary()
{
    if (ui::BeginTable("##Components", 2))
    {
        ui::TableSetupColumn("Component");
        ui::TableSetupColumn("Count");
        ui::TableHeadersRow();

        for (const auto& [typeName, count] : componentSummary_)
        {
            ui::TableNextRow();
            ui::TableNextColumn();
            ui::Text("%s", typeName.c_str());
            ui::TableNextColumn();
            ui::Text("%d", count);
        }
        ui::EndTable();
    }
}

void NodeComponentInspector::RenderAddComponent()
{
    if (ui::Button(ICON_FA_SQUARE_PLUS " Add Component"))
        ui::OpenPopup("##AddComponent");
    if (ui::BeginPopup("##AddComponent"))
    {
        if (const auto componentType = RenderCreateComponentMenu(context_))
        {
            AddComponentToNodes(*componentType);
            ui::CloseCurrentPopup();
        }
        ui::EndPopup();
    }
}

void NodeComponentInspector::RenderContextMenuItems()
{
}

void NodeComponentInspector::RenderMenu()
{
    if (inspectedTab_)
        inspectedTab_->RenderMenu();
}

void NodeComponentInspector::ApplyHotkeys(HotkeyManager* hotkeyManager)
{
    // TODO: Fix me. It's currently too annoying to deal with text edit hotkeys.
    // if (inspectedTab_)
    //     inspectedTab_->ApplyHotkeys(hotkeyManager);
}

}
