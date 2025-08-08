#pragma once

#include "Client/Render/Vertices/UIVertex.hpp"
#include "Independent/ECS/GameObjectManager.hpp"

using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;

namespace Blaster::Client::UI
{
    class UILayout;
    class UIElement;

    template <typename>
    class LayoutBuilder;

    template <typename>
    class ElementBuilder;

    template<class MemPtr, class ElementT, class... Args>
    concept InvocableMember = std::is_member_pointer_v<MemPtr> && std::is_invocable_v<MemPtr, ElementT&, Args...>;

    template<class MemPtr, class ElementT, class... Args>
    concept InvocableNonVoidMember = std::is_member_pointer_v<MemPtr> && requires (ElementT const& obj, MemPtr mp, Args&&... as)
    {
        { std::invoke(mp, obj, std::forward<Args>(as)...) } -> std::same_as<std::invoke_result_t<MemPtr, ElementT const&, Args...>>;
    } && !std::is_void_v<std::invoke_result_t<MemPtr, ElementT const&, Args...>>;

    class UIBuilder
    {

    public:

        static UIBuilder NewMenu(const std::string& name = "ui_root", const std::string& parent = ".")
        {
            bool alreadyExists = false;
            std::shared_ptr<GameObject> root;

            if (GameObjectManager::GetInstance().Has(name))
            {
                alreadyExists = true;
                root = GameObjectManager::GetInstance().Get(name).value();
            }
            else
                root = GameObjectManager::GetInstance().Register(GameObject::Create(name, true, ClientNetwork::GetInstance().GetNetworkId(), true), parent, false);

            auto transform = root->GetTransform2d();

            transform->SetStretch(Transform2d::Stretch::TOP | Transform2d::Stretch::BOTTOM | Transform2d::Stretch::RIGHT | Transform2d::Stretch::LEFT);

            if (alreadyExists)
                return UIBuilder(root->GetAbsolutePath(), nullptr, nullptr);
            else
                return UIBuilder(root->GetAbsolutePath(), root, nullptr);
        }

        template <class LayoutT, class... Args> requires std::is_base_of_v<UILayout, LayoutT>
        auto AddLayout(const std::string& name = "", Args&&... args)
        {
            auto child = CreateChildGameObject(name);

            if (!child)
                return LayoutBuilder<LayoutT>(rootName, child, this);

            child->AddComponent(LayoutT::Create(std::forward<Args>(args)...));

            return LayoutBuilder<LayoutT>(rootName, child, this);
        }

        template <class ElementT, class... Args> requires std::is_base_of_v<UIElement, ElementT>
        auto AddElement(const std::string& name = "", Args&&... args)
        {
            auto child = CreateChildGameObject(name);

            if (!child)
                return ElementBuilder<ElementT>(rootName, child, this);

            auto element = ElementT::Create(std::forward<Args>(args)...);

            if (element->GetShader().has_value())
                child->AddComponent(element->GetShader().value());

            child->AddComponent(Mesh<UIVertex>::Create({}, {}));
            child->AddComponent(element);

            return ElementBuilder<ElementT>(rootName, child, this);
        }

        UIBuilder& MoveDown()
        {
            if (!parent)
                throw std::logic_error("Already at root");

            return *parent;
        }

        std::shared_ptr<GameObject> Finish()
        {
            if (!node)
                return GameObjectManager::GetInstance().Get(rootName).value();

            RunLayout(node);

            return node;
        }

    private:

        template <class E>
        friend class LayoutBuilder;

        template <class E>
        friend class ElementBuilder;

        UIBuilder(std::string rootName, std::shared_ptr<GameObject> n, UIBuilder* p) : rootName(rootName), node(std::move(n)), parent(p) { }

        std::shared_ptr<GameObject> CreateChildGameObject(const std::string& name)
        {
            static size_t counter{};

            if (!node)
                return nullptr;

            return GameObjectManager::GetInstance().Register(GameObject::Create(name == "" ? "ui_" + std::to_string(counter++) : name, true, ClientNetwork::GetInstance().GetNetworkId(), true), node->GetAbsolutePath(), false);
        }

        static void RunLayout(const std::shared_ptr<GameObject>& gameObject)
        {
            if (gameObject->HasComponent<UILayout>())
            {
                Vector<float, 2> size = gameObject->GetComponent<UILayout>().value()->GetMeasurement();

                auto transform = gameObject->GetTransform2d();

                transform->SetDimensions(size);

                Rect<float> rect{ {0,0}, size };
                gameObject->GetComponent<UILayout>().value()->Arrange(rect);
            }

            for (auto& child : gameObject->GetChildMap() | std::views::values)
                RunLayout(child);
        }

        std::string rootName;
        std::shared_ptr<GameObject> node;
        UIBuilder* parent;
    };

    template <class LayoutT>
    class LayoutBuilder final : public UIBuilder
    {

    public:

        using UIBuilder::UIBuilder;

        template <auto Member, class... Args>
        using MemberResultT = std::invoke_result_t<decltype(Member), LayoutT&, Args...>;

        template <auto Member, class... Args> requires InvocableMember<decltype(Member), LayoutT, Args...>
        LayoutBuilder& Call(Args&&... args)
        {
            (GetLayout().*Member)(std::forward<Args>(args)...);

            return *this;
        }

        template <auto Member, class Callback, class... Args> requires InvocableNonVoidMember<decltype(Member), LayoutT, Args...>&& std::invocable<Callback, std::invoke_result_t<decltype(Member), LayoutT&, Args...>>
        LayoutBuilder& CallAndThen(Callback&& callback, Args&&... args)
        {
            auto&& result = std::invoke(Member, GetLayout(), std::forward<Args>(args)...);

            std::invoke(std::forward<Callback>(callback), std::forward<decltype(result)>(result));

            return *this;
        }

    private:

        LayoutT& GetLayout()
        {
            return *node->GetComponent<LayoutT>().value();
        }

    };

    template <class ElementT>
    class ElementBuilder : public UIBuilder
    {

    public:

        using UIBuilder::UIBuilder;
        
        template <auto Member, class... Args>
        using MemberResultT = std::invoke_result_t<decltype(Member), ElementT&, Args...>;

        template <auto Member, class... Args> requires InvocableMember<decltype(Member), ElementT, Args...>
        ElementBuilder& Call(Args&&... args) 
        {
            (GetElement().*Member)(std::forward<Args>(args)...);

            return *this;
        }

        template <auto Member, class Callback, class... Args> requires InvocableMember<decltype(Member), ElementT, Args...> && std::is_invocable_v<Callback, MemberResultT<Member, Args...>>
        ElementBuilder& CallAndThen(Callback&& callback, Args&&... args)
        {
            auto&& result = std::invoke(Member, GetElement(), std::forward<Args>(args)...);

            std::invoke(std::forward<Callback>(callback), std::forward<decltype(result)>(result));

            return *this;
        }

        template <class Fn> requires std::invocable<Fn, ElementT&>
        ElementBuilder& Modify(Fn&& fn)
        {
            fn(GetElement());

            return *this;
        }

    private:

        ElementT& GetElement()
        {
            return *node->GetComponent<ElementT>().value();
        }
    };
}