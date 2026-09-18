#pragma once

#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

class TileMap : public sf::Drawable, public sf::Transformable
{
private:
    struct Tileset
    {
        unsigned firstGid = 1;

        unsigned tileWidth = 0;
        unsigned tileHeight = 0;

        unsigned columns = 0;
        unsigned tileCount = 0;

        sf::Texture texture;
    };

    struct TileBatch
    {
        std::size_t tilesetIndex = 0;
        sf::VertexArray vertices{sf::PrimitiveType::Triangles};
    };

    struct TileLayer
    {
        std::string name;
        bool visible = true;

        std::vector<TileBatch> batches;
    };

    bool loadTilesets(
        const std::string& mapJsonPath,
        const nlohmann::json& tilesetsJson);

    bool loadExternalTileset(
        const std::string& tsxPath,
        unsigned firstGid);

    bool loadInlineTileset(
        const nlohmann::json& tilesetJson);

    void buildLayerVertices(
        TileLayer& layer,
        const std::vector<unsigned>& gids);

    const Tileset* findTileset(unsigned gid) const;

    unsigned getTileLocalId(
        unsigned gid,
        const Tileset& tileset) const;

    std::vector<Tileset> m_tilesets;

    std::vector<TileLayer> m_layers;

    std::vector<std::vector<unsigned>> m_layerData;

    unsigned m_tileWidth = 0;
    unsigned m_tileHeight = 0;

    unsigned m_widthTiles = 0;
    unsigned m_heightTiles = 0;

    int m_originX = 0;
    int m_originY = 0;

    bool m_infinite = false;

public:
    bool load(
        const std::string& mapJsonPath,
        const std::string& tilesetImagePath = "");

    unsigned getTileGid(
        std::size_t layerIndex,
        unsigned x,
        unsigned y) const;

    void draw(
        sf::RenderTarget& target,
        sf::RenderStates states) const override;
};