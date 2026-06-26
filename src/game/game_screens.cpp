#include "game_screens.h"
#include "../core/constant.h"
#include <functional>
#include <ctime>

// TODO: 状态模式重构，此文件内全是 UI 状态机，太丑

bool updateCraftingScreen(
    GameState &state,
    InputHandler &input,
    Inventory &inventory,
    CraftingManager &craftMgr,
    Renderer &renderer,
    IMAGE &bgImage,
    Inventory::CraftMode craftMode,
    const std::function<void(const wchar_t *)> &playSFX,
    const wchar_t *clickPath)
{
    if (input.isPressed(Key::Esc) || input.isPressed(Key::E))
    {
        playSFX(clickPath);
        if (inventory.isDragging()) inventory.cancelDrag();
        // 关闭前将合成区物品退回背包
        inventory.returnCraftItems();
        if (craftMode == Inventory::CM_CraftingTable3x3)
            inventory.setCraftMode(Inventory::CM_Inventory2x2);
        state = GameState::Gameplay;
        input.showMouseCursor(false);
        input.getMouseDelta();
        return true;
    }

    int bgW = bgImage.getwidth();
    int bgH = bgImage.getheight();
    int bgX = (SCREEN_WIDTH - bgW) / 2;
    int bgY = (SCREEN_HEIGHT - bgH) / 2;
    int nativeW = bgW / 3;
    int nativeH = bgH / 3;

    POINT mp = input.getMouseScreenPos();
    bool mouseDown = input.getMouseClick(0);
    bool mouseUp = input.getMouseRelease(0);
    bool rightClick = input.getMouseClick(1);
    bool rightHeld = input.isMouseButtonDown(1);

    int hoveredSlot = inventory.hitTest(mp.x, mp.y,
        bgX, bgY, bgW, bgH, nativeW, nativeH);

    constexpr int CRAFT_BASE = Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS;
    constexpr int OUTPUT_SLOT = CRAFT_BASE + Inventory::CRAFT_INPUT;

    static int  dragSrcSlot = -1;
    static bool didAutoPickup = false;

    // ── 左键按下（含双击检测） ──
    static clock_t lastClickTime = 0;
    static int     lastClickSlot = -1;
    if (mouseDown && hoveredSlot >= 0)
    {
        dragSrcSlot = hoveredSlot;
        didAutoPickup = false;
        bool wasCraft = (hoveredSlot >= CRAFT_BASE);

        clock_t now = clock();
        bool isDoubleClick = (hoveredSlot == lastClickSlot &&
            (now - lastClickTime) < CLOCKS_PER_SEC * 350 / 1000);

        if (isDoubleClick && !inventory.isDragging())
        {
            // 双击：收集所有同类物品到手上
            inventory.collectAll(hoveredSlot);
            didAutoPickup = inventory.isDragging();
            lastClickSlot = -1;
            lastClickTime = 0;
        }
        else if (!inventory.isDragging())
        {
            if (hoveredSlot == OUTPUT_SLOT)
            {
                inventory.takeCraftOutput(craftMgr);
                didAutoPickup = inventory.isDragging();
            }
            else
            {
                didAutoPickup = inventory.pickup(hoveredSlot, -1);
            }
            lastClickSlot = hoveredSlot;
            lastClickTime = now;
        }

        if (wasCraft)
            inventory.updateCraftingResult(craftMgr);
    }

    // ── 左键释放 ──
    if (mouseUp)
    {
        bool wasCraft = (hoveredSlot >= CRAFT_BASE);

        if (inventory.isDragging())
        {
            bool sameSlot = (hoveredSlot == dragSrcSlot);
            if (sameSlot && didAutoPickup)
            {

            }
            else if (hoveredSlot >= 0)
            {
                if (hoveredSlot == OUTPUT_SLOT)
                    inventory.takeCraftOutput(craftMgr);
                else
                    inventory.placeInto(hoveredSlot);
            }
        }
        else if (hoveredSlot >= 0)
        {
            if (hoveredSlot == OUTPUT_SLOT)
                inventory.takeCraftOutput(craftMgr);
            else
                inventory.pickup(hoveredSlot, -1);
        }

        if (wasCraft)
            inventory.updateCraftingResult(craftMgr);
    }

    // ── 右键按下（离散点击）+ 右键拖动 ──
    static int  lastRightDragSlot = -1;
    if (rightClick && hoveredSlot >= 0)
    {
        bool wasCraft = (hoveredSlot >= CRAFT_BASE);

        if (inventory.isDragging())
        {
            if (hoveredSlot == OUTPUT_SLOT)
                inventory.takeCraftOutput(craftMgr);
            else
            {
                inventory.placeOneInto(hoveredSlot);
                lastRightDragSlot = hoveredSlot;  // 防止拖动重复放置
            }
        }
        else
        {
            if (hoveredSlot == OUTPUT_SLOT)
                inventory.takeCraftOutput(craftMgr);
            else
                inventory.pickup(hoveredSlot, 1);
        }

        if (wasCraft)
            inventory.updateCraftingResult(craftMgr);
    }

    if (rightHeld && inventory.isDragging())
    {
        if (hoveredSlot >= 0 && hoveredSlot != lastRightDragSlot &&
            hoveredSlot != OUTPUT_SLOT)
        {
            inventory.placeOneInto(hoveredSlot);
            lastRightDragSlot = hoveredSlot;
        }
    }
    if (!rightHeld)
    {
        lastRightDragSlot = -1;
    }

    cleardevice();
    setbkcolor(RGB(10, 10, 30));
    renderer.drawBackground();
    renderer.drawImageCentered(&bgImage);

    // 只渲染背包/快捷栏/合成/护甲槽，不渲染熔炉槽
    for (int i = 0; i < Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS; ++i)
    {
        const auto &slot = inventory.getSlot(i);
        if (slot.blockType == BLOCK_AIR || slot.count <= 0) continue;
        int sx, sy, sw, sh;
        inventory.slotScreenRect(i, bgX, bgY, bgW, bgH, nativeW, nativeH, sx, sy, sw, sh);
        renderer.drawBlockIcon(sx, sy, sh, slot.blockType, slot.count);
    }

    if (inventory.isDragging())
    {
        int sz = (int) (16 * (double) bgW / nativeW);
        renderer.drawBlockIcon(mp.x - sz / 2, mp.y - sz / 2, sz,
            inventory.dragBlockType(), inventory.dragCount());
    }

    renderer.flushToScreen();
    FlushBatchDraw();
    return true;
}

