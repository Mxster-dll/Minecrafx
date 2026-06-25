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