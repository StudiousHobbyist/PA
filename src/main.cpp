//main.cpp
#include <SFML/Graphics.hpp>
#include "logger.h"

// Ctrl+Shift+B -> Build

#define LOGGED(Type, name) Type name{#name}
#define LOGGED_VAR(Type, name) Logged<Type> name{#name}

inline void updateViewPos(sf::View& view, float dt){
    sf::Vector2f move(0,0);

    float speed = 200.f;

    // controls
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) move += {-1.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) move += {1.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) move += {0.f, -1.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) move += {0.f, 1.f};
    
    if (move != sf::Vector2f{0,0})
        view.move(move.normalized() * speed * dt);
}

inline void updateViewZoom(sf::View& view, const sf::Event::MouseWheelScrolled* scroll){
    float speed = 0.1f;

    if (scroll->wheel == sf::Mouse::Wheel::Vertical)
    {
        if (scroll->delta > 0)
            view.zoom(1.f - speed);
        else if (scroll->delta < 0)
            view.zoom(1.f + speed);
    }
}

int main()
{
    sf::RenderWindow window( sf::VideoMode({800, 600}), "SFML Test");
    window.setMinimumSize(sf::Vector2u(400u, 300u));

    sf::View camera(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));

    sf::Clock clock;

    window.setFramerateLimit(60u);

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) window.close();

            if (event->is<sf::Event::MouseWheelScrolled>())
            {
                const sf::Event::MouseWheelScrolled* scroll = event->getIf<sf::Event::MouseWheelScrolled>();

                updateViewZoom(camera, scroll);
            }
        }

        updateViewPos(camera, dt);

        window.setView(camera);

        window.clear();

        window.display();
    }

    return 0;
}