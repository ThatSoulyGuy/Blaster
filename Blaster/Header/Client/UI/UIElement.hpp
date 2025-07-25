#pragma once

#include "Client/Render/Vertices/UIVertex.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/UI/UIObject.hpp"

using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;

namespace Blaster::Client::UI
{
	class UIElement : public UIObject
	{

	public:

		virtual ~UIElement() { }

		void Initialize() override 
		{
			UIObject::Initialize();

			if (!GetGameObject()->HasComponent<Mesh<UIVertex>>())
				throw std::runtime_error(std::string("UIElement '") + GetGameObject()->GetAbsolutePath() + "' does not have required component 'Mesh<UIVertex>'!");
		}

		std::shared_ptr<Mesh<UIVertex>> GetMesh() const
		{
			return GetGameObject()->GetComponent<Mesh<UIVertex>>().value();
		}

	private:

		DESCRIBE_AND_REGISTER(UIElement, (UIObject), (), (), ())

	};
}