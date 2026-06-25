/**
 * @file main.cpp
 * @author 陈煜仕 (Mxster@qq.com)
 * @brief Minecrafx 主入口
 * @version 3.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 * 初始化窗口、世界、摄像机、渲染器
 * 处理主循环的移动、旋转、方块交互和渲染
 */

#include <graphics.h>
#include <windows.h>
#include <imm.h>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <mmsystem.h>

#include "linalg.h"
#include "world.h"
#include "camera.h"
#include "renderer.h"
#include "constant.h"
#include "input/input_handler.h"
#include "inventory.h"
#include "crafting.h"

 // ---- 游戏状态 ----
enum class GameState
{
    Login,         // 登录页（选择模式）
    Gameplay,      // 正常游戏
    Inventory,     // 背包界面（E 键打开，2×2 合成）
    CraftingTable, // 工作台界面（右键工作台打开，3×3 合成）
    Paused         // 暂停菜单（Esc 键打开）
};

// 整数倍最邻近放大（像素无后期处理）
static void nearestScale(IMAGE &dst, const IMAGE &src, int scale)
{
    int srcW = src.getwidth(), srcH = src.getheight();
    if (srcW <= 0 || srcH <= 0 || scale <= 1) { dst = src; return; }
    Resize(&dst, srcW * scale, srcH * scale);
    DWORD *s = GetImageBuffer(const_cast<IMAGE *>(&src));
    DWORD *d = GetImageBuffer(&dst);
    if (s && d)
    {
        int dstW = srcW * scale;
        for (int y = 0; y < dst.getheight(); ++y)
            for (int x = 0; x < dstW; ++x)
                d[y * dstW + x] = s[(y / scale) * srcW + (x / scale)];
    }
}

// 方块过程色（哈希）
static COLORREF blockColor(int x, int y, int z, int w)
{
    unsigned int h = (unsigned int) (x * 73856093 + y * 19349663 + z * 83492791 + w * 39916801);
    h = (h ^ (h >> 13)) * 0x9e3779b9;
    int r = 60 + (h & 0xFF) % 156;
    int g = 60 + ((h >> 8) & 0xFF) % 156;
    int b = 60 + ((h >> 16) & 0xFF) % 156;
    return RGB(r, g, b);
}

