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
#include "world/world_gen.h"
#include "game/game_screens.h"

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

    ImmAssociateContext(hwnd, NULL);

    {

        wchar_t exePath[MAX_PATH], mp3Path[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t *lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *lastSlash = L'\0';

        wcscpy(mp3Path, exePath);
        wcscat(mp3Path, L"\\..\\assert\\sounds\\方块的世界.mp3");

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

    renderer.loadBlockTextures();
    renderer.loadDestroyStages();
    renderer.loadHotbar();
    renderer.loadInventoryIcons();

    constexpr int MX = 48, MZ = 48, MW = 24;

    bool flyMode = true;
    bool isCreative = false;
    double verticalVel = 0.0;
    bool onGround = false;
    clock_t lastSpacePress = 0;
    constexpr double DOUBLE_TAP_MS = 350;
    constexpr double GRAVITY = 25.0;
    constexpr double JUMP_VEL = 8.5;
    double sliceVelocity = 0.0;
    double pendingSliceRotation = 0.0;
    clock_t lastFrame = clock();
    int interactCooldown = 0;
    int selectedSlot = 0;

    IVec4 miningTarget;
    double miningProgress = 0.0;
    double miningTotalTime = 0.0;
    double miningCooldown = 0.0;
    clock_t lastStepTime = 0;
    clock_t lastDigTime = 0;

    Map3D map3D;
    double cam3U = 0, cam3V = 0, cam3Y = 0;
    double cam3Yaw = 0, cam3Pitch = 0;

    SetWindowText(hwnd, L"Minecrafx");

    IMAGE imgInventory, imgCraftingTable, imgSmoker, imgButton;
    IMAGE imgBurnProgress, imgLitProgress;
    IMAGE imgIsles;
    loadimage(&imgIsles, L"../assert/start.png");

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

    constexpr int BTN_W = 594, BTN_H = 54;
    loadimage(&imgButton, L"../assert/gui/widget/button.png");

    GameState state = GameState::Login;
    Inventory inventory;
    CraftingManager craftMgr;
    FurnaceManager::State furnaceState;

    InputHandler input(hwnd);
    input.showMouseCursor(true);
    bool loginBgReady = false;
    while (!input.isQuitRequested())
    {
        input.update();

        clock_t nowFrame = clock();
        double dt = static_cast<double>(nowFrame - lastFrame) / CLOCKS_PER_SEC;
        lastFrame = nowFrame;
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0.0) dt = 0.001;

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

        if (state == GameState::Login)
        {

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

            if (click1)
            {
                playSFX(clickPath);
                isCreative = false;
                initSurvivalInventory(inventory);
                generateSurvivalWorld(world, MX, MZ, MW);

                map3D = generateMap3D(world, camera, 0.5, [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                { Plane2D pl = camera.getViewPlane(); Vec3 cXZW = Vec3::fromVec4(camera.getPos()); cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p); cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q); cam3Y = camera.getPos().y - map3D.camRef4D.y; }
                pendingSliceRotation = 0.0;
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            if (click2)
            {
                playSFX(clickPath);
                isCreative = true;
                initCreativeInventory(inventory);
                constexpr int CF_X = 16, CF_Z = 16, CF_W = 16;
                generateCreativeWorld(world, CF_X, CF_Z, CF_W);
                map3D = generateMap3D(world, camera, 0.5, [](int bx, int by, int bz, int bw)->COLORREF { return blockColor(bx, by, bz, bw); });
                { Plane2D pl = camera.getViewPlane(); Vec3 cXZW = Vec3::fromVec4(camera.getPos()); cam3U = vec3Dot(cXZW, pl.p) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.p); cam3V = vec3Dot(cXZW, pl.q) - vec3Dot(Vec3::fromVec4(map3D.camRef4D), pl.q); cam3Y = camera.getPos().y - map3D.camRef4D.y; }
                pendingSliceRotation = 0.0;
                state = GameState::Gameplay;
                input.showMouseCursor(false);
                input.getMouseDelta();
                continue;
            }

            renderer.drawButton(btn1X, btn1Y, LOGIN_BTN_W, LOGIN_BTN_H,
                &imgButton, &imgButton, &imgButton, L"生存模式", hover1, hover1 && input.isMouseButtonDown(0));
            renderer.drawButton(btn2X, btn2Y, LOGIN_BTN_W, LOGIN_BTN_H,
                &imgButton, &imgButton, &imgButton, L"创造模式", hover2, hover2 && input.isMouseButtonDown(0));

            renderer.flushToScreen();
            FlushBatchDraw();
            continue;
        }

        if (state == GameState::Inventory)
        {
            if (updateCraftingScreen(state, input, inventory, craftMgr, renderer, imgInventory, Inventory::CM_Inventory2x2, playSFX, clickPath)) continue;
        }
        if (state == GameState::CraftingTable)
        {
            if (updateCraftingScreen(state, input, inventory, craftMgr, renderer, imgCraftingTable, Inventory::CM_CraftingTable3x3, playSFX, clickPath)) continue;
        }
        if (state == GameState::Furnace)
        {
            if (updateFurnaceScreen(state, input, inventory, furnaceState, renderer, imgSmoker, imgBurnProgress, imgLitProgress)) continue;
        }
        if (state == GameState::Paused)
        {
            if (updatePauseScreen(state, input, renderer, world, camera, map3D, cam3U, cam3V, cam3Y, cam3Yaw, cam3Pitch, pendingSliceRotation, selectedSlot, loginBgReady, imgButton, BTN_W, BTN_H, playSFX, clickPath)) continue;
        }

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

        {
            Plane2D pl = map3D.plane;
            const Vec4 &fwd = camera.getForward(), &rht = camera.getRight();
            double fU = fwd.x * pl.p.x + fwd.z * pl.p.z + fwd.w * pl.p.w;
            double fV = fwd.x * pl.q.x + fwd.z * pl.q.z + fwd.w * pl.q.w;
            double rU = rht.x * pl.p.x + rht.z * pl.p.z + rht.w * pl.p.w;
            double rV = rht.x * pl.q.x + rht.z * pl.q.z + rht.w * pl.q.w;

            cam3Yaw = std::atan2(fU, fV);

            double moveU = 0, moveV = 0;
            double speed = MOVE_SPEED * dt;
            bool isMoving = false;
            if (input.isKeyDown(Key::W)) { moveU += fU * speed; moveV += fV * speed; isMoving = true; }
            if (input.isKeyDown(Key::S)) { moveU -= fU * speed; moveV -= fV * speed; isMoving = true; }
            if (input.isKeyDown(Key::D)) { moveU += rU * speed; moveV += rV * speed; isMoving = true; }
            if (input.isKeyDown(Key::A)) { moveU -= rU * speed; moveV -= rV * speed; isMoving = true; }

            double oldU = cam3U, oldV = cam3V, oldY = cam3Y;

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

                    bool inside = true;
                    for (int i = 0; i < n && inside; ++i)
                    {
                        int j = (i + 1) % n;
                        double eu = pr.u[j] - pr.u[i], ev = pr.v[j] - pr.v[i];
                        if (eu * (v - pr.v[i]) - ev * (u - pr.u[i]) < -1e-9) inside = false;
                    }
                    if (inside) return true;

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

            {
                double nU, nV;
                if (!collideNormal(cam3U + moveU, cam3V + moveV, cam3Y, nU, nV))
                {
                    newU += moveU; newV += moveV;
                }
                else
                {

                    double dot = moveU * nU + moveV * nV;
                    if (dot > 0) { moveU -= dot * nU; moveV -= dot * nV; }
                    if (!mapCollide(cam3U + moveU, cam3V + moveV, cam3Y))
                    {
                        newU += moveU; newV += moveV;
                    }
                }
            }

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

        {
            auto [dx, dy] = input.getMouseDelta();
            if (dx != 0) { cam3Yaw += dx * MOUSE_SENSITIVITY; camera.rotateAroundUp(dx * MOUSE_SENSITIVITY); }
            if (dy != 0) { cam3Pitch -= dy * MOUSE_SENSITIVITY; camera.addPitch(-dy * MOUSE_SENSITIVITY); }
            cam3Pitch = camera.getPitch();
        }

        if (input.isPressed(Key::Num1)) selectedSlot = 0;
        if (input.isPressed(Key::Num2)) selectedSlot = 1;
        if (input.isPressed(Key::Num3)) selectedSlot = 2;
        if (input.isPressed(Key::Num4)) selectedSlot = 3;
        if (input.isPressed(Key::Num5)) selectedSlot = 4;
        if (input.isPressed(Key::Num6)) selectedSlot = 5;
        if (input.isPressed(Key::Num7)) selectedSlot = 6;
        if (input.isPressed(Key::Num8)) selectedSlot = 7;
        if (input.isPressed(Key::Num9)) selectedSlot = 8;

        if (input.isPressed(Key::F3)) renderer.toggleHUD();

        {
            double inputDesire = 0.0;
            int wheel = input.getMouseWheel();
            if (wheel != 0) inputDesire += (wheel / (double) WHEEL_DELTA) * SLICE_STEP;
            sliceVelocity += (inputDesire - sliceVelocity) * SLICE_SMOOTH;

            if (std::abs(sliceVelocity) > 1e-10)
            {
                camera.rotateSlice(sliceVelocity);
                pendingSliceRotation += sliceVelocity;

                constexpr double MAP_REBUILD_THRESHOLD = 0.012;
                if (std::abs(pendingSliceRotation) >= MAP_REBUILD_THRESHOLD)
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
            }
            else
            {

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

            IVec4 hitPos, prevPos;
            bool mapChanged = false;
            IVec4 changedPos; int changedType = 0;

            if (isCreative)
            {

                if (interactCooldown > 0) --interactCooldown;
                else if (input.getMouseClick(0) && hasTarget)
                {
                    if (raycast3D(hitPos, prevPos))
                    {
                        world.set(hitPos, 0);
                        playSFX(digPath[rand() % 4]);
                        mapChanged = true; changedPos = hitPos; changedType = 0;
                        interactCooldown = 4;
                    }
                }
            }
            else
            {

                if (input.isMouseButtonDown(0) && hasTarget)
                {
                    int curBlock = world.get(targetedBlock);
                    if (curBlock <= 0) { miningProgress = 0.0; }
                    else if (targetedBlock == miningTarget)
                    {

                        int toolType = inventory.hotbarBlockType(selectedSlot);
                        miningTotalTime = getMiningTime(curBlock, toolType);
                        miningProgress += dt;

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
                            miningProgress = miningTotalTime;
                            if (miningCooldown <= 0.0)
                                miningCooldown = 0.05;
                        }
                    }
                    else
                    {

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

                    if (miningCooldown <= 0.0)
                    {
                        miningProgress = 0.0;
                        miningTarget = IVec4();
                        miningCooldown = 0.0;
                    }
                }

                if (miningCooldown > 0.0)
                {
                    miningCooldown -= dt;
                    if (miningCooldown <= 0.0)
                    {
                        int destroyedType = world.get(miningTarget);
                        world.set(miningTarget, 0);
                        mapChanged = true; changedPos = miningTarget; changedType = 0;
                        bool harvested = canHarvest(destroyedType, inventory.hotbarBlockType(selectedSlot));

                        if (harvested)
                            playSFX(popPath);

                        if (harvested && destroyedType != BLOCK_LEAVES)
                        {
                            int dropType = destroyedType;
                            int dropCount = 1;

                            if (destroyedType == BLOCK_GRASS)
                                dropType = BLOCK_DIRT;

                            else if (destroyedType == BLOCK_STONE)
                                dropType = BLOCK_COBBLESTONE;

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

            if (input.getMouseClick(1))
            {

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
                    if (placeType <= 0) {  }
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

        {
            cleardevice();
            setbkcolor(RGB(10, 10, 30));

            if (hasTarget)
                renderer.setTargetBlock(targetedBlock);
            else
                renderer.clearTargetBlock();

            if (!isCreative && miningProgress > 0.0 && miningTotalTime > 0.0 && world.get(miningTarget) > 0)
                renderer.setMiningTarget(miningTarget, miningProgress / miningTotalTime);
            else
                renderer.clearMiningTarget();

            renderer.renderWorld(world, camera);

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

    mciSendString(L"stop bgm", NULL, 0, NULL);
    mciSendString(L"close bgm", NULL, 0, NULL);
    mciSendString(L"close sfx", NULL, 0, NULL);

    closegraph();
}