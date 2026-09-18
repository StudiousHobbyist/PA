// main.cpp
#include <SFML/Graphics.hpp>
#include "TileMap.hpp"

// Ctrl+Shift+B -> Build

inline void updateViewPos(sf::View& view, float dt) {
    sf::Vector2f move(0.f, 0.f);
    const float speed = 400.f; // pixels/sec

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) move += {-1.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) move += {1.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) move += {0.f, -1.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) move += {0.f, 1.f};

    if (move != sf::Vector2f{0.f, 0.f})
        view.move(move.normalized() * speed * dt);
}

inline void updateViewZoom(sf::View& view, const sf::Event::MouseWheelScrolled* scroll) {
    const float speed = 0.1f;

    if (scroll->wheel == sf::Mouse::Wheel::Vertical) {
        if (scroll->delta > 0)
            view.zoom(1.f - speed);
        else if (scroll->delta < 0)
            view.zoom(1.f + speed);
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode({1280u, 720u}), "Tiled + SFML");
    window.setMinimumSize(sf::Vector2u(400u, 300u));
    window.setFramerateLimit(60u);

    TileMap map;
    // Point these at your exported map and its tileset image, e.g.:
    //   assets/map.json   (Tiled: File -> Export As -> JSON)
    //   assets/tileset.png
    if (!map.load("assets/tilemaps/untitled.tmj", "assets/tilesets/PATileSet.png")) {
        return 1; // error already printed to stderr
    }

    sf::View camera(sf::FloatRect({0.f, 0.f}, {1280.f, 720.f}));

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (event->is<sf::Event::MouseWheelScrolled>()) {
                const sf::Event::MouseWheelScrolled* scroll =
                    event->getIf<sf::Event::MouseWheelScrolled>();
                updateViewZoom(camera, scroll);
            }
        }

        updateViewPos(camera, dt);
        window.setView(camera);

        window.clear(sf::Color(30, 30, 40));
        window.draw(map);
        window.display();
    }

    return 0;
}