int main()
{
    initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
    HWND hwnd = GetHWnd();

    // 禁用输入法，防止 Shift 被系统拦截切换中英文
    ImmAssociateContext(hwnd, NULL);

    // ---- BGM ----
    {
        // 获取 exe 所在目录，构建 mp3 绝对路径
        wchar_t exePath[MAX_PATH], mp3Path[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t *lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *lastSlash = L'\0';  // 去掉 exe 文件名
        // 从 bin/ 回到项目根目录
        wcscpy(mp3Path, exePath);
        wcscat(mp3Path, L"\\..\\assert\\sounds\\方块的世界.mp3");
        // 转换为绝对路径
        wchar_t fullPath[MAX_PATH];
        GetFullPathNameW(mp3Path, MAX_PATH, fullPath, NULL);

        wchar_t cmd[512], errBuf[256];
        swprintf(cmd, 512, L"open \"%ls\" alias bgm", fullPath);
        MCIERROR err = mciSendStringW(cmd, NULL, 0, NULL);
        if (err == 0)
        {
            mciSendStringW(L"play bgm repeat", NULL, 0, NULL);
            mciSendStringW(L"setaudio bgm volume to 100", NULL, 0, NULL);
        }
        else
        {
            mciGetErrorStringW(err, errBuf, 256);
            MessageBoxW(NULL, errBuf, L"BGM 加载失败", MB_ICONWARNING);
        }
    }

    // ---- 预计算音效路径 ----
    wchar_t sfxDir[MAX_PATH];
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t *lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *lastSlash = L'\0';
        wcscpy(sfxDir, exePath);
        wcscat(sfxDir, L"\\..\\assert\\sounds\\");
        wchar_t tmp[MAX_PATH];
        GetFullPathNameW(sfxDir, MAX_PATH, tmp, NULL);
        wcscpy(sfxDir, tmp);
    }

    wchar_t digPath[4][MAX_PATH], stepPath[6][MAX_PATH], clickPath[MAX_PATH];
    for (int i = 0; i < 4; ++i)
        swprintf(digPath[i], MAX_PATH, L"%lsdig%d.mp3", sfxDir, i + 1);
    for (int i = 0; i < 6; ++i)
        swprintf(stepPath[i], MAX_PATH, L"%lsstep%d.mp3", sfxDir, i + 1);
    swprintf(clickPath, MAX_PATH, L"%lsclick_stereo.mp3", sfxDir);

    // 音效播放：先关旧音效再开新的（异步，不阻塞游戏）
    auto playSFX = [](const wchar_t *path)
    {
        mciSendStringW(L"close sfx", NULL, 0, NULL);
        wchar_t cmd[512];
        swprintf(cmd, 512, L"open \"%ls\" alias sfx", path);
        mciSendStringW(cmd, NULL, 0, NULL);
        mciSendStringW(L"play sfx", NULL, 0, NULL);
    };

    BeginBatchDraw();

    World world;
    Camera4D camera;
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);

    // ---- 加载方块贴图 ----
    renderer.loadBlockTextures();
    renderer.loadHotbar();
    renderer.loadInventoryIcons();

    // ---- 地形高度函数（两种模式共用） ----
    auto terrainHeight = [](int x, int z, int w) -> int
    {
        double h = 0.0;
        h += std::sin(x * 0.25) * std::cos(z * 0.30) * 6.0;
        h += std::cos(x * 0.45 + 1.2) * std::sin(z * 0.55) * 4.0;
        h += std::sin((x + z) * 0.35) * 3.0;
        h += std::cos(w * 0.40) * std::sin(x * 0.50 + z * 0.30) * 2.5;
        h += std::sin(x * 0.70 - z * 0.60) * std::cos(w * 0.50) * 2.0;
        h += 5.0;
        return (int) std::floor(h);
    };

    constexpr int MX = 48, MZ = 48, MW = 24;

    // ---- 移动模式 ----
    bool flyMode = true;
    double verticalVel = 0.0;
    bool onGround = false;
    clock_t lastSpacePress = 0;
    constexpr double DOUBLE_TAP_MS = 350;
    constexpr double GRAVITY = 25.0;
    constexpr double JUMP_VEL = 8.5;
    double sliceVelocity = 0.0;
    double pendingSliceRotation = 0.0;  // 累积未应用的切片旋转（用于节流地图重建）
    clock_t lastFrame = clock();
    int interactCooldown = 0;  // 放置/摧毁冷却帧数
    int selectedSlot = 0;      // 热键栏选中槽位
    clock_t lastStepTime = 0;  // 上次脚步声时间

    // ---- 3D 地图 + 3D 摄像机（选模式后初始化） ----
    Map3D map3D;
    double cam3U = 0, cam3V = 0, cam3Y = 0;
    double cam3Yaw = 0, cam3Pitch = 0;

    SetWindowText(hwnd, L"Minecrafx");

    // ---- 预加载 GUI 图片 ----
    IMAGE imgInventory, imgCraftingTable, imgButton;
    IMAGE imgIsles;  // 登录页背景
    loadimage(&imgIsles, L"../assert/start.png");
    // 加载原图 → 3x 最邻近放大（无后期处理）
    {
        IMAGE imgInvNative, imgCTNative;
        loadimage(&imgInvNative, L"../assert/gui/widget/inventory.png");
        loadimage(&imgCTNative, L"../assert/gui/widget/crafting_table.png");
        nearestScale(imgInventory, imgInvNative, 3);
        nearestScale(imgCraftingTable, imgCTNative, 3);
    }
    // 按钮贴图：594×54，无需缩放
    constexpr int BTN_W = 594, BTN_H = 54;
    loadimage(&imgButton, L"../assert/gui/widget/button.png");

    // ---- 游戏状态 ----
    GameState state = GameState::Login;
    Inventory inventory;
    CraftingManager craftMgr;  // 合成配方

    InputHandler input(hwnd);
    input.showMouseCursor(true);  // 登录页显示鼠标
    bool loginBgReady = false;    // 登录页模糊背景是否就绪
    while (!input.isQuitRequested())
    {
        input.update();

        // ---- 帧间隔 ----
        clock_t nowFrame = clock();
        double dt = static_cast<double>(nowFrame - lastFrame) / CLOCKS_PER_SEC;
        lastFrame = nowFrame;
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0.0) dt = 0.001;

        // ================================================================
        // 登录页：选择模式
        // ================================================================
        if (state == GameState::Login)
        {
            // 绘制背景（拉伸铺满至 DIB），首帧做高斯模糊并缓存
            {
                DWORD *bgBuf = GetImageBuffer(&imgIsles);
                int bgW = imgIsles.getwidth(), bgH = imgIsles.getheight();
                DWORD *bits = renderer.getPixelBits();
                if (bgBuf && bgW > 0 && bgH > 0 && bits)
                {
                    if (!loginBgReady)
                    {
                        for (int y = 0; y < SCREEN_HEIGHT; ++y)
                        {
                            int sy = y * bgH / SCREEN_HEIGHT;
                            for (int x = 0; x < SCREEN_WIDTH; ++x)
                            {
                                int sx = x * bgW / SCREEN_WIDTH;
                                DWORD c = bgBuf[sy * bgW + sx];
                                if (c != 0 && c != RGB(0, 0, 0))
                                    bits[y * SCREEN_WIDTH + x] = c;
                            }
                        }
                        renderer.captureBackground();
                        renderer.applyGaussianBlur();
                        renderer.drawBackground();
                        loginBgReady = true;
                    }
                    else
                    {
                        renderer.drawBackground();
                    }
                }
            }

            // 两个按钮
            constexpr int LOGIN_BTN_W = 594, LOGIN_BTN_H = 54;
            constexpr int BORDER = 2;
            int btn1X = (SCREEN_WIDTH - LOGIN_BTN_W) / 2;
            int btn1Y = SCREEN_HEIGHT / 2 - 10;
            int btn2X = btn1X;
            int btn2Y = btn1Y + LOGIN_BTN_H + 15;

            POINT mp = input.getMouseScreenPos();
            bool hover1 = (mp.x >= btn1X - BORDER && mp.x < btn1X + LOGIN_BTN_W + BORDER &&
                mp.y >= btn1Y - BORDER && mp.y < btn1Y + LOGIN_BTN_H + BORDER);
            bool hover2 = (mp.x >= btn2X - BORDER && mp.x < btn2X + LOGIN_BTN_W + BORDER &&
                mp.y >= btn2Y - BORDER && mp.y < btn2Y + LOGIN_BTN_H + BORDER);
            bool mouseClick = input.getMouseClick(0);
            bool click1 = mouseClick && hover1;
            bool click2 = mouseClick && hover2;

            // 生存模式
            if (click1)
            {
                playSFX(clickPath);
                // 生成丘陵地形
                for (int x = 0; x < MX; ++x)
                    for (int z = 0; z < MZ; ++z)
                        for (int w = 0; w < MW; ++w)
                        {
                            int ht = terrainHeight(x, z, w);
                            if (ht < 1) ht = 1;
                            for (int y = 0; y < ht; ++y)
                            {
                                int type = BLOCK_DIRT;
                                if (y == ht - 1) type = BLOCK_GRASS;
                                world.set(IVec4(x, y, z, w), type);
                            }
                        }
                // 树木
                srand(42);
                for (int i = 0; i < 120; ++i)
                {
                    int tx = rand() % (MX - 2) + 1, tz = rand() % (MZ - 2) + 1, tw = rand() % (MW - 2) + 1;
                    int groundY = terrainHeight(tx, tz, tw);
                    if (groundY < 2) continue;
                    int trunkH = 3 + rand() % 3;
                    for (int ty = groundY; ty < groundY + trunkH; ++ty)
                        world.set(IVec4(tx, ty, tz, tw), BLOCK_LOG);
                    int leafBase = groundY + trunkH - 1;
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dz = -1; dz <= 1; ++dz)
                            for (int dw = -1; dw <= 1; ++dw)
                            {
                                int lx = tx + dx, lz = tz + dz, lw = tw + dw;
                                if (lx < 0 || lx >= MX || lz < 0 || lz >= MZ || lw < 0 || lw >= MW) continue;
                                if (dx == 0 && dz == 0 && dw == 0) continue;
                                if ((dx != 0) + (dz != 0) + (dw != 0) >= 2 && (rand() % 3) != 0) continue;
                                for (int ly = leafBase; ly <= leafBase + 2; ++ly)
                                    if (ly < 20) world.set(IVec4(lx, ly, lz, lw), BLOCK_LEAVES);
                            }
                }
                // 矿物生成
                srand(12345);
                for (int i = 0; i < 800; ++i)
                {
                    int ox = rand() % MX, oz = rand() % MZ, ow = rand() % MW;
                    int oy = rand() % 5 + 1;  // 地下 1~5 层
                    int oreType = BLOCK_DIAMOND_ORE;
                    int r = rand() % 100;
                    if (r < 60) oreType = BLOCK_IRON_ORE;
                    else if (r < 90) oreType = BLOCK_GOLD_ORE;
                    int clusterSize = 2 + rand() % 4;
                    for (int j = 0; j < clusterSize; ++j)
                    {
                        int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1, dz = (rand() % 3) - 1, dw = (rand() % 3) - 1;
                        int nx = ox + dx, ny = oy + dy, nz = oz + dz, nw = ow + dw;
                        if (nx >= 0 && nx < MX && nz >= 0 && nz < MZ && nw >= 0 && nw < MW && ny >= 0 && ny < terrainHeight(nx, nz, nw) - 1)
                            if (world.get(IVec4(nx, ny, nz, nw)) == BLOCK_DIRT)
                                world.set(IVec4(nx, ny, nz, nw), oreType);
                    }
                }
                // 初始地图
                map3D = generateMap3D(world, camera, 0.5, [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                { Plane2D pl = camera.getViewPlane(); Vec3 cXZW = Vec3::fromVec4(camera.getPos()); cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p); cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q); cam3Y = camera.getPos().y - map3D.camRef4D.y; }
                pendingSliceRotation = 0.0;  // 新地图，重置累积
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            // 创造模式（超平坦）
            if (click2)
            {
                playSFX(clickPath);
                constexpr int CF_X = 16, CF_Z = 16, CF_W = 16;
                for (int x = 0; x < CF_X; ++x)
                    for (int z = 0; z < CF_Z; ++z)
                        for (int w = 0; w < CF_W; ++w)
                            world.set(IVec4(x, 0, z, w), BLOCK_GRASS);
                map3D = generateMap3D(world, camera, 0.5, [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                { Plane2D pl = camera.getViewPlane(); Vec3 cXZW = Vec3::fromVec4(camera.getPos()); cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p); cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q); cam3Y = camera.getPos().y - map3D.camRef4D.y; }
                pendingSliceRotation = 0.0;  // 新地图，重置累积
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            // 渲染按钮
            renderer.drawButton(btn1X, btn1Y, LOGIN_BTN_W, LOGIN_BTN_H,
                &imgButton, &imgButton, &imgButton, L"生存模式", hover1, hover1 && input.isMouseButtonDown(0));
            renderer.drawButton(btn2X, btn2Y, LOGIN_BTN_W, LOGIN_BTN_H,
                &imgButton, &imgButton, &imgButton, L"创造模式", hover2, hover2 && input.isMouseButtonDown(0));

            renderer.flushToScreen();   // DIB → 屏幕
            FlushBatchDraw();
            continue;
        }

        // ================================================================
        // 非 Gameplay 状态处理
        // ================================================================
        if (state == GameState::Inventory)
        {
            if (input.isPressed(Key::Esc) || input.isPressed(Key::E))
            {
                playSFX(clickPath);
                if (inventory.isDragging()) inventory.cancelDrag();
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            // 计算 inventory 图片显示区域
            int invDispW = imgInventory.getwidth();
            int invDispH = imgInventory.getheight();
            int invDispX = (SCREEN_WIDTH - invDispW) / 2;
            int invDispY = (SCREEN_HEIGHT - invDispH) / 2;
            // 原生尺寸（加载 3x 前）
            int invNativeW = invDispW / 3;
            int invNativeH = invDispH / 3;

            // 鼠标事件（边沿触发）
            POINT mp = input.getMouseScreenPos();
            bool mouseDown = input.getMouseClick(0);     // 左键按下
            bool mouseUp = input.getMouseRelease(0);    // 左键释放
            bool rightClick = input.getMouseClick(1);      // 右键按下

            int hoveredSlot = inventory.hitTest(mp.x, mp.y,
                invDispX, invDispY, invDispW, invDispH,
                invNativeW, invNativeH);

            constexpr int CRAFT_BASE_SLOT = Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS;
            constexpr int OUTPUT_SLOT = CRAFT_BASE_SLOT + Inventory::CRAFT_INPUT;

            // 拖拽状态追踪
            static int  dragSrcSlot = -1;
            static bool didAutoPickup = false;

            // ═══════════════════════════════════════════════════════
            // 左键按下：记录起点；手空 → 自动拿起（长按起点）
            // ═══════════════════════════════════════════════════════
            if (mouseDown && hoveredSlot >= 0)
            {
                dragSrcSlot = hoveredSlot;
                didAutoPickup = false;
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (!inventory.isDragging())
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
                }
                if (wasCraft)
                    inventory.updateCraftingResult(craftMgr);
            }

            // ═══════════════════════════════════════════════════════
            // 左键释放
            // ═══════════════════════════════════════════════════════
            if (mouseUp)
            {
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (inventory.isDragging())
                {
                    bool sameSlot = (hoveredSlot == dragSrcSlot);

                    if (sameSlot && didAutoPickup)
                    {
                        // 同格 + 本周期拿起 → 点按拿起，保留物品
                    }
                    else if (hoveredSlot >= 0)
                    {
                        // 拖放终点 / 点按放下
                        if (hoveredSlot == OUTPUT_SLOT)
                            inventory.takeCraftOutput(craftMgr);
                        else
                            inventory.placeInto(hoveredSlot);
                    }
                    // 不在任何格子上 → 保留在手上，等下次点击再放
                }
                else if (hoveredSlot >= 0)
                {
                    // 手空 + 未自动拿起（如输出槽变空后点按）
                    if (hoveredSlot == OUTPUT_SLOT)
                        inventory.takeCraftOutput(craftMgr);
                    else
                        inventory.pickup(hoveredSlot, -1);
                }

                if (wasCraft)
                    inventory.updateCraftingResult(craftMgr);
            }

            // ═══════════════════════════════════════════════════════
            // 右键：拿起 1 个 / 放下 1 个
            // ═══════════════════════════════════════════════════════
            if (rightClick && hoveredSlot >= 0)
            {
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (inventory.isDragging())
                {
                    // 手上有物品 → 放下 1 个
                    if (hoveredSlot == OUTPUT_SLOT)
                        inventory.takeCraftOutput(craftMgr);
                    else
                        inventory.placeOneInto(hoveredSlot);
                }
                else
                {
                    // 手上空 → 拿起 1 个
                    if (hoveredSlot == OUTPUT_SLOT)
                        inventory.takeCraftOutput(craftMgr);
                    else
                        inventory.pickup(hoveredSlot, 1);
                }

                if (wasCraft)
                    inventory.updateCraftingResult(craftMgr);
            }

            // 渲染背包界面
            cleardevice();
            setbkcolor(RGB(10, 10, 30));
            renderer.drawBackground();
            renderer.drawImageCentered(&imgInventory);

            // 绘制所有槽位的物品图标
            for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i)
            {
                const auto &slot = inventory.getSlot(i);
                if (slot.blockType == BLOCK_AIR || slot.count <= 0) continue;
                int sx, sy, sw, sh;
                inventory.slotScreenRect(i,
                    invDispX, invDispY, invDispW, invDispH,
                    invNativeW, invNativeH,
                    sx, sy, sw, sh);
                renderer.drawBlockIcon(sx, sy, sh, slot.blockType, slot.count);
            }

            // 绘制拖拽中的物品（跟随鼠标）
            if (inventory.isDragging())
            {
                int sz = (int) (16 * (double) invDispW / invNativeW);
                renderer.drawBlockIcon(mp.x - sz / 2, mp.y - sz / 2, sz,
                    inventory.dragBlockType(), inventory.dragCount());
            }

            renderer.flushToScreen();
            FlushBatchDraw();
            continue;
        }

        // ================================================================
        // 工作台界面（3×3 合成，E / Esc 关闭）
        // ================================================================
        if (state == GameState::CraftingTable)
        {
            if (input.isPressed(Key::Esc) || input.isPressed(Key::E))
            {
                playSFX(clickPath);
                if (inventory.isDragging()) inventory.cancelDrag();
                inventory.setCraftMode(Inventory::CM_Inventory2x2);
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            int ctDispW = imgCraftingTable.getwidth();
            int ctDispH = imgCraftingTable.getheight();
            int ctDispX = (SCREEN_WIDTH - ctDispW) / 2;
            int ctDispY = (SCREEN_HEIGHT - ctDispH) / 2;
            int ctNativeW = ctDispW / 3;
            int ctNativeH = ctDispH / 3;

            POINT mp = input.getMouseScreenPos();
            bool mouseDown = input.getMouseClick(0);
            bool mouseUp = input.getMouseRelease(0);
            bool rightClick = input.getMouseClick(1);

            int hoveredSlot = inventory.hitTest(mp.x, mp.y,
                ctDispX, ctDispY, ctDispW, ctDispH,
                ctNativeW, ctNativeH);

            constexpr int CRAFT_BASE_SLOT = Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS;
            constexpr int OUTPUT_SLOT = CRAFT_BASE_SLOT + Inventory::CRAFT_INPUT;

            static int  dragSrcSlot = -1;
            static bool didAutoPickup = false;

            // ── 左键按下 ──
            if (mouseDown && hoveredSlot >= 0)
            {
                dragSrcSlot = hoveredSlot;
                didAutoPickup = false;
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (!inventory.isDragging())
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
                }
                if (wasCraft)
                    inventory.updateCraftingResult(craftMgr);
            }

            // ── 左键释放 ──
            if (mouseUp)
            {
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (inventory.isDragging())
                {
                    bool sameSlot = (hoveredSlot == dragSrcSlot);
                    if (sameSlot && didAutoPickup)
                    {
                        // 点按拿起，保留物品
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

            // ── 右键 ──
            if (rightClick && hoveredSlot >= 0)
            {
                bool wasCraft = (hoveredSlot >= CRAFT_BASE_SLOT);

                if (inventory.isDragging())
                {
                    if (hoveredSlot == OUTPUT_SLOT)
                        inventory.takeCraftOutput(craftMgr);
                    else
                        inventory.placeOneInto(hoveredSlot);
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

            // 渲染工作台界面
            cleardevice();
            setbkcolor(RGB(10, 10, 30));
            renderer.drawBackground();
            renderer.drawImageCentered(&imgCraftingTable);

            for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i)
            {
                const auto &slot = inventory.getSlot(i);
                if (slot.blockType == BLOCK_AIR || slot.count <= 0) continue;
                int sx, sy, sw, sh;
                inventory.slotScreenRect(i,
                    ctDispX, ctDispY, ctDispW, ctDispH,
                    ctNativeW, ctNativeH,
                    sx, sy, sw, sh);
                renderer.drawBlockIcon(sx, sy, sh, slot.blockType, slot.count);
            }

            if (inventory.isDragging())
            {
                int sz = (int) (16 * (double) ctDispW / ctNativeW);
                renderer.drawBlockIcon(mp.x - sz / 2, mp.y - sz / 2, sz,
                    inventory.dragBlockType(), inventory.dragCount());
            }

            renderer.flushToScreen();
            FlushBatchDraw();
            continue;
        }

        if (state == GameState::Paused)
        {
            if (input.isPressed(Key::Esc))
            {
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            int btnX = (SCREEN_WIDTH - BTN_W) / 2;
            int btn1Y = SCREEN_HEIGHT / 2 - 15;
            int btn2Y = btn1Y + BTN_H + 15;
            constexpr int BORDER = 2;
            POINT mp = input.getMouseScreenPos();
            bool hover1 = (mp.x >= btnX - BORDER && mp.x < btnX + BTN_W + BORDER &&
                mp.y >= btn1Y - BORDER && mp.y < btn1Y + BTN_H + BORDER);
            bool hover2 = (mp.x >= btnX - BORDER && mp.x < btnX + BTN_W + BORDER &&
                mp.y >= btn2Y - BORDER && mp.y < btn2Y + BTN_H + BORDER);
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
                continue;
            }

            cleardevice();
            setbkcolor(RGB(10, 10, 30));
            renderer.drawBackground();
            renderer.drawButton(btnX, btn1Y, BTN_W, BTN_H,
                &imgButton, &imgButton, &imgButton,
                L"返回标题页", hover1, hover1 && input.isMouseButtonDown(0));
            renderer.drawButton(btnX, btn2Y, BTN_W, BTN_H,
                &imgButton, &imgButton, &imgButton,
                L"退出游戏", hover2, hover2 && input.isMouseButtonDown(0));
            renderer.flushToScreen();
            FlushBatchDraw();
            continue;
        }

        // ================================================================
        // Gameplay 状态下的模式切换检测
        // ================================================================
        if (input.isPressed(Key::Esc))
        {
            state = GameState::Paused;
            renderer.captureBackground();
            renderer.applyGaussianBlur();
            input.showMouseCursor(true);
            continue;
        }
        if (input.isPressed(Key::E))
        {
            playSFX(clickPath);
            inventory.setCraftMode(Inventory::CM_Inventory2x2);
            state = GameState::Inventory;
            renderer.captureBackground();
            renderer.applyGaussianBlur();
            input.showMouseCursor(true);
            continue;
        }

        // ---- 键盘移动（3D 空间，方向由 4D 相机基向量投影决定） ----
        {
            Plane2D pl = map3D.plane;
            const Vec4 &fwd = camera.getForward(), &rht = camera.getRight();
            double fU = fwd.x * pl.p.x + fwd.z * pl.p.z + fwd.w * pl.p.w;
            double fV = fwd.x * pl.q.x + fwd.z * pl.q.z + fwd.w * pl.q.w;
            double rU = rht.x * pl.p.x + rht.z * pl.p.z + rht.w * pl.p.w;
            double rV = rht.x * pl.q.x + rht.z * pl.q.z + rht.w * pl.q.w;
            // 同步 cam3Yaw
            cam3Yaw = std::atan2(fU, fV);

            double moveU = 0, moveV = 0;
            double speed = MOVE_SPEED * dt;
            bool isMoving = false;
            if (input.isKeyDown(Key::W)) { moveU += fU * speed; moveV += fV * speed; isMoving = true; }
            if (input.isKeyDown(Key::S)) { moveU -= fU * speed; moveV -= fV * speed; isMoving = true; }
            if (input.isKeyDown(Key::D)) { moveU += rU * speed; moveV += rV * speed; isMoving = true; }
            if (input.isKeyDown(Key::A)) { moveU -= rU * speed; moveV -= rV * speed; isMoving = true; }

            double oldU = cam3U, oldV = cam3V, oldY = cam3Y;


            // 模式切换 & 跳跃
            if (input.isPressed(Key::Space))
            {
                clock_t now = clock();
                double elapsed = static_cast<double>(now - lastSpacePress) * 1000.0 / CLOCKS_PER_SEC;
                if (elapsed < DOUBLE_TAP_MS && lastSpacePress != 0)
                {
                    flyMode = !flyMode; verticalVel = 0.0; lastSpacePress = 0;
                }
                else
                {
                    lastSpacePress = now;
                    if (!flyMode && onGround) { verticalVel = JUMP_VEL; onGround = false; }
                }
            }

            double moveY = 0;
            if (flyMode)
            {
                if (input.isKeyDown(Key::Space)) moveY = speed;
                if (input.isKeyDown(Key::LShift) || input.isKeyDown(Key::RShift)) moveY = -speed;
            }
            else
            {
                verticalVel -= GRAVITY * dt;
                moveY = verticalVel * dt;
            }

            // ---- 碰撞检测（棱柱多边形 + 法向量） ----
            double cR = CYLINDER_R, cH = CYLINDER_H;
            auto collideNormal = [&](double u, double v, double y, double &nU, double &nV) -> bool
            {
                nU = nV = 0; double bestDist = cR;
                for (size_t pi = 0; pi < map3D.prisms.size(); ++pi)
                {
                    auto &ab = map3D.aabbs[pi];
                    if (u - cR > ab.uMax || u + cR < ab.uMin ||
                        v - cR > ab.vMax || v + cR < ab.vMin ||
                        y - cH > ab.yMax || y < ab.yMin) continue;
                    auto &pr = map3D.prisms[pi];
                    int n = (int) pr.u.size();
                    // 点在凸多边形内？（修复地板穿透）
                    bool inside = true;
                    for (int i = 0; i < n && inside; ++i)
                    {
                        int j = (i + 1) % n;
                        double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                        if (eu * (v - pr.v[i]) - ev * (u - pr.u[i]) < -1e-9) inside = false;
                    }
                    if (inside) return true;
                    // 圆 vs 凸多边形边
                    for (int i = 0; i < n; ++i)
                    {
                        int j = (i + 1) % n;
                        double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                        double len2 = eu * eu + ev * ev;
                        double t = ((u - pr.u[i]) * eu + (v - pr.v[i]) * ev) / len2;
                        if (t < 0) t = 0; else if (t > 1) t = 1;
                        double cu = pr.u[i] + t * eu, cv = pr.v[i] + t * ev;
                        double du = u - cu, dv = v - cv;
                        double dist = std::sqrt(du * du + dv * dv);
                        if (dist < bestDist) { bestDist = dist; nU = du / dist; nV = dv / dist; }
                    }
                }
                return bestDist < cR;
            };
            auto mapCollide = [&](double u, double v, double y) -> bool
            {
                double nU, nV; return collideNormal(u, v, y, nU, nV);
            };

            double newU = cam3U, newV = cam3V, newY = cam3Y;
            // 水平：法向投影滑动
            {
                double nU, nV;
                if (!collideNormal(cam3U + moveU, cam3V + moveV, cam3Y, nU, nV))
                {
                    newU += moveU; newV += moveV;
                }
                else
                {
                    // 投影到面：去掉法向分量
                    double dot = moveU * nU + moveV * nV;
                    if (dot > 0) { moveU -= dot * nU; moveV -= dot * nV; }
                    if (!mapCollide(cam3U + moveU, cam3V + moveV, cam3Y))
                    {
                        newU += moveU; newV += moveV;
                    }
                }
            }
            // 垂直
            if (std::abs(moveY) > 1e-12)
            {
                if (!mapCollide(newU, newV, cam3Y + moveY))
                {
                    newY += moveY; if (!flyMode) onGround = false;
                }
                else if (!flyMode && moveY < 0) onGround = true;
            }
            else if (!flyMode && verticalVel <= 0)
            {
                onGround = mapCollide(newU, newV, cam3Y - 0.001);
            }

            cam3U = newU; cam3V = newV; cam3Y = newY;
            if (onGround && verticalVel < 0) verticalVel = 0;

            // ---- 脚步声（仅实际位移时播放） ----
            bool actuallyMoved = (std::abs(cam3U - oldU) > 1e-9 ||
                std::abs(cam3V - oldV) > 1e-9 ||
                std::abs(cam3Y - oldY) > 1e-9);
            if (isMoving && actuallyMoved && !flyMode && onGround)
            {
                double sinceStep = (double) (clock() - lastStepTime) / CLOCKS_PER_SEC;
                if (sinceStep >= 0.25)
                {
                    playSFX(stepPath[rand() % 6]);
                    lastStepTime = clock();
                }
            }

            // ---- 同步 4D 摄像机位置 ----
            {
                Plane2D pl = map3D.plane;
                const Vec4 &ref = map3D.camRef4D;
                Vec4 new4D = ref;
                new4D.x += cam3U * pl.p.x + cam3V * pl.q.x;
                new4D.z += cam3U * pl.p.z + cam3V * pl.q.z;
                new4D.w += cam3U * pl.p.w + cam3V * pl.q.w;
                new4D.y = ref.y + cam3Y;
                Vec4 delta = vec4Sub(new4D, camera.getPos());
                if (vec4LengthSq(delta) > 1e-12) camera.move(delta);
            }
        }

        // ---- 3D 视角旋转（鼠标） ----
        {
            auto [dx, dy] = input.getMouseDelta();
            if (dx != 0) { cam3Yaw += dx * MOUSE_SENSITIVITY; camera.rotateAroundUp(dx * MOUSE_SENSITIVITY); }
            if (dy != 0) { cam3Pitch -= dy * MOUSE_SENSITIVITY; camera.addPitch(-dy * MOUSE_SENSITIVITY); }
            cam3Pitch = camera.getPitch();  // 同步（摄像机内部有钳制）
        }

        // ---- 热键栏选择 ----
        if (input.isPressed(Key::Num1)) selectedSlot = 0;
        if (input.isPressed(Key::Num2)) selectedSlot = 1;
        if (input.isPressed(Key::Num3)) selectedSlot = 2;
        if (input.isPressed(Key::Num4)) selectedSlot = 3;
        if (input.isPressed(Key::Num5)) selectedSlot = 4;
        if (input.isPressed(Key::Num6)) selectedSlot = 5;
        if (input.isPressed(Key::Num7)) selectedSlot = 6;
        if (input.isPressed(Key::Num8)) selectedSlot = 7;
        if (input.isPressed(Key::Num9)) selectedSlot = 8;

        // ---- F3：切换 HUD 显示 ----
        if (input.isPressed(Key::F3)) renderer.toggleHUD();

        // ---- 滚轮：重建地图（累积旋转，超过阈值才重建，避免平滑衰减期间每帧重建卡顿） ----
        {
            double inputDesire = 0.0;
            int wheel = input.getMouseWheel();
            if (wheel != 0) inputDesire += (wheel / (double) WHEEL_DELTA) * SLICE_STEP;
            sliceVelocity += (inputDesire - sliceVelocity) * SLICE_SMOOTH;

            if (std::abs(sliceVelocity) > 1e-10)
            {
                camera.rotateSlice(sliceVelocity);
                pendingSliceRotation += sliceVelocity;

                // 只在累积旋转超过阈值时重建地图（~0.7°/次），大幅减少 map3D 重建频率
                constexpr double MAP_REBUILD_THRESHOLD = 0.012;
                if (std::abs(pendingSliceRotation) >= MAP_REBUILD_THRESHOLD)
                {
                    pendingSliceRotation = 0.0;
                    // 重建 3D 地图
                    map3D = generateMap3D(world, camera, 0.5,
                        [](int bx, int by, int bz, int bw) -> COLORREF
                    {
                        return blockColor(bx, by, bz, bw);
                    });
                    // 重新计算 3D 位置
                    Plane2D pl = camera.getViewPlane();
                    Vec3 cXZW = Vec3::fromVec4(camera.getPos());
                    cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p);
                    cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q);
                    cam3Y = camera.getPos().y - map3D.camRef4D.y;
                }
            }
            else
            {
                // 旋转完全停止，若仍有未应用累积则做最后一次重建以确保碰撞精度
                if (std::abs(pendingSliceRotation) > 1e-10)
                {
                    pendingSliceRotation = 0.0;
                    map3D = generateMap3D(world, camera, 0.5,
                        [](int bx, int by, int bz, int bw) -> COLORREF
                    {
                        return blockColor(bx, by, bz, bw);
                    });
                    Plane2D pl = camera.getViewPlane();
                    Vec3 cXZW = Vec3::fromVec4(camera.getPos());
                    cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p);
                    cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q);
                    cam3Y = camera.getPos().y - map3D.camRef4D.y;
                }
                sliceVelocity = 0;
            }
        }


        // ---- 视线射线（每帧计算，用于方块边框 + 鼠标交互） ----
        IVec4 targetedBlock, targetedPrev;
        bool hasTarget = false;
        {
            Plane2D pl = map3D.plane;
            double cp = camera.getPitch();
            double du = std::sin(cam3Yaw) * std::cos(cp);
            double dv = std::cos(cam3Yaw) * std::cos(cp);
            double dys = std::sin(cp);
            Vec4 rayDir(du * pl.p.x + dv * pl.q.x, dys, du * pl.p.z + dv * pl.q.z, du * pl.p.w + dv * pl.q.w);
            double rLen = vec4Length(rayDir); if (rLen > 1e-9) rayDir = vec4Scale(rayDir, 1.0 / rLen);
            auto raycast3D = [&](IVec4 &hit, IVec4 &prev) -> bool
            {
                Vec4 pos = camera.getPos();
                IVec4 pg((int) std::round(pos.x), (int) std::round(pos.y), (int) std::round(pos.z), (int) std::round(pos.w));
                for (double t = 0.2; t <= 6.0; t += 0.2)
                {
                    Vec4 s = vec4Add(pos, vec4Scale(rayDir, t));
                    IVec4 g((int) std::round(s.x), (int) std::round(s.y), (int) std::round(s.z), (int) std::round(s.w));
                    if (g.x == pg.x && g.y == pg.y && g.z == pg.z && g.w == pg.w) continue;
                    if (world.get(g)) { hit = g; prev = pg; return true; } pg = g;
                }
                return false;
            };
            hasTarget = raycast3D(targetedBlock, targetedPrev);

            // 鼠标按键行为（带冷却防止连点）
            IVec4 hitPos, prevPos;
            bool mapChanged = false;
            IVec4 changedPos; int changedType = 0;  // 增量更新参数
            if (interactCooldown > 0) --interactCooldown;
            else
            {
                if (input.getMouseClick(0))
                {
                    if (raycast3D(hitPos, prevPos))
                    {
                        world.set(hitPos, 0);
                        mapChanged = true; changedPos = hitPos; changedType = 0;
                        interactCooldown = 8;
                        playSFX(digPath[rand() % 4]);
                    }
                }
                if (input.getMouseClick(1))
                {
                    // 右键工作台 → 打开工作台界面
                    if (hasTarget && world.get(targetedBlock) == BLOCK_CRAFTING_TABLE)
                    {
                        playSFX(clickPath);
                        inventory.setCraftMode(Inventory::CM_CraftingTable3x3);
                        state = GameState::CraftingTable;
                        renderer.captureBackground();
                        renderer.applyGaussianBlur();
                        input.showMouseCursor(true);
                        continue;
                    }

                    if (raycast3D(hitPos, prevPos))
                    {
                        int placeType = inventory.hotbarBlockType(selectedSlot);
                        Vec4 pp = camera.getPos();
                        IVec4 playerFeet((int) std::round(pp.x), (int) std::round(pp.y - CYLINDER_H + 0.5),
                            (int) std::round(pp.z), (int) std::round(pp.w));
                        IVec4 playerHead((int) std::round(pp.x), (int) std::round(pp.y),
                            (int) std::round(pp.z), (int) std::round(pp.w));
                        bool wouldCollide = (prevPos == playerFeet || prevPos == playerHead);
                        for (int dy = playerFeet.y + 1; dy < playerHead.y && !wouldCollide; ++dy)
                            if (prevPos.x == playerFeet.x && prevPos.y == dy &&
                                prevPos.z == playerFeet.z && prevPos.w == playerFeet.w)
                                wouldCollide = true;

                        if (!wouldCollide)
                        {
                            world.set(prevPos, placeType);
                            mapChanged = true; changedPos = prevPos; changedType = placeType;
                            interactCooldown = 15;
                            playSFX(digPath[rand() % 4]);
                        }
                    }
                }
            }
            if (mapChanged)
            {
                map3D_updateBlock(map3D, changedPos, changedType, camera, 0.5,
                    [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
            }
        }

        // 绘制
        {
            cleardevice();
            setbkcolor(RGB(10, 10, 30));

            // 设置目标方块（用于线框绘制）
            if (hasTarget)
                renderer.setTargetBlock(targetedBlock);
            else
                renderer.clearTargetBlock();

            renderer.renderWorld(world, camera);
            // 快捷栏类型从 Inventory 读取
            int hbTypes[9];
            for (int i = 0; i < 9; ++i) hbTypes[i] = inventory.getSlot(i).blockType;
            renderer.drawHotbar(selectedSlot, hbTypes);
            renderer.drawCrosshair();
            renderer.drawHUD(camera);

            FlushBatchDraw();
        }
    }

    EndBatchDraw();

    // 停止并关闭 BGM / SFX
    mciSendString(L"stop bgm", NULL, 0, NULL);
    mciSendString(L"close bgm", NULL, 0, NULL);
    mciSendString(L"close sfx", NULL, 0, NULL);

    closegraph();
}
