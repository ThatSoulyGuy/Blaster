
#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Independent/Math/Vector.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Core
{
	class Window final
	{

	public:

		~Window()
		{
			glfwDestroyWindow(handle);
			glfwTerminate();
		}

		Window(const Window&) = delete;
		Window(Window&&) = delete;
		Window& operator=(const Window&) = delete;
		Window& operator=(Window&&) = delete;

		void Initialize(const std::string& title, const Vector<int, 2>& dimensions)
		{
			if (!glfwInit())
				throw std::runtime_error("Failed to initialize GLFW!");

			glfwDefaultWindowHints();
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
			glfwWindowHint(GLFW_VISIBLE, false);

			handle = glfwCreateWindow(dimensions.x(), dimensions.y(), title.c_str(), nullptr, nullptr);

			if (!handle)
				throw std::runtime_error("Failed to create GLFW window!");

			glfwMakeContextCurrent(handle);

			glfwSetFramebufferSizeCallback(handle, [](GLFWwindow*, const int width, const int height) { glViewport(0, 0, width, height); });

			if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
				throw std::runtime_error("Failed to initialize GLAD!");

			glfwShowWindow(handle);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}

		[[nodiscard]]
		bool IsRunning() const
		{
			return !glfwWindowShouldClose(handle);
		}

		static void Clear()
		{
			glClearColor(0.0f, 0.45f, 0.75f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		void Present() const
		{
			glfwPollEvents();
			glfwSwapBuffers(handle);
		}

		[[nodiscard]]
		std::string GetTitle() const
		{
			return glfwGetWindowTitle(handle);
		}

		void SetTitle(const std::string& title) const
		{
			glfwSetWindowTitle(handle, title.c_str());
		}

		[[nodiscard]]
		Vector<int, 2> GetDimensions() const
		{
			int width, height;

			glfwGetWindowSize(handle, &width, &height);

			return { width, height };
		}

		void SetDimensions(const Vector<int, 2>& dimensions) const
		{
			glfwSetWindowSize(handle, dimensions.x(), dimensions.y());
		}

		[[nodiscard]]
		Vector<int, 2> GetPosition() const
		{
			int x, y;

			glfwGetWindowPos(handle, &x, &y);

			return { x, y };
		}

		[[nodiscard]]
		GLFWwindow* GetHandle() const
		{
			return handle;
		}

		void SetPosition(const Vector<int, 2>& position) const
		{
			glfwSetWindowPos(handle, position.x(), position.y());
		}

		static Window& GetInstance()
		{
			std::call_once(initializationFlag, [&]()
			{
				instance = std::unique_ptr<Window>(new Window());
			});

			return *instance;
		}

	private:

		Window() = default;

		GLFWwindow* handle;

		static std::once_flag initializationFlag;
		static std::unique_ptr<Window> instance;

	};

	std::once_flag Window::initializationFlag;
	std::unique_ptr<Window> Window::instance;
}
