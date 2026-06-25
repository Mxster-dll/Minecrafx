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

#include "core/linalg.h"
#include "world/world.h"
#include "world/camera.h"
#include "render/renderer.h"
#include "core/constant.h"
#include "input/input_handler.h"
#include "game/inventory.h"
#include "game/crafting.h"
#include "game/furnace.h"
#include "game/block_data.h"

 // ---- 游戏状态 ----
enum class GameState
{
    Login,         // 登录页（选择模式）
    Gameplay,      // 正常游戏
    Inventory,     // 背包界面（E 键打开，2×2 合成）
    CraftingTable, // 工作台界面（右键工作台打开，3×3 合成）
    Furnace,       // 熔炉界面（右键熔炉打开）
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

    wchar_t digPath[4][MAX_PATH], stepPath[6][MAX_PATH], clickPath[MAX_PATH], popPath[MAX_PATH];
    for (int i = 0; i < 4; ++i)
        swprintf(digPath[i], MAX_PATH, L"%lsdig%d.mp3", sfxDir, i + 1);
    for (int i = 0; i < 6; ++i)
        swprintf(stepPath[i], MAX_PATH, L"%lsstep%d.mp3", sfxDir, i + 1);
    swprintf(clickPath, MAX_PATH, L"%lsclick_stereo.mp3", sfxDir);
    swprintf(popPath, MAX_PATH, L"%lspop.mp3", sfxDir);

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
    renderer.loadDestroyStages();
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
    bool isCreative = false;   // 创造模式 / 生存模式
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
    // ---- 挖掘状态（生存模式） ----
    IVec4 miningTarget;
    double miningProgress = 0.0;
    double miningTotalTime = 0.0;
    double miningCooldown = 0.0;  // 完成后 0.05s 延迟
    clock_t lastStepTime = 0;  // 上次脚步声时间
    clock_t lastDigTime = 0;   // 上次挖掘声音时间

    // ---- 3D 地图 + 3D 摄像机（选模式后初始化） ----
    Map3D map3D;
    double cam3U = 0, cam3V = 0, cam3Y = 0;
    double cam3Yaw = 0, cam3Pitch = 0;

    SetWindowText(hwnd, L"Minecrafx");

    // ---- 预加载 GUI 图片 ----
    IMAGE imgInventory, imgCraftingTable, imgSmoker, imgButton;
    IMAGE imgBurnProgress, imgLitProgress;
    IMAGE imgIsles;  // 登录页背景
    loadimage(&imgIsles, L"../assert/start.png");
    // 加载原图 → 3x 最邻近放大（无后期处理）
    {
        IMAGE imgInvNative, imgCTNative, imgSmokerNative;
        loadimage(&imgInvNative, L"../assert/gui/widget/inventory.png");
        loadimage(&imgCTNative, L"../assert/gui/widget/crafting_table.png");
        loadimage(&imgSmokerNative, L"../assert/gui/widget/smoker.png");
        nearestScale(imgInventory, imgInvNative, 3);
        nearestScale(imgCraftingTable, imgCTNative, 3);
        nearestScale(imgSmoker, imgSmokerNative, 3);
    }
    loadimage(&imgBurnProgress, L"../assert/gui/widget/burn_progress.png");
    loadimage(&imgLitProgress, L"../assert/gui/widget/lit_progress.png");
    // 按钮贴图：594×54，无需缩放
    constexpr int BTN_W = 594, BTN_H = 54;
    loadimage(&imgButton, L"../assert/gui/widget/button.png");

    // ---- 游戏状态 ----
    GameState state = GameState::Login;
    Inventory inventory;
    CraftingManager craftMgr;  // 合成配方
    FurnaceManager::State furnaceState;  // 熔炉状态

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

