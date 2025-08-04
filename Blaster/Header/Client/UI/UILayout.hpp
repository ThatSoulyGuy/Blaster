#pragma once

#include "Client/UI/UIObject.hpp"
#include "Independent/Math/Rect.hpp"

namespace Blaster::Client::UI
{
	class UILayout : public UIObject
	{

	public:

		virtual ~UILayout() { }

		virtual Vector<float, 2> GetMeasurement() const = 0;
		virtual void Arrange(const Rect<float>& parentRect) = 0;

	private:

		DESCRIBE_AND_REGISTER(UILayout, (UIObject), (), (), ())

	};
}