#pragma once

#include <windows.h>
#include "../core/constant.h"
#include "../input/input_handler.h"
#include "../game/inventory.h"
#include "../game/crafting.h"
#include "../game/furnace.h"
#include "../render/renderer.h"
#include "../world/world.h"
#include "../world/camera.h"
#include "../world/project4d.h"

#include "game_state.h"

/**
 * @brief 更新背包或工作台界面（2×2 / 3×3 合成）
 *
 * 处理物品拖放、合成结果拿取、右键操作。
 * 两个界面逻辑完全相同，仅背景图和合成模式不同。
 *
 * @return true 表示消费了本帧（调用方应 continue）
 */
bool updateCraftingScreen(
    GameState &state,
    InputHandler &input,
    Inventory &inventory,
    CraftingManager &craftMgr,
    Renderer &renderer,
    IMAGE &bgImage,
    Inventory::CraftMode craftMode,
    const std::function<void(const wchar_t *)> &playSFX,
    const wchar_t *clickPath
);

/**
 * @brief 更新熔炉界面
 * @return true 表示消费了本帧
 */
bool updateFurnaceScreen(
    GameState &state,
    InputHandler &input,
    Inventory &inventory,
    FurnaceManager::State &fs,
    Renderer &renderer,
    IMAGE &imgSmoker,
    IMAGE &imgBurnProgress,
    IMAGE &imgLitProgress
);

/**
 * @brief 更新暂停界面
 * @return true 表示消费了本帧
 */
bool updatePauseScreen(
    GameState &state,
    InputHandler &input,
    Renderer &renderer,
    World &world,
    Camera4D &camera,
    Map3D &map3D,
    double &cam3U, double &cam3V, double &cam3Y,
    double &cam3Yaw, double &cam3Pitch,
    double &pendingSliceRotation,
    int &selectedSlot,
    bool &loginBgReady,
    IMAGE &imgButton,
    int btnW, int btnH,
    const std::function<void(const wchar_t *)> &playSFX,
    const wchar_t *clickPath
);
