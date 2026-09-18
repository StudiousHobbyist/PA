#include "TileMap.hpp"

#include <nlohmann/json.hpp>
#include <tinyxml2.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace fs = std::filesystem;


// ============================================================
// TILED GID FLAGS
// ============================================================

static constexpr unsigned FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
static constexpr unsigned FLIPPED_VERTICALLY_FLAG   = 0x40000000;
static constexpr unsigned FLIPPED_DIAGONALLY_FLAG   = 0x20000000;

static constexpr unsigned GID_MASK =
    ~(FLIPPED_HORIZONTALLY_FLAG |
      FLIPPED_VERTICALLY_FLAG |
      FLIPPED_DIAGONALLY_FLAG);


// ============================================================
// INFINITE MAP CHUNK
// ============================================================

namespace
{

struct ChunkInfo
{
    int x = 0;
    int y = 0;

    unsigned width = 0;
    unsigned height = 0;

    std::vector<unsigned> data;
};


struct RawLayer
{
    std::string name;
    bool visible = true;

    std::vector<ChunkInfo> chunks;
};


// ============================================================
// TSX LOADING
// ============================================================

bool loadTSX(
    const std::string& tsxPath,
    unsigned& tileWidth,
    unsigned& tileHeight,
    unsigned& columns,
    unsigned& tileCount,
    std::string& imageSource)
{
    tinyxml2::XMLDocument document;

    const tinyxml2::XMLError result =
        document.LoadFile(tsxPath.c_str());

    if (result != tinyxml2::XML_SUCCESS)
    {
        std::cerr
            << "TileMap: could not load TSX "
            << tsxPath
            << ": "
            << document.ErrorStr()
            << '\n';

        return false;
    }

    const tinyxml2::XMLElement* tileset =
        document.FirstChildElement("tileset");

    if (!tileset)
    {
        std::cerr
            << "TileMap: TSX has no <tileset> element: "
            << tsxPath
            << '\n';

        return false;
    }


    // --------------------------------------------------------
    // Tileset dimensions
    // --------------------------------------------------------

    tileset->QueryUnsignedAttribute(
        "tilewidth",
        &tileWidth);

    tileset->QueryUnsignedAttribute(
        "tileheight",
        &tileHeight);

    tileset->QueryUnsignedAttribute(
        "columns",
        &columns);

    tileset->QueryUnsignedAttribute(
        "tilecount",
        &tileCount);


    // --------------------------------------------------------
    // Image
    // --------------------------------------------------------

    const tinyxml2::XMLElement* image =
        tileset->FirstChildElement("image");

    if (!image)
    {
        std::cerr
            << "TileMap: TSX has no <image>: "
            << tsxPath
            << '\n';

        return false;
    }

    const char* source =
        image->Attribute("source");

    if (!source)
    {
        std::cerr
            << "TileMap: TSX image has no source: "
            << tsxPath
            << '\n';

        return false;
    }

    imageSource = source;


    // --------------------------------------------------------
    // Some TSX files may omit columns/tilecount.
    //
    // Calculate them from image dimensions.
    // --------------------------------------------------------

    unsigned imageWidth = 0;
    unsigned imageHeight = 0;

    image->QueryUnsignedAttribute(
        "width",
        &imageWidth);

    image->QueryUnsignedAttribute(
        "height",
        &imageHeight);

    if (columns == 0 &&
        tileWidth > 0 &&
        imageWidth > 0)
    {
        columns = imageWidth / tileWidth;
    }

    if (tileCount == 0 &&
        columns > 0 &&
        tileHeight > 0 &&
        imageHeight > 0)
    {
        unsigned rows =
            imageHeight / tileHeight;

        tileCount =
            columns * rows;
    }


    return true;
}

} // namespace


// ============================================================
// LOAD
// ============================================================

