#pragma once

#include "Independent/ECS/GameObject.hpp"

namespace Blaster::Client::UI
{
	class UIObject : public Component
	{
		
	public:

		virtual ~UIObject() { }

		void Initialize() override
		{
			if (!GetGameObject()->HasComponent<Transform2d>())
				throw std::runtime_error(std::string("UIObject '") + GetGameObject()->GetAbsolutePath() + "' doesn't have required component 'Transform2d'!");
		}

	private:

		DESCRIBE_AND_REGISTER(UIObject, (Component), (), (), ())

	};
}