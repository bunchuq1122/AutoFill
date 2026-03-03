#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <queue>
#include <set>
#include <vector>

using namespace geode::prelude;

class $modify(filler, LevelEditorLayer) {
    struct Fields {
        int m_blockID = 1;
    };

    // i used ai as assistant to make this algorithm.
    void runFillSolidSmart(CCObject* sender) {
        auto blockID = this->m_fields->m_blockID;
        auto selected = this->m_editorUI->getSelectedObjects();
        if (!selected || selected->count() == 0) {
            FLAlertLayer::create("Error", "Select objects first.", "OK")->show();
            return;
        }
            

        constexpr float spacing = 30.f; // editor grid size
        constexpr int MAX_OBJECTS = 2000; // for preventing overflow

        std::set<std::pair<int,int>> wall; // pos of selected objs
        std::set<std::pair<int,int>> visited;

        int minGX = INT_MAX, minGY = INT_MAX; // minimum grid
        int maxGX = INT_MIN, maxGY = INT_MIN; // maximum grid

        float avgX = 0.f, avgY = 0.f;

        for (int i = 0; i < selected->count(); i++) {
            auto obj = static_cast<GameObject*>(selected->objectAtIndex(i));
            CCPoint pos = obj->getPosition();

            avgX += pos.x;
            avgY += pos.y;

            // correct the position
            int gx = static_cast<int>(floorf((pos.x + spacing/2) / spacing));
            int gy = static_cast<int>(floorf((pos.y + spacing/2) / spacing));

            wall.insert({gx, gy});

            minGX = std::min(minGX, gx);
            minGY = std::min(minGY, gy);
            maxGX = std::max(maxGX, gx);
            maxGY = std::max(maxGY, gy);
        }

        avgX /= selected->count(); // expand the outline
        avgY /= selected->count();

        minGX -= 1; minGY -= 1;
        maxGX += 1; maxGY += 1;

        std::queue<std::pair<int,int>> q;
        q.push({minGX, minGY});
        visited.insert({minGX, minGY});

        while (!q.empty()) {
            auto [gx, gy] = q.front();
            q.pop();

            std::vector<std::pair<int,int>> neighbors = {
                {gx+1, gy}, {gx-1, gy},
                {gx, gy+1}, {gx, gy-1}
            };

            for (auto [nx, ny] : neighbors) {

                if (nx < minGX || nx > maxGX ||
                    ny < minGY || ny > maxGY)
                    continue;

                // ignore the wall
                if (wall.count({nx, ny}))
                    continue;

                // ignore the pre-visited cells
                if (visited.count({nx, ny}))
                    continue;

                visited.insert({nx, ny});
                q.push({nx, ny});
            }
        }

        std::vector<std::pair<int,int>> fillCells;

        for (int gx = minGX + 1; gx < maxGX; gx++) {
            for (int gy = minGY + 1; gy < maxGY; gy++) {

                if (wall.count({gx, gy}))
                    continue;

                if (!visited.count({gx, gy})) {
                    fillCells.push_back({gx, gy});
                }
            }
        }

        // SMART FILL
        // If the object is not closed -> smart fill

        if (fillCells.empty()) {

            std::vector<CCPoint> polygon; // for saving object's pos

            for (int i = 0; i < selected->count(); i++) {
                auto obj = static_cast<GameObject*>(selected->objectAtIndex(i));
                polygon.push_back(obj->getPosition());
            } // save all pos of objects

            // A function that determines whether a point is inside a polygon
            // similar to ray casting ig

            auto pointInPolygon = [&](CCPoint pt) {
                bool inside = false; // is the point inside the pol?

                for (size_t i = 0, j = polygon.size() - 1;
                    i < polygon.size();
                    j = i++) {

                    const CCPoint& pi = polygon[i];
                    const CCPoint& pj = polygon[j];

                    // Check if the horizontal line intersects the edge
                    if (((pi.y > pt.y) != (pj.y > pt.y)) &&
                        (pt.x < (pj.x - pi.x) *
                        (pt.y - pi.y) / (pj.y - pi.y) + pi.x))
                        inside = !inside; // invert the inside/outside
                }

                return inside;
            };

            for (int gx = minGX + 1; gx < maxGX; gx++) {
                for (int gy = minGY + 1; gy < maxGY; gy++) {

                    if (wall.count({gx, gy}))
                        continue;

                    CCPoint pos = {
                        gx * spacing - 15.f, // correction
                        gy * spacing - 15.f
                    };

                    if (pointInPolygon(pos)) {
                        fillCells.push_back({gx, gy});

                        if ((int)fillCells.size() > MAX_OBJECTS)
                            break;
                    }
                }
            }
        }

        if ((int)fillCells.size() > MAX_OBJECTS) {
            log::warn("Fill cancelled: too many objects");
            return;
        } // anti overflowing

        std::vector<GameObject*> created;

        for (auto [gx, gy] : fillCells) {

            CCPoint pos = {
                gx * spacing -15.f,
                gy * spacing -15.f
            };

            auto obj = this->createObject(blockID, pos, false);
            if (obj)
                created.push_back(obj);
            obj->m_editorLayer = 1;
        }

        this->m_editorUI->onDeselectAll(nullptr);
    }

	bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;
        auto spr = CCSprite::create("logo.png"_spr);
        spr->setScale(0.08f);

        auto btn = CCMenuItemSpriteExtra::create(
			spr,
			this,
			menu_selector(filler::runFillSolidSmart)
		);
        btn->setContentSize({40.f,40.f});
        auto mn = CCMenu::create();
        mn->setID(Mod::get()->getID()+"/auto-fill"_spr);
        mn->addChild(btn);
        mn->setZOrder(100);
        mn->setPosition({117.f,134.f});
        this->addChild(mn);
        
        this->m_fields->m_blockID = Mod::get()->getSettingValue<int>("fillblock");
		
        //this->schedule(schedule_selector(filler::loop),0.01f);
        return true;
	}

    void updateDebugDraw() {
        LevelEditorLayer::updateDebugDraw();
        auto editorUI = this->m_editorUI;
        auto menu = editorUI->getChildByID("editor-buttons-menu");
        auto anyBtn = menu->getChildByID("copy-paste-button");
        this->getChildByID(Mod::get()->getID()+"/auto-fill"_spr)->setVisible(anyBtn->isVisible());
    }
};