bool TileMap::load(
    const std::string& mapJsonPath,
    const std::string& tilesetImagePath)
{
    std::ifstream file(mapJsonPath);

    if (!file.is_open())
    {
        std::cerr
            << "TileMap: could not open "
            << mapJsonPath
            << '\n';

        return false;
    }


    // --------------------------------------------------------
    // Parse TMJ / JSON
    // --------------------------------------------------------

    json map;

    try
    {
        file >> map;
    }
    catch (const json::parse_error& e)
    {
        std::cerr
            << "TileMap: JSON parse error in "
            << mapJsonPath
            << ": "
            << e.what()
            << '\n';

        return false;
    }


    // --------------------------------------------------------
    // Map properties
    // --------------------------------------------------------

    m_infinite =
        map.value("infinite", false);

    m_tileWidth =
        map.value("tilewidth", 0u);

    m_tileHeight =
        map.value("tileheight", 0u);


    m_widthTiles =
        map.value("width", 0u);

    m_heightTiles =
        map.value("height", 0u);


    // --------------------------------------------------------
    // Clear previous map
    // --------------------------------------------------------

    m_tilesets.clear();
    m_layers.clear();
    m_layerData.clear();


    // --------------------------------------------------------
    // Load ALL tilesets
    // --------------------------------------------------------

    if (!map.contains("tilesets") ||
        !map["tilesets"].is_array() ||
        map["tilesets"].empty())
    {
        std::cerr
            << "TileMap: no tilesets defined in "
            << mapJsonPath
            << '\n';

        return false;
    }


    const fs::path mapPath =
        fs::absolute(mapJsonPath);

    const fs::path mapDirectory =
        mapPath.parent_path();


    for (const auto& tilesetReference : map["tilesets"])
    {
        const unsigned firstGid =
            tilesetReference.value(
                "firstgid",
                1u);


        // ====================================================
        // EXTERNAL TSX
        // ====================================================

        if (tilesetReference.contains("source"))
        {
            const std::string source =
                tilesetReference["source"]
                    .get<std::string>();


            fs::path tsxPath =
                mapDirectory / source;

            tsxPath =
                fs::weakly_canonical(tsxPath);


            unsigned tileWidth = 0;
            unsigned tileHeight = 0;
            unsigned columns = 0;
            unsigned tileCount = 0;

            std::string imageSource;


            if (!loadTSX(
                    tsxPath.string(),
                    tileWidth,
                    tileHeight,
                    columns,
                    tileCount,
                    imageSource))
            {
                return false;
            }


            fs::path imagePath =
                tsxPath.parent_path() / imageSource;

            imagePath =
                fs::weakly_canonical(imagePath);


            Tileset tileset;

            tileset.firstGid =
                firstGid;

            tileset.tileWidth =
                tileWidth;

            tileset.tileHeight =
                tileHeight;

            tileset.columns =
                columns;

            tileset.tileCount =
                tileCount;


            if (!tileset.texture.loadFromFile(
                    imagePath.string()))
            {
                std::cerr
                    << "TileMap: could not load tileset image "
                    << imagePath.string()
                    << '\n';

                return false;
            }


            m_tilesets.push_back(
                std::move(tileset));
        }


        // ====================================================
        // INLINE TILESET
        // ====================================================

        else
        {
            const auto& tilesetJson =
                tilesetReference;


            Tileset tileset;

            tileset.firstGid =
                firstGid;

            tileset.tileWidth =
                tilesetJson.value(
                    "tilewidth",
                    m_tileWidth);

            tileset.tileHeight =
                tilesetJson.value(
                    "tileheight",
                    m_tileHeight);

            tileset.columns =
                tilesetJson.value(
                    "columns",
                    0u);

            tileset.tileCount =
                tilesetJson.value(
                    "tilecount",
                    0u);


            // ------------------------------------------------
            // Determine columns if absent.
            // ------------------------------------------------

            unsigned imageWidth =
                tilesetJson.value(
                    "imagewidth",
                    0u);

            unsigned imageHeight =
                tilesetJson.value(
                    "imageheight",
                    0u);


            if (tileset.columns == 0 &&
                tileset.tileWidth > 0 &&
                imageWidth > 0)
            {
                tileset.columns =
                    imageWidth /
                    tileset.tileWidth;
            }


            if (tileset.tileCount == 0 &&
                tileset.columns > 0 &&
                tileset.tileHeight > 0 &&
                imageHeight > 0)
            {
                unsigned rows =
                    imageHeight /
                    tileset.tileHeight;

                tileset.tileCount =
                    tileset.columns *
                    rows;
            }


            // ------------------------------------------------
            // Image path
            //
            // Inline tilesets normally contain:
            //
            // "image": "tiles.png"
            //
            // If tilesetImagePath is supplied, preserve
            // compatibility with your old loader.
            // ------------------------------------------------

            std::string imagePathString;


            if (tilesetJson.contains("image"))
            {
                imagePathString =
                    tilesetJson["image"]
                        .get<std::string>();

                fs::path imagePath =
                    mapDirectory /
                    imagePathString;

                imagePath =
                    fs::weakly_canonical(imagePath);

                imagePathString =
                    imagePath.string();
            }
            else
            {
                imagePathString =
                    tilesetImagePath;
            }


            if (imagePathString.empty())
            {
                std::cerr
                    << "TileMap: inline tileset has no image path\n";

                return false;
            }


            if (!tileset.texture.loadFromFile(
                    imagePathString))
            {
                std::cerr
                    << "TileMap: could not load tileset image "
                    << imagePathString
                    << '\n';

                return false;
            }


            m_tilesets.push_back(
                std::move(tileset));
        }
    }


    // --------------------------------------------------------
    // Sort tilesets by first GID.
    //
    // Tiled normally already stores them in this order, but
    // sorting makes GID lookup deterministic.
    // --------------------------------------------------------

    std::sort(
        m_tilesets.begin(),
        m_tilesets.end(),
        [](const Tileset& a, const Tileset& b)
        {
            return a.firstGid < b.firstGid;
        });


    if (m_tilesets.empty())
    {
        std::cerr
            << "TileMap: no valid tilesets loaded\n";

        return false;
    }


    // ========================================================
    // INFINITE MAP
    // ========================================================

    m_originX = 0;
    m_originY = 0;


    if (m_infinite)
    {
        std::vector<RawLayer> rawLayers;


        long long minX =
            std::numeric_limits<long long>::max();

        long long minY =
            std::numeric_limits<long long>::max();

        long long maxX =
            std::numeric_limits<long long>::min();

        long long maxY =
            std::numeric_limits<long long>::min();


        // ----------------------------------------------------
        // Read chunks
        // ----------------------------------------------------

        for (const auto& layerJson : map["layers"])
        {
            if (layerJson.value(
                    "type",
                    "") != "tilelayer")
            {
                continue;
            }

            if (!layerJson.contains("chunks"))
                continue;


            RawLayer raw;

            raw.name =
                layerJson.value(
                    "name",
                    "");

            raw.visible =
                layerJson.value(
                    "visible",
                    true);


            for (const auto& chunkJson :
                 layerJson["chunks"])
            {
                ChunkInfo chunk;

                chunk.x =
                    chunkJson.value(
                        "x",
                        0);

                chunk.y =
                    chunkJson.value(
                        "y",
                        0);

                chunk.width =
                    chunkJson.value(
                        "width",
                        16u);

                chunk.height =
                    chunkJson.value(
                        "height",
                        16u);


                if (!chunkJson.contains("data"))
                    continue;


                chunk.data =
                    chunkJson["data"]
                        .get<std::vector<unsigned>>();


                minX =
                    std::min<long long>(
                        minX,
                        chunk.x);

                minY =
                    std::min<long long>(
                        minY,
                        chunk.y);

                maxX =
                    std::max<long long>(
                        maxX,
                        static_cast<long long>(
                            chunk.x) +
                        chunk.width);

                maxY =
                    std::max<long long>(
                        maxY,
                        static_cast<long long>(
                            chunk.y) +
                        chunk.height);


                raw.chunks.push_back(
                    std::move(chunk));
            }


            rawLayers.push_back(
                std::move(raw));
        }


        if (rawLayers.empty() ||
            minX > maxX ||
            minY > maxY)
        {
            std::cerr
                << "TileMap: infinite map has no chunk data in "
                << mapJsonPath
                << '\n';

            return false;
        }


        // ----------------------------------------------------
        // Calculate bounding rectangle
        // ----------------------------------------------------

        m_originX =
            static_cast<int>(minX);

        m_originY =
            static_cast<int>(minY);


        m_widthTiles =
            static_cast<unsigned>(
                maxX - minX);

        m_heightTiles =
            static_cast<unsigned>(
                maxY - minY);


        // ----------------------------------------------------
        // Convert chunks into dense grids
        // ----------------------------------------------------

        for (auto& raw : rawLayers)
        {
            std::vector<unsigned> gids(
                static_cast<std::size_t>(
                    m_widthTiles) *
                m_heightTiles,
                0u);


            for (const auto& chunk :
                 raw.chunks)
            {
                for (unsigned cy = 0;
                     cy < chunk.height;
                     ++cy)
                {
                    for (unsigned cx = 0;
                         cx < chunk.width;
                         ++cx)
                    {
                        const std::size_t srcIndex =
                            static_cast<std::size_t>(cy) *
                            chunk.width +
                            cx;


                        if (srcIndex >=
                            chunk.data.size())
                        {
                            continue;
                        }


                        const unsigned gid =
                            chunk.data[srcIndex];


                        if (gid == 0)
                            continue;


                        const long long absoluteX =
                            static_cast<long long>(
                                chunk.x) +
                            cx;

                        const long long absoluteY =
                            static_cast<long long>(
                                chunk.y) +
                            cy;


                        const std::size_t destX =
                            static_cast<std::size_t>(
                                absoluteX -
                                minX);

                        const std::size_t destY =
                            static_cast<std::size_t>(
                                absoluteY -
                                minY);


                        gids[
                            destY *
                            m_widthTiles +
                            destX
                        ] = gid;
                    }
                }
            }


            TileLayer layer;

            layer.name =
                raw.name;

            layer.visible =
                raw.visible;


            buildLayerVertices(
                layer,
                gids);


            m_layers.push_back(
                std::move(layer));

            m_layerData.push_back(
                std::move(gids));
        }
    }


    // ========================================================
    // FINITE MAP
    // ========================================================

    else
    {
        m_originX = 0;
        m_originY = 0;


        for (const auto& layerJson :
             map["layers"])
        {
            if (layerJson.value(
                    "type",
                    "") != "tilelayer")
            {
                continue;
            }

            if (!layerJson.contains("data"))
                continue;


            std::vector<unsigned> gids =
                layerJson["data"]
                    .get<std::vector<unsigned>>();


            TileLayer layer;

            layer.name =
                layerJson.value(
                    "name",
                    "");

            layer.visible =
                layerJson.value(
                    "visible",
                    true);


            buildLayerVertices(
                layer,
                gids);


            m_layers.push_back(
                std::move(layer));

            m_layerData.push_back(
                std::move(gids));
        }
    }


    return true;
}