bool updateFurnaceScreen(
    GameState &state,
    InputHandler &input,
    Inventory &inventory,
    FurnaceManager::State &fs,
    Renderer &renderer,
    IMAGE &imgSmoker,
    IMAGE &imgBurnProgress,
    IMAGE &imgLitProgress)
{
    if (input.isPressed(Key::Esc) || input.isPressed(Key::E))
    {
        if (inventory.isDragging()) inventory.cancelDrag();
        state = GameState::Gameplay;
        input.showMouseCursor(false);
        input.getMouseDelta();
        return true;
    }

    int smW = imgSmoker.getwidth();
    int smH = imgSmoker.getheight();
    int smX = (SCREEN_WIDTH - smW) / 2;
    int smY = (SCREEN_HEIGHT - smH) / 2;
    int nW = smW / 3;
    int nH = smH / 3;

    POINT mp = input.getMouseScreenPos();
    bool mouseDown = input.getMouseClick(0);
    bool mouseUp = input.getMouseRelease(0);
    bool rightClick = input.getMouseClick(1);
    bool rightHeld = input.isMouseButtonDown(1);

    constexpr int FURN_IN = Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS;
    constexpr int FURN_FU = FURN_IN + 1;
    constexpr int FURN_OU = FURN_IN + 2;

    auto hitFurnaceSlot = [&](int nx, int ny) -> int
    {
        if (nx >= 56 && nx < 72 && ny >= 17 && ny < 33) return FURN_IN;
        if (nx >= 56 && nx < 72 && ny >= 53 && ny < 69) return FURN_FU;
        if (nx >= 116 && nx < 132 && ny >= 35 && ny < 51) return FURN_OU;
        return -1;
    };

    int hoveredSlot = -1;
    {
        double sx = (double) smW / nW;
        double sy = (double) smH / nH;
        double nx = (mp.x - smX) / sx;
        double ny = (mp.y - smY) / sy;
        hoveredSlot = hitFurnaceSlot((int) nx, (int) ny);
    }
    if (hoveredSlot < 0)
        hoveredSlot = inventory.hitTest(mp.x, mp.y, smX, smY, smW, smH, nW, nH);

    static int  dragSrcSlot = -1;
    static bool didAutoPickup = false;

    auto canPlaceInFurnace = [&](int slotIdx, int itemType) -> bool
    {
        if (slotIdx == FURN_OU) return false;
        if (slotIdx == FURN_FU) return FurnaceManager::fuelValue(itemType) > 0.0;
        if (slotIdx == FURN_IN) return true;
        return true;
    };

    // ── 左键按下（含双击检测） ──
    static clock_t lastClickTime_f = 0;
    static int     lastClickSlot_f = -1;
    if (mouseDown && hoveredSlot >= 0)
    {
        dragSrcSlot = hoveredSlot;
        didAutoPickup = false;

        clock_t now = clock();
        bool isDoubleClick = (hoveredSlot == lastClickSlot_f &&
            (now - lastClickTime_f) < CLOCKS_PER_SEC * 350 / 1000);

        if (isDoubleClick && !inventory.isDragging())
        {
            inventory.collectAll(hoveredSlot);
            didAutoPickup = inventory.isDragging();
            lastClickSlot_f = -1;
            lastClickTime_f = 0;
        }
        else if (!inventory.isDragging())
        {
            didAutoPickup = inventory.pickup(hoveredSlot, -1);
            lastClickSlot_f = hoveredSlot;
            lastClickTime_f = now;
        }
    }
    if (mouseUp && inventory.isDragging())
    {
        bool sameSlot = (hoveredSlot == dragSrcSlot);
        if (!(sameSlot && didAutoPickup) && hoveredSlot >= 0)
        {
            if (canPlaceInFurnace(hoveredSlot, inventory.dragBlockType()))
                inventory.placeInto(hoveredSlot);
        }
    }
    else if (mouseUp && hoveredSlot >= 0 && !inventory.isDragging())
    {
        if (hoveredSlot == FURN_OU && inventory.getSlot(FURN_OU).blockType != BLOCK_AIR)
        {
            inventory.pickup(FURN_OU, -1);
            inventory.getSlot(FURN_OU) = { BLOCK_AIR, 0 };
        }
        else
            inventory.pickup(hoveredSlot, -1);
    }
    // ── 右键按下（离散点击）+ 右键拖动 ──
    static int  lastRightDragSlot_f = -1;
    if (rightClick && hoveredSlot >= 0)
    {
        if (inventory.isDragging())
        {
            if (canPlaceInFurnace(hoveredSlot, inventory.dragBlockType()))
            {
                inventory.placeOneInto(hoveredSlot);
                lastRightDragSlot_f = hoveredSlot;  // 防止拖动重复放置
            }
        }
        else
            inventory.pickup(hoveredSlot, 1);
    }

    if (rightHeld && inventory.isDragging())
    {
        if (hoveredSlot >= 0 && hoveredSlot != lastRightDragSlot_f)
        {
            if (canPlaceInFurnace(hoveredSlot, inventory.dragBlockType()))
                inventory.placeOneInto(hoveredSlot);
            lastRightDragSlot_f = hoveredSlot;
        }
    }
    if (!rightHeld)
    {
        lastRightDragSlot_f = -1;
    }

    cleardevice();
    setbkcolor(RGB(10, 10, 30));
    renderer.drawBackground();
    renderer.drawImageCentered(&imgSmoker);

    for (int i = 0; i < Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS; ++i)
    {
        const auto &slot = inventory.getSlot(i);
        if (slot.blockType == BLOCK_AIR || slot.count <= 0) continue;
        int sx, sy, sw, sh;
        inventory.slotScreenRect(i, smX, smY, smW, smH, nW, nH, sx, sy, sw, sh);
        renderer.drawBlockIcon(sx, sy, sh, slot.blockType, slot.count);
    }

    auto drawFurnSlot = [&](int nx, int ny, int slotIdx)
    {
        const auto &slot = inventory.getSlot(slotIdx);
        if (slot.blockType == BLOCK_AIR || slot.count <= 0) return;
        double sx = (double) smW / nW;
        double sy = (double) smH / nH;
        int sz = (int) (Inventory::SLOT_SIZE * sx);
        int dx = smX + (int) (nx * sx);
        int dy = smY + (int) (ny * sy);
        renderer.drawBlockIcon(dx, dy, sz, slot.blockType, slot.count);
    };
    drawFurnSlot(56, 17, FURN_IN);
    drawFurnSlot(56, 53, FURN_FU);
    drawFurnSlot(116, 35, FURN_OU);

    if (fs.active && imgBurnProgress.getwidth() > 0)
    {
        int progW = imgBurnProgress.getwidth();
        int progH = imgBurnProgress.getheight();
        int cols = (int) (fs.burnProgress * progW);
        if (cols > progW) cols = progW;
        if (cols > 0)
        {
            double sx = (double) smW / nW;
            double sy = (double) smH / nH;
            int barX = smX + (int) (80 * sx);
            int barY = smY + (int) (34 * sy);
            int barW = (int) (22 * sx);
            int barH = (int) (16 * sy);
            DWORD *pBuf = GetImageBuffer(&imgBurnProgress);
            int pSrcW = imgBurnProgress.getwidth();
            if (pBuf)
            {
                int dispCols = cols * barW / progW;
                if (dispCols < 1) dispCols = 1;
                for (int dy = 0; dy < barH; ++dy)
                {
                    int py = barY + dy;
                    if (py < 0 || py >= SCREEN_HEIGHT) continue;
                    int srcY = dy * progH / barH;
                    for (int dx = 0; dx < dispCols; ++dx)
                    {
                        int px = barX + dx;
                        if (px < 0 || px >= SCREEN_WIDTH) continue;
                        int srcX = dx * progW / barW;
                        if (srcX >= cols) srcX = cols - 1;
                        DWORD c = pBuf[srcY * pSrcW + srcX];
                        if (c == 0) continue;
                        DWORD *bits = renderer.getPixelBits();
                        bits[py * SCREEN_WIDTH + px] = c;
                    }
                }
            }
        }
    }

    if (fs.active && imgLitProgress.getwidth() > 0)
    {
        double fuelRatio = (fs.fuelCapacity > 0.0)
            ? fs.burnTimeRemain / fs.fuelCapacity : 0.0;
        if (fuelRatio < 0.0) fuelRatio = 0.0;
        if (fuelRatio > 1.0) fuelRatio = 1.0;

        int visibleRows = (int) (fuelRatio * 14.0 + 0.5);
        if (visibleRows < 0) visibleRows = 0;
        if (visibleRows > 14) visibleRows = 14;

        if (visibleRows > 0)
        {
            double sx = (double) smW / nW;
            double sy = (double) smH / nH;
            int fireX = smX + (int) (56 * sx);
            int fireY = smY + (int) (36 * sy);
            int fireW = (int) (14 * sx);
            int fireH = (int) (14 * sy);
            DWORD *pBuf = GetImageBuffer(&imgLitProgress);
            int pSrcW = imgLitProgress.getwidth();
            int pSrcH = imgLitProgress.getheight();
            if (pBuf && pSrcW > 0 && pSrcH > 0)
            {
                int emptyRows = 14 - visibleRows;
                int dispStartRow = emptyRows * fireH / 14;
                int srcStartRow = emptyRows * pSrcH / 14;
                int dispCount = fireH - dispStartRow;
                int srcCount = pSrcH - srcStartRow;
                if (dispCount < 1) dispCount = 1;
                if (srcCount < 1) srcCount = 1;
                for (int dy = 0; dy < dispCount; ++dy)
                {
                    int py = fireY + dispStartRow + dy;
                    if (py < 0 || py >= SCREEN_HEIGHT) continue;
                    int srcY = srcStartRow + dy * srcCount / dispCount;
                    if (srcY >= pSrcH) srcY = pSrcH - 1;
                    for (int dx = 0; dx < fireW; ++dx)
                    {
                        int px = fireX + dx;
                        if (px < 0 || px >= SCREEN_WIDTH) continue;
                        int srcX = dx * pSrcW / fireW;
                        if (srcX >= pSrcW) srcX = pSrcW - 1;
                        DWORD c = pBuf[srcY * pSrcW + srcX];
                        if (c == 0) continue;
                        DWORD *bits = renderer.getPixelBits();
                        bits[py * SCREEN_WIDTH + px] = c;
                    }
                }
            }
        }
    }

    if (inventory.isDragging())
    {
        double sx = (double) smW / nW;
        int sz = (int) (16 * sx);
        renderer.drawBlockIcon(mp.x - sz / 2, mp.y - sz / 2, sz,
            inventory.dragBlockType(), inventory.dragCount());
    }

    renderer.flushToScreen();
    FlushBatchDraw();
    return true;
}

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
    const wchar_t *clickPath)
{
    if (input.isPressed(Key::Esc))
    {
        state = GameState::Gameplay;
        input.showMouseCursor(false);
        input.getMouseDelta();
        return true;
    }

    int btnX = (SCREEN_WIDTH - btnW) / 2;
    int btn1Y = SCREEN_HEIGHT / 2 - 15;
    int btn2Y = btn1Y + btnH + 15;
    constexpr int BORDER = 2;
    POINT mp = input.getMouseScreenPos();
    bool hover1 = (mp.x >= btnX - BORDER && mp.x < btnX + btnW + BORDER &&
        mp.y >= btn1Y - BORDER && mp.y < btn1Y + btnH + BORDER);
    bool hover2 = (mp.x >= btnX - BORDER && mp.x < btnX + btnW + BORDER &&
        mp.y >= btn2Y - BORDER && mp.y < btn2Y + btnH + BORDER);
    bool mouseClick = input.getMouseClick(0);
    if (mouseClick && hover2)
    {
        playSFX(clickPath);
        input.requestQuit();
    }
    if (mouseClick && hover1)
    {
        playSFX(clickPath);
        world = World();
        camera = Camera4D();
        map3D.valid = false;
        cam3U = cam3V = cam3Y = cam3Yaw = cam3Pitch = 0;
        selectedSlot = 0;
        pendingSliceRotation = 0.0;
        loginBgReady = false;
        state = GameState::Login;
        input.showMouseCursor(true);
        return true;
    }

    cleardevice();
    setbkcolor(RGB(10, 10, 30));
    renderer.drawBackground();
    renderer.drawButton(btnX, btn1Y, btnW, btnH,
        &imgButton, &imgButton, &imgButton,
        L"返回标题页", hover1, hover1 && input.isMouseButtonDown(0));
    renderer.drawButton(btnX, btn2Y, btnW, btnH,
        &imgButton, &imgButton, &imgButton,
        L"退出游戏", hover2, hover2 && input.isMouseButtonDown(0));
    renderer.flushToScreen();
    FlushBatchDraw();
    return true;
}