        // ── 熔炉后台更新（所有状态下持续运行） ──
        {
            constexpr int FURN_IN = Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS;
            constexpr int FURN_FU = FURN_IN + 1;
            constexpr int FURN_OU = FURN_IN + 2;
            auto &inSlot = inventory.getSlot(FURN_IN);
            auto &fuelSlot = inventory.getSlot(FURN_FU);
            auto &outSlot = inventory.getSlot(FURN_OU);
            furnaceState.inputType = inSlot.blockType;
            furnaceState.inputCount = inSlot.count;
            furnaceState.fuelType = fuelSlot.blockType;
            furnaceState.fuelCount = fuelSlot.count;
            furnaceState.outputType = outSlot.blockType;
            furnaceState.outputCount = outSlot.count;

            FurnaceManager::update(furnaceState, dt);

            inSlot.blockType = furnaceState.inputType;
            inSlot.count = furnaceState.inputCount;
            fuelSlot.blockType = furnaceState.fuelType;
            fuelSlot.count = furnaceState.fuelCount;
            outSlot.blockType = furnaceState.outputType;
            outSlot.count = furnaceState.outputCount;
            renderer.setFurnaceActive(furnaceState.active);
        }

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
                isCreative = false;
                // 清空背包，首格放铁镐
                for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
                    inventory.getSlot(i) = { BLOCK_AIR, 0 };
                inventory.getSlot(0) = { BLOCK_IRON_PICKAXE, 1 };
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
                                else if (y < ht - 3) type = BLOCK_STONE;  // 深层石头
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
                    int oreType = BLOCK_COAL_ORE;
                    int r = rand() % 100;
                    if (r < 50) oreType = BLOCK_COAL_ORE;
                    else if (r < 80) oreType = BLOCK_IRON_ORE;
                    else if (r < 95) oreType = BLOCK_GOLD_ORE;
                    else oreType = BLOCK_DIAMOND_ORE;
                    int clusterSize = 2 + rand() % 4;
                    for (int j = 0; j < clusterSize; ++j)
                    {
                        int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1, dz = (rand() % 3) - 1, dw = (rand() % 3) - 1;
                        int nx = ox + dx, ny = oy + dy, nz = oz + dz, nw = ow + dw;
                        if (nx >= 0 && nx < MX && nz >= 0 && nz < MZ && nw >= 0 && nw < MW && ny >= 0 && ny < terrainHeight(nx, nz, nw) - 1)
                            if (world.get(IVec4(nx, ny, nz, nw)) == BLOCK_DIRT || world.get(IVec4(nx, ny, nz, nw)) == BLOCK_STONE)
                                world.set(IVec4(nx, ny, nz, nw), oreType);
                    }
                }
                // 额外煤矿生成（深层石头中）
                for (int i = 0; i < 600; ++i)
                {
                    int ox = rand() % MX, oz = rand() % MZ, ow = rand() % MW;
                    int oy = rand() % 3;  // 深层 0~2
                    int clusterSize = 3 + rand() % 5;
                    for (int j = 0; j < clusterSize; ++j)
                    {
                        int dx = (rand() % 3) - 1, dy = (rand() % 3) - 1, dz = (rand() % 3) - 1, dw = (rand() % 3) - 1;
                        int nx = ox + dx, ny = oy + dy, nz = oz + dz, nw = ow + dw;
                        if (nx >= 0 && nx < MX && nz >= 0 && nz < MZ && nw >= 0 && nw < MW && ny >= 0 && ny < terrainHeight(nx, nz, nw) - 1)
                            if (world.get(IVec4(nx, ny, nz, nw)) == BLOCK_STONE)
                                world.set(IVec4(nx, ny, nz, nw), BLOCK_COAL_ORE);
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
                isCreative = true;
                // 填充背包：排除工具、武器、盔甲
                {
                    auto isExcluded = [](int id) -> bool
                    {
                        // 木/石/铁/金/钻石 工具 (22-26, 27-31, 32-36, 41-45, 50-54)
                        if (id >= 22 && id <= 54) return true;
                        // 盔甲 (37-40, 46-49, 55-58)
                        if (id >= 37 && id <= 40) return true;
                        if (id >= 46 && id <= 49) return true;
                        if (id >= 55 && id <= 58) return true;
                        return false;
                    };
                    int slot = 0;
                    for (int id = 1; id < MAX_BLOCK_TYPE && slot < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++id)
                    {
                        if (!isExcluded(id))
                            inventory.getSlot(slot++) = { id, 1 };
                    }
                }
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

        // ================================================================
        // 熔炉界面
        // ================================================================
        if (state == GameState::Furnace)
        {
            if (input.isPressed(Key::Esc) || input.isPressed(Key::E))
            {
                if (inventory.isDragging()) inventory.cancelDrag();
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            int smDispW = imgSmoker.getwidth();
            int smDispH = imgSmoker.getheight();
            int smDispX = (SCREEN_WIDTH - smDispW) / 2;
            int smDispY = (SCREEN_HEIGHT - smDispH) / 2;
            int smNativeW = smDispW / 3;
            int smNativeH = smDispH / 3;

            POINT mp = input.getMouseScreenPos();
            bool mouseDown = input.getMouseClick(0);
            bool mouseUp = input.getMouseRelease(0);
            bool rightClick = input.getMouseClick(1);

            // 熔炉槽位索引
            constexpr int FURN_INPUT = Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS;     // 50
            constexpr int FURN_FUEL = FURN_INPUT + 1;  // 51
            constexpr int FURN_OUTPUT = FURN_INPUT + 2;  // 52

            // 槽位命中检测
            auto hitFurnaceSlot = [&](int nx, int ny) -> int
            {
                if (nx >= 56 && nx < 72 && ny >= 17 && ny < 33) return FURN_INPUT;
                if (nx >= 56 && nx < 72 && ny >= 53 && ny < 69) return FURN_FUEL;
                if (nx >= 116 && nx < 132 && ny >= 35 && ny < 51) return FURN_OUTPUT;
                return -1;
            };

            // 背包/快捷栏沿用 inventory.hitTest
            int hoveredSlot = -1;
            // 先检测熔炉槽位
            {
                double sx = (double) smDispW / smNativeW;
                double sy = (double) smDispH / smNativeH;
                double nx = (mp.x - smDispX) / sx;
                double ny = (mp.y - smDispY) / sy;
                hoveredSlot = hitFurnaceSlot((int) nx, (int) ny);
            }
            if (hoveredSlot < 0)
                hoveredSlot = inventory.hitTest(mp.x, mp.y,
                    smDispX, smDispY, smDispW, smDispH,
                    smNativeW, smNativeH);

            // 左键交互（与背包/工作台一致）
            static int dragSrcSlot = -1;
            static bool didAutoPickup = false;

            // 熔炉格放入验证
            auto canPlaceInFurnace = [&](int slotIdx, int itemType) -> bool
            {
                if (slotIdx == FURN_OUTPUT) return false;  // 输出格不可放入
                if (slotIdx == FURN_FUEL) return FurnaceManager::fuelValue(itemType) > 0.0;
                if (slotIdx == FURN_INPUT) return true;    // 输入格接受任何物品
                return true;
            };

            if (mouseDown && hoveredSlot >= 0)
            {
                dragSrcSlot = hoveredSlot;
                didAutoPickup = false;
                if (!inventory.isDragging())
                    didAutoPickup = inventory.pickup(hoveredSlot, -1);
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
                // 点击输出格有产物 → 拿取
                if (hoveredSlot == FURN_OUTPUT && inventory.getSlot(FURN_OUTPUT).blockType != BLOCK_AIR)
                {
                    inventory.pickup(FURN_OUTPUT, -1);
                    inventory.getSlot(FURN_OUTPUT) = { BLOCK_AIR, 0 };
                }
                else
                    inventory.pickup(hoveredSlot, -1);
            }
            if (rightClick && hoveredSlot >= 0)
            {
                if (inventory.isDragging())
                {
                    if (canPlaceInFurnace(hoveredSlot, inventory.dragBlockType()))
                        inventory.placeOneInto(hoveredSlot);
                }
                else
                    inventory.pickup(hoveredSlot, 1);
            }

            // 渲染
            cleardevice();
            setbkcolor(RGB(10, 10, 30));
            renderer.drawBackground();
            renderer.drawImageCentered(&imgSmoker);

            // 绘制背包+快捷栏+盔甲槽（跳过熔炉三格，它们单独绘制）
            for (int i = 0; i < Inventory::ARMOR_BASE + Inventory::ARMOR_SLOTS; ++i)
            {
                const auto &slot = inventory.getSlot(i);
                if (slot.blockType == BLOCK_AIR || slot.count <= 0) continue;
                int sx, sy, sw, sh;
                inventory.slotScreenRect(i,
                    smDispX, smDispY, smDispW, smDispH,
                    smNativeW, smNativeH, sx, sy, sw, sh);
                renderer.drawBlockIcon(sx, sy, sh, slot.blockType, slot.count);
            }
            // 绘制熔炉三格物品
            auto drawFurnSlot = [&](int nativeX, int nativeY, int slotIdx)
            {
                const auto &slot = inventory.getSlot(slotIdx);
                if (slot.blockType == BLOCK_AIR || slot.count <= 0) return;
                double sx = (double) smDispW / smNativeW;
                double sy = (double) smDispH / smNativeH;
                int sz = (int) (Inventory::SLOT_SIZE * sx);
                int dx = smDispX + (int) (nativeX * sx);
                int dy = smDispY + (int) (nativeY * sy);
                renderer.drawBlockIcon(dx, dy, sz, slot.blockType, slot.count);
            };
            drawFurnSlot(56, 17, FURN_INPUT);
            drawFurnSlot(56, 53, FURN_FUEL);
            drawFurnSlot(116, 35, FURN_OUTPUT);

            // 燃烧进度条
            if (furnaceState.active && imgBurnProgress.getwidth() > 0)
            {
                int progW = imgBurnProgress.getwidth();
                int progH = imgBurnProgress.getheight();
                int cols = (int) (furnaceState.burnProgress * progW);
                if (cols > progW) cols = progW;
                if (cols > 0)
                {
                    double sx = (double) smDispW / smNativeW;
                    double sy = (double) smDispH / smNativeH;
                    int barX = smDispX + (int) (80 * sx);
                    int barY = smDispY + (int) (34 * sy);
                    int barW = (int) (22 * sx);
                    int barH = (int) (16 * sy);
                    DWORD *pBuf = GetImageBuffer(&imgBurnProgress);
                    int pSrcW = imgBurnProgress.getwidth();
                    if (pBuf)
                    {
                        int dispCols = cols * barW / progW;
                        if (dispCols < 1) dispCols = 1;
                        for (int dy = 0; dy < barH && dy < progH * barH / progH; ++dy)
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

            // 燃料剩余指示（lit_progress.png: 14×14，顶行=满，底面=空）
            if (furnaceState.active && imgLitProgress.getwidth() > 0)
            {
                // 计算燃料剩余比例（相对于当前燃料的热值）
                // burnTimeRemain / fuelCapacity = 剩余可烧数 / 总容量
                double fuelRatio = (furnaceState.fuelCapacity > 0.0)
                    ? furnaceState.burnTimeRemain / furnaceState.fuelCapacity : 0.0;
                if (fuelRatio < 0.0) fuelRatio = 0.0;
                if (fuelRatio > 1.0) fuelRatio = 1.0;

                int visibleRows = (int) (fuelRatio * 14.0 + 0.5);
                if (visibleRows < 0) visibleRows = 0;
                if (visibleRows > 14) visibleRows = 14;

                if (visibleRows > 0)
                {
                    double sx = (double) smDispW / smNativeW;
                    double sy = (double) smDispH / smNativeH;
                    int fireX = smDispX + (int) (56 * sx);
                    int fireY = smDispY + (int) (36 * sy);
                    int fireW = (int) (14 * sx);
                    int fireH = (int) (14 * sy);
                    DWORD *pBuf = GetImageBuffer(&imgLitProgress);
                    int pSrcW = imgLitProgress.getwidth();
                    int pSrcH = imgLitProgress.getheight();
                    if (pBuf && pSrcW > 0 && pSrcH > 0)
                    {
                        // 从上往下消失：显示底部 visibleRows/14 比例
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

            // 拖拽中物品
            if (inventory.isDragging())
            {
                double sx = (double) smDispW / smNativeW;
                int sz = (int) (16 * sx);
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

            // 鼠标按键行为
            IVec4 hitPos, prevPos;
            bool mapChanged = false;
            IVec4 changedPos; int changedType = 0;  // 增量更新参数

            // ── 左键：挖掘（创造模式即时 / 生存模式持握蓄力） ──
            if (isCreative)
            {
                // 创造模式：点击即摧毁，无进度，不掉落
                if (interactCooldown > 0) --interactCooldown;
                else if (input.getMouseClick(0) && hasTarget)
                {
                    if (raycast3D(hitPos, prevPos))
                    {
                        world.set(hitPos, 0);
                        playSFX(digPath[rand() % 4]);  // 创造模式只响一声
                        mapChanged = true; changedPos = hitPos; changedType = 0;
                        interactCooldown = 4;
                    }
                }
            }
            else
            {
                // 生存模式：持握挖掘，有进度
                if (input.isMouseButtonDown(0) && hasTarget)
                {
                    int curBlock = world.get(targetedBlock);
                    if (curBlock <= 0) { miningProgress = 0.0; }
                    else if (targetedBlock == miningTarget)
                    {
                        // 继续挖掘同一方块
                        int toolType = inventory.hotbarBlockType(selectedSlot);
                        miningTotalTime = getMiningTime(curBlock, toolType);
                        miningProgress += dt;
                        // 挖掘音效（间隔 0.25s，与走路一致）
                        {
                            double sinceDig = (double) (clock() - lastDigTime) / CLOCKS_PER_SEC;
                            if (sinceDig >= 0.25)
                            {
                                playSFX(digPath[rand() % 4]);
                                lastDigTime = clock();
                            }
                        }
                        if (miningProgress >= miningTotalTime)
                        {
                            miningProgress = miningTotalTime;  // 到达 100%
                            if (miningCooldown <= 0.0)
                                miningCooldown = 0.05;  // 进入 0.05s 延迟
                        }
                    }
                    else
                    {
                        // 目标切换（冷却期间不重置）
                        if (miningCooldown <= 0.0)
                        {
                            miningTarget = targetedBlock;
                            miningProgress = 0.0;
                            miningCooldown = 0.0;
                        }
                    }
                }
                else
                {
                    // 未按住左键或没有目标（冷却期间不重置）
                    if (miningCooldown <= 0.0)
                    {
                        miningProgress = 0.0;
                        miningTarget = IVec4();
                        miningCooldown = 0.0;
                    }
                }

                // 挖掘冷却倒计时
                if (miningCooldown > 0.0)
                {
                    miningCooldown -= dt;
                    if (miningCooldown <= 0.0)
                    {
                        int destroyedType = world.get(miningTarget);
                        world.set(miningTarget, 0);
                        mapChanged = true; changedPos = miningTarget; changedType = 0;
                        bool harvested = canHarvest(destroyedType, inventory.hotbarBlockType(selectedSlot));
                        // 音效（仅当工具等级足够时播放）
                        if (harvested)
                            playSFX(popPath);
                        // 掉落（仅当工具等级足够，且树叶不掉落自身）
                        if (harvested && destroyedType != BLOCK_LEAVES)
                        {
                            int dropType = destroyedType;
                            int dropCount = 1;
                            // 草方块 → 泥土
                            if (destroyedType == BLOCK_GRASS)
                                dropType = BLOCK_DIRT;
                            // 石头 → 圆石
                            else if (destroyedType == BLOCK_STONE)
                                dropType = BLOCK_COBBLESTONE;
                            // 煤矿 → 煤炭 2~4 个
                            else if (destroyedType == BLOCK_COAL_ORE)
                            {
                                dropType = BLOCK_COAL; dropCount = 2 + (rand() % 3);
                            }

                            auto addItem = [&](int type, int cnt)
                            {
                                for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
                                {
                                    auto &slot = inventory.getSlot(i);
                                    if (slot.blockType == type)
                                    {
                                        slot.count += cnt; return;
                                    }
                                }
                                for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
                                {
                                    auto &slot = inventory.getSlot(i);
                                    if (slot.blockType == BLOCK_AIR)
                                    {
                                        slot.blockType = type; slot.count = cnt; return;
                                    }
                                }
                            };
                            addItem(dropType, dropCount);
                        }
                        // 树叶额外掉落苹果（50% 概率）
                        if (destroyedType == BLOCK_LEAVES && (rand() % 100) < 50)
                        {
                            bool placed = false;
                            for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
                            {
                                auto &slot = inventory.getSlot(i);
                                if (slot.blockType == BLOCK_APPLE)
                                {
                                    slot.count++; placed = true; break;
                                }
                            }
                            if (!placed)
                            {
                                for (int i = 0; i < Inventory::HOTBAR_SLOTS + Inventory::BACKPACK_SLOTS; ++i)
                                {
                                    auto &slot = inventory.getSlot(i);
                                    if (slot.blockType == BLOCK_AIR)
                                    {
                                        slot.blockType = BLOCK_APPLE; slot.count = 1; break;
                                    }
                                }
                            }
                        }
                        miningProgress = 0.0;
                        miningTotalTime = 0.0;
                        miningCooldown = 0.0;
                        miningTarget = IVec4();
                    }
                }
            }

            // ── 右键：放置方块 / 打开工作台 ──
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
                // 右键熔炉 → 打开熔炉界面
                if (hasTarget && world.get(targetedBlock) == BLOCK_FURNACE)
                {
                    playSFX(clickPath);
                    state = GameState::Furnace;
                    renderer.captureBackground();
                    renderer.applyGaussianBlur();
                    input.showMouseCursor(true);
                    continue;
                }

                if (raycast3D(hitPos, prevPos))
                {
                    int placeType = inventory.hotbarBlockType(selectedSlot);
                    if (placeType <= 0) { /* 手持空物品，不放置 */ }
                    else
                    {
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
                            // 生存模式消耗 1 个物品
                            if (!isCreative)
                            {
                                auto &slot = inventory.getSlot(selectedSlot);
                                if (slot.count > 0) slot.count--;
                                if (slot.count <= 0) slot.blockType = BLOCK_AIR;
                            }
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

            // 挖掘目标（必须在 renderWorld 之前设置，管线内逐面叠加 destroy_stage）
            if (!isCreative && miningProgress > 0.0 && miningTotalTime > 0.0 && world.get(miningTarget) > 0)
                renderer.setMiningTarget(miningTarget, miningProgress / miningTotalTime);
            else
                renderer.clearMiningTarget();

            renderer.renderWorld(world, camera);
            // 快捷栏类型从 Inventory 读取
            int hbTypes[9], hbCounts[9];
            for (int i = 0; i < 9; ++i)
            {
                hbTypes[i] = inventory.getSlot(i).blockType;
                hbCounts[i] = inventory.getSlot(i).count;
            }
            renderer.drawHotbar(selectedSlot, hbTypes, hbCounts);
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