// ============================================================
// FIND TILESET
// ============================================================

const TileMap::Tileset*
TileMap::findTileset(unsigned gid) const
{
    gid &= GID_MASK;


    if (gid == 0)
        return nullptr;


    const Tileset* result =
        nullptr;


    for (const auto& tileset :
         m_tilesets)
    {
        if (gid >= tileset.firstGid)
        {
            result = &tileset;
        }
        else
        {
            break;
        }
    }


    return result;
}


// ============================================================
// LOCAL TILE ID
// ============================================================

unsigned TileMap::getTileLocalId(
    unsigned gid,
    const Tileset& tileset) const
{
    gid &= GID_MASK;

    return gid - tileset.firstGid;
}


// ============================================================
// BUILD VERTICES
// ============================================================

void TileMap::buildLayerVertices(
    TileLayer& layer,
    const std::vector<unsigned>& gids)
{
    // --------------------------------------------------------
    // One batch for each tileset.
    // --------------------------------------------------------

    layer.batches.clear();

    layer.batches.reserve(
        m_tilesets.size());


    for (std::size_t i = 0;
         i < m_tilesets.size();
         ++i)
    {
        TileBatch batch;

        batch.tilesetIndex = i;

        batch.vertices =
            sf::VertexArray(
                sf::PrimitiveType::Triangles);


        layer.batches.push_back(
            std::move(batch));
    }


    // --------------------------------------------------------
    // Build geometry
    // --------------------------------------------------------

    for (unsigned y = 0;
         y < m_heightTiles;
         ++y)
    {
        for (unsigned x = 0;
             x < m_widthTiles;
             ++x)
        {
            const std::size_t index =
                static_cast<std::size_t>(y) *
                m_widthTiles +
                x;


            if (index >= gids.size())
                continue;


            const unsigned rawGid =
                gids[index];


            const unsigned gid =
                rawGid & GID_MASK;


            if (gid == 0)
                continue;


            // ------------------------------------------------
            // Find which tileset owns this GID.
            // ------------------------------------------------

            const Tileset* tileset =
                findTileset(gid);


            if (!tileset)
                continue;


            const unsigned tilesetIndex =
                static_cast<unsigned>(
                    tileset -
                    m_tilesets.data());


            if (tilesetIndex >=
                layer.batches.size())
            {
                continue;
            }


            // ------------------------------------------------
            // Local tile ID
            // ------------------------------------------------

            const unsigned localId =
                getTileLocalId(
                    gid,
                    *tileset);


            if (tileset->columns == 0)
                continue;


            if (tileset->tileCount > 0 &&
                localId >= tileset->tileCount)
            {
                std::cerr
                    << "TileMap: GID "
                    << gid
                    << " points outside tileset\n";

                continue;
            }


            const unsigned tileU =
                localId %
                tileset->columns;

            const unsigned tileV =
                localId /
                tileset->columns;


            // ------------------------------------------------
            // World position
            //
            // Infinite maps have m_originX/m_originY, so
            // convert the dense grid position back into the
            // original Tiled coordinate.
            // ------------------------------------------------

            const float px =
                static_cast<float>(
                    (static_cast<int>(x) +
                     m_originX) *
                    static_cast<int>(
                        m_tileWidth));

            const float py =
                static_cast<float>(
                    (static_cast<int>(y) +
                     m_originY) *
                    static_cast<int>(
                        m_tileHeight));


            sf::Vector2f p0(
                px,
                py);

            sf::Vector2f p1(
                px +
                static_cast<float>(
                    m_tileWidth),
                py);

            sf::Vector2f p2(
                px +
                static_cast<float>(
                    m_tileWidth),
                py +
                static_cast<float>(
                    m_tileHeight));

            sf::Vector2f p3(
                px,
                py +
                static_cast<float>(
                    m_tileHeight));


            // ------------------------------------------------
            // Texture coordinates
            //
            // Use the tileset's own tile dimensions.
            // ------------------------------------------------

            float left =
                static_cast<float>(
                    tileU *
                    tileset->tileWidth);

            float top =
                static_cast<float>(
                    tileV *
                    tileset->tileHeight);

            float right =
                left +
                static_cast<float>(
                    tileset->tileWidth);

            float bottom =
                top +
                static_cast<float>(
                    tileset->tileHeight);


            // ------------------------------------------------
            // Tiled flip flags
            // ------------------------------------------------

            const bool flipH =
                (rawGid &
                 FLIPPED_HORIZONTALLY_FLAG) != 0;

            const bool flipV =
                (rawGid &
                 FLIPPED_VERTICALLY_FLAG) != 0;


            if (flipH)
                std::swap(left, right);

            if (flipV)
                std::swap(top, bottom);


            sf::Vector2f t0(
                left,
                top);

            sf::Vector2f t1(
                right,
                top);

            sf::Vector2f t2(
                right,
                bottom);

            sf::Vector2f t3(
                left,
                bottom);


            // ------------------------------------------------
            // Append two triangles
            // ------------------------------------------------

            auto& vertices =
                layer.batches[
                    tilesetIndex
                ].vertices;


            vertices.append(
                {p0, sf::Color::White, t0});

            vertices.append(
                {p1, sf::Color::White, t1});

            vertices.append(
                {p2, sf::Color::White, t2});


            vertices.append(
                {p0, sf::Color::White, t0});

            vertices.append(
                {p2, sf::Color::White, t2});

            vertices.append(
                {p3, sf::Color::White, t3});
        }
    }
}


// ============================================================
// GET TILE GID
// ============================================================

unsigned TileMap::getTileGid(
    std::size_t layerIndex,
    unsigned x,
    unsigned y) const
{
    if (layerIndex >=
        m_layerData.size())
    {
        return 0;
    }


    if (x >= m_widthTiles ||
        y >= m_heightTiles)
    {
        return 0;
    }


    const std::size_t index =
        static_cast<std::size_t>(y) *
        m_widthTiles +
        x;


    const auto& gids =
        m_layerData[layerIndex];


    if (index >= gids.size())
        return 0;


    return gids[index] &
           GID_MASK;
}


// ============================================================
// DRAW
// ============================================================

void TileMap::draw(
    sf::RenderTarget& target,
    sf::RenderStates states) const
{
    states.transform *=
        getTransform();


    for (const auto& layer :
         m_layers)
    {
        if (!layer.visible)
            continue;


        for (const auto& batch :
             layer.batches)
        {
            if (batch.tilesetIndex >=
                m_tilesets.size())
            {
                continue;
            }


            const auto& tileset =
                m_tilesets[
                    batch.tilesetIndex
                ];


            states.texture =
                &tileset.texture;


            target.draw(
                batch.vertices,
                states);
        }
    }
}