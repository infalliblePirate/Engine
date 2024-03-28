#include "dmpch.h"
#include "Application.h"

#include "Events/ApplicationEvent.h"
#include "spdlog/sinks/stdout_sinks.h"

#include <glad/glad.h>

namespace Deimos {
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    Application* Application::s_instance = nullptr;

    Application::Application() {
        DM_CORE_ASSERT(!s_instance, "Application already exists!");
        s_instance = this;

        m_window = std::unique_ptr<Window>(Window::create());
        m_window->setEventCallback(BIND_EVENT_FN(onEvent)); // set onEvent as the callback fun
        m_ImGuiLayer = new ImGuiLayer();
        pushOverlay(m_ImGuiLayer);
    }

    Application::~Application() {

    }

    void Application::pushLayer(Layer *layer) {
        m_layerStack.pushLayer(layer);
        layer->onAttach();
    }

    void Application::pushOverlay(Layer *overlay) {
        m_layerStack.pushOverlay(overlay);
        overlay->onAttach();
    }

    // whenever an even is occurred, it calls this function
    void Application::onEvent(Event &e) {
        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>(BIND_EVENT_FN(onWindowClose));

        // if handled an event, terminate (starts from overlays or last layers)
        for (auto it = m_layerStack.end(); it != m_layerStack.begin();) {
            (*--it)->onEvent(e);
            if (e.handled)
                break;
        }
    }


    void Application::run() {
        while (m_running) {
            glClearColor(1, 1, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            for (Layer* layer : m_layerStack)
                layer->onUpdate();

            m_ImGuiLayer->begin();
            for (Layer* layer : m_layerStack) {
                layer->onImGuiRender();
            }
            m_ImGuiLayer->end();

            m_window->onUpdate();
        }
    }

    bool Application::onWindowClose(WindowCloseEvent &e) {
        m_running = false;
        return true; // everything went great, the fun was handled
    }
}
