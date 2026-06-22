#include "renderer.h"
#include "constant.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <iostream>
#include <windows.h>

// ============================================================================
// Alpha 混合：EasyX PNG 像素 (0xAARRGGBB) → DIB 像素 (0x00RRGGBB)
// ============================================================================

/** @brief 将 ARGB 源像素叠加到 DIB 目标像素，返回混合结果 */
static inline DWORD alphaBlend(DWORD dst, DWORD src)
{
    unsigned int a = (src >> 24) & 0xFF;
    if (a == 0) return dst;                     // 全透明，保持目标
    if (a == 255) return src & 0x00FFFFFF;       // 不透明，直接覆盖（去掉 alpha 字节）
    // 半透明混合
    unsigned int sr = (src >> 16) & 0xFF;
    unsigned int sg = (src >> 8) & 0xFF;
    unsigned int sb = src & 0xFF;
    unsigned int dr = (dst >> 16) & 0xFF;
    unsigned int dg = (dst >> 8) & 0xFF;
    unsigned int db = dst & 0xFF;
    unsigned int invA = 255 - a;
    dr = (sr * a + dr * invA) / 255;
    dg = (sg * a + dg * invA) / 255;
    db = (sb * a + db * invA) / 255;
    return RGB(dr, dg, db);
}

// ============================================================================
// 构造 / 析构
// ============================================================================

Renderer::Renderer(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_blockHalf(0.5)
    , m_frameCount(0)
    , m_hBmp(nullptr)
    , m_memDC(nullptr)
    , m_oldBmp(nullptr)
    , m_dibReady(false)
    , m_hFont(nullptr)
    , m_hFontLarge(nullptr)
    , m_hOldFont(nullptr)
    , m_fpsFrames(0)
    , m_fpsTime(0)
    , m_fps(0)
    , m_diagTotal(0)
    , m_diagSlice(0)
    , m_diagOccl(0)
    , m_diagGeom(0)
    , m_diagFaces(0)
    , m_diagFaceCull(0)
    , m_diagChunkTotal(0)
    , m_diagChunkPass(0)
    , m_msCollect(0.0)
    , m_msFrustum(0.0)
    , m_msBlock2Tri(0.0)
    , m_msRaster(0.0)
    , m_blockTexLoaded(false)
{
    m_zbuf.resize(m_screenWidth * m_screenHeight);
    memset(m_texPixels, 0, sizeof(m_texPixels));
    memset(m_texW, 0, sizeof(m_texW));
    memset(m_texH, 0, sizeof(m_texH));

    // 预创建 DIB
    HDC hdc = GetImageHDC();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_screenWidth;
    bmi.bmiHeader.biHeight = -m_screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    DWORD *bits = nullptr;
    m_hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void **>(&bits), nullptr, 0);
    if (m_hBmp && bits)
    {
        m_memDC = CreateCompatibleDC(hdc);
        m_oldBmp = (HBITMAP) SelectObject(m_memDC, m_hBmp);
        m_pBits = bits;
        m_dibReady = true;

        // 加载 Minecraft AE 字体（小号：HUD / 大号：按钮）
        m_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Minecraft AE");
        m_hFontLarge = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Minecraft AE");
        if (m_hFont)
            m_hOldFont = (HFONT) SelectObject(m_memDC, m_hFont);
    }
}

Renderer::~Renderer()
{
    if (m_dibReady)
    {
        // 恢复原字体
        if (m_hOldFont)
            SelectObject(m_memDC, m_hOldFont);
        if (m_hFont)
            DeleteObject(m_hFont);
        if (m_hFontLarge)
            DeleteObject(m_hFontLarge);

        SelectObject(m_memDC, m_oldBmp);
        DeleteDC(m_memDC);
        DeleteObject(m_hBmp);
    }
}

// ============================================================================
// 方块贴图加载（像素数据）
// ============================================================================

static void loadTexPixels(const wchar_t *path, COLORREF out[16][16], int &w, int &h)
{
    w = h = 0;
    IMAGE img;
    loadimage(&img, path);
    DWORD *buf = GetImageBuffer(&img);
    if (!buf) return;
    w = img.getwidth(); h = img.getheight();
    if (w <= 0 || h <= 0) { w = h = 0; return; }
    if (w > 16) w = 16;
    if (h > 16) h = 16;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            out[y][x] = buf[y * img.getwidth() + x];
}

void Renderer::loadBlockTextures()
{
    int tw, th;
    // 草方块
    loadTexPixels(L"../assert/texture/grass_block_top.png", m_texPixels[0], tw, th); m_texW[0] = tw; m_texH[0] = th;
    loadTexPixels(L"../assert/texture/grass_block_side.png", m_texPixels[1], tw, th); m_texW[1] = tw; m_texH[1] = th;
    loadTexPixels(L"../assert/texture/grass_block_bottom.png", m_texPixels[2], tw, th); m_texW[2] = tw; m_texH[2] = th;
    // 泥土
    loadTexPixels(L"../assert/texture/dirt_top.png", m_texPixels[3], tw, th); m_texW[3] = tw; m_texH[3] = th;
    loadTexPixels(L"../assert/texture/dirt_side.png", m_texPixels[4], tw, th); m_texW[4] = tw; m_texH[4] = th;
    loadTexPixels(L"../assert/texture/dirt_bottom.png", m_texPixels[5], tw, th); m_texW[5] = tw; m_texH[5] = th;
    // 树干
    loadTexPixels(L"../assert/texture/oak_log_top.png", m_texPixels[6], tw, th); m_texW[6] = tw; m_texH[6] = th;
    loadTexPixels(L"../assert/texture/oak_log_side.png", m_texPixels[7], tw, th); m_texW[7] = tw; m_texH[7] = th;
    loadTexPixels(L"../assert/texture/oak_log_bottom.png", m_texPixels[8], tw, th); m_texW[8] = tw; m_texH[8] = th;
    // 树叶
    loadTexPixels(L"../assert/texture/oak_leaves_top.png", m_texPixels[9], tw, th); m_texW[9] = tw; m_texH[9] = th;
    loadTexPixels(L"../assert/texture/oak_leaves_side.png", m_texPixels[10], tw, th); m_texW[10] = tw; m_texH[10] = th;
    loadTexPixels(L"../assert/texture/oak_leaves_bottom.png", m_texPixels[11], tw, th); m_texW[11] = tw; m_texH[11] = th;
    // 石头
    loadTexPixels(L"../assert/texture/stone_top.png", m_texPixels[12], tw, th); m_texW[12] = tw; m_texH[12] = th;
    loadTexPixels(L"../assert/texture/stone_side.png", m_texPixels[13], tw, th); m_texW[13] = tw; m_texH[13] = th;
    loadTexPixels(L"../assert/texture/stone_bottom.png", m_texPixels[14], tw, th); m_texW[14] = tw; m_texH[14] = th;
    // 木板
    loadTexPixels(L"../assert/texture/oak_planks_top.png", m_texPixels[15], tw, th); m_texW[15] = tw; m_texH[15] = th;
    loadTexPixels(L"../assert/texture/oak_planks_side.png", m_texPixels[16], tw, th); m_texW[16] = tw; m_texH[16] = th;
    loadTexPixels(L"../assert/texture/oak_planks_bottom.png", m_texPixels[17], tw, th); m_texW[17] = tw; m_texH[17] = th;

    m_blockTexLoaded = true;
}

int Renderer::blockTexId(int blockType, int face)
{
    if (blockType <= 0 || blockType > 6) return -1;
    return (blockType - 1) * 3 + face;  // face: 0=顶,1=侧,2=底
}

COLORREF Renderer::sampleTexture(int texId, double tu, double tv) const
{
    if (texId < 0 || texId >= MAX_TEX || !m_blockTexLoaded)
        return RGB(128, 128, 128);
    int w = m_texW[texId], h = m_texH[texId];
    if (w <= 0 || h <= 0) return RGB(128, 128, 128);
    tu = tu - std::floor(tu);
    tv = tv - std::floor(tv);
    if (tu < 0) tu += 1.0;
    if (tv < 0) tv += 1.0;
    int px = (int) (tu * w);
    int py = (int) (tv * h);
    if (px < 0) px = 0;
    if (px >= w) px = w - 1;
    if (py < 0) py = 0;
    if (py >= h) py = h - 1;
    return m_texPixels[texId][py][px];
}

// ============================================================================
// 热键栏
// ============================================================================

static const wchar_t *kHotbarIcons[9] = {
    L"../assert/gui/item/grass_block.png",
    L"../assert/gui/item/dirt.png",
    L"../assert/gui/item/oak_log.png",
    L"../assert/gui/item/oak_leaves.png",
    L"../assert/gui/item/stone.png",
    L"../assert/gui/item/oak_planks.png",
    L"",  // 槽位 7（空）
    L"",  // 槽位 8（空）
    L""   // 槽位 9（空）
};

static const wchar_t *kBigIconPaths[9] = {
    L"../assert/gui/block/grass_block.png",
    L"../assert/gui/block/dirt.png",
    L"../assert/gui/block/oak_log.png",
    L"../assert/gui/block/oak_leaves.png",
    L"../assert/gui/block/stone.png",
    L"../assert/gui/block/oak_planks.png",
    L"",
    L"",
    L""
};

void Renderer::loadHotbar()
{
    bool bgOk = false;
    // 加载 hotbar 背景
    {
        IMAGE img;
        loadimage(&img, L"../assert/gui/widget/hotbar.png");
        DWORD *buf = GetImageBuffer(&img);
        int srcW = img.getwidth();
        if (buf && srcW > 0)
        {
            m_hbBgW = img.getwidth(); m_hbBgH = img.getheight();
            m_hotbarBg.resize(m_hbBgW * m_hbBgH);
            for (int y = 0; y < m_hbBgH; ++y)
                for (int x = 0; x < m_hbBgW; ++x)
                    m_hotbarBg[y * m_hbBgW + x] = buf[y * srcW + x];
            bgOk = true;
        }
    }
    // 加载快捷栏图标（用 loadimage 缩放至槽位显示尺寸）
    double scale = (double) HB_HEIGHT / m_hbBgH;
    int iconSz = (int) (HB_SLOT_SIZE * scale);
    if (iconSz < 1) iconSz = 1;
    m_hbIconDisplaySize = iconSz;

    for (int i = 0; i < HOTBAR_SLOTS; ++i)
    {
        if (kHotbarIcons[i][0] == L'\0') continue;  // 空槽位跳过
        IMAGE img;
        loadimage(&img, kHotbarIcons[i], iconSz, iconSz, true);  // loadimage 缩放
        DWORD *buf = GetImageBuffer(&img);
        int srcW = img.getwidth();
        if (buf && srcW > 0)
        {
            m_hotbarIcons[i].resize(iconSz * iconSz);
            for (int y = 0; y < iconSz; ++y)
                for (int x = 0; x < iconSz; ++x)
                    m_hotbarIcons[i][y * iconSz + x] = buf[y * srcW + x];
        }
    }
    // 加载右下角大图标（loadimage 缩放至 32x32）
    const int BIG = HB_ICON_SIZE * 2;
    for (int i = 0; i < HOTBAR_SLOTS; ++i)
    {
        if (kBigIconPaths[i][0] == L'\0') continue;
        IMAGE img;
        loadimage(&img, kBigIconPaths[i], BIG, BIG, true);
        DWORD *buf = GetImageBuffer(&img);
        int srcW = img.getwidth();
        if (buf && srcW > 0)
        {
            m_hotbarIconsBig[i].resize(BIG * BIG);
            for (int y = 0; y < BIG; ++y)
                for (int x = 0; x < BIG; ++x)
                    m_hotbarIconsBig[i][y * BIG + x] = buf[y * srcW + x];
        }
    }
    m_hotbarBlockTypes[0] = BLOCK_GRASS;
    m_hotbarBlockTypes[1] = BLOCK_DIRT;
    m_hotbarBlockTypes[2] = BLOCK_LOG;
    m_hotbarBlockTypes[3] = BLOCK_LEAVES;
    m_hotbarBlockTypes[4] = BLOCK_STONE;
    m_hotbarBlockTypes[5] = BLOCK_PLANKS;
    m_hotbarBlockTypes[6] = BLOCK_AIR;
    m_hotbarBlockTypes[7] = BLOCK_AIR;
    m_hotbarBlockTypes[8] = BLOCK_AIR;

    // 加载选中框 select.png（与 hotbar 同缩放比）
    {
        IMAGE selImg;
        loadimage(&selImg, L"../assert/gui/widget/select.png");
        int selW = selImg.getwidth(), selH = selImg.getheight();
        if (selW > 0 && selH > 0)
        {
            // 按 hotbar 缩放比缩放至显示尺寸
            int dispW = (int) (selW * scale);
            int dispH = (int) (selH * scale);
            if (dispW < 1) dispW = 1;
            if (dispH < 1) dispH = 1;
            loadimage(&selImg, L"../assert/gui/widget/select.png", dispW, dispH, true);
            DWORD *buf = GetImageBuffer(&selImg);
            int srcW = selImg.getwidth();
            if (buf && srcW > 0)
            {
                m_selectW = dispW; m_selectH = dispH;
                m_selectPixels.resize(dispW * dispH);
                for (int y = 0; y < dispH; ++y)
                    for (int x = 0; x < dispW; ++x)
                        m_selectPixels[y * dispW + x] = buf[y * srcW + x];
            }
        }
    }

    m_hotbarLoaded = bgOk;  // 仅背景加载成功才允许绘制
}

void Renderer::drawHotbar(int selectedSlot)
{
    if (!m_hotbarLoaded || !m_dibReady || m_hbBgW <= 0 || m_hbBgH <= 0) return;

    // 按高度 44 缩放，宽度自适应（保持比例）
    double scale = (double) HB_HEIGHT / m_hbBgH;
    int hbW = (int) (m_hbBgW * scale);
    int hbH = HB_HEIGHT;
    int hbX = (m_screenWidth - hbW) / 2;
    int hbY = m_screenHeight - hbH - 4;

    // 绘制 hotbar 背景
    for (int dy = 0; dy < hbH; ++dy)
    {
        int sy = (int) (dy / scale);
        if (sy >= m_hbBgH) sy = m_hbBgH - 1;
        for (int dx = 0; dx < hbW; ++dx)
        {
            int sx = (int) (dx / scale);
            if (sx >= m_hbBgW) sx = m_hbBgW - 1;
            COLORREF c = m_hotbarBg[sy * m_hbBgW + sx];
            if (c == 0 || c == RGB(0, 0, 0)) continue;
            int px = hbX + dx, py = hbY + dy;
            if (px >= 0 && px < m_screenWidth && py >= 0 && py < m_screenHeight)
                m_pBits[py * m_screenWidth + px] = alphaBlend(m_pBits[py * m_screenWidth + px], c);
        }
    }

    // 绘制每个槽位的图标（loadimage 已预缩放到显示尺寸，1:1 复制）
    for (int slot = 0; slot < HOTBAR_SLOTS; ++slot)
    {
        if (m_hotbarIcons[slot].empty()) continue;
        int nativeX = HB_SLOT_ORIGIN_X + slot * HB_SLOT_STEP;
        int nativeY = HB_SLOT_ORIGIN_Y;
        int screenX = hbX + (int) (nativeX * scale);
        int screenY = hbY + (int) (nativeY * scale);
        int sz = m_hbIconDisplaySize;

        for (int dy = 0; dy < sz; ++dy)
        {
            int py = screenY + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            int srcRow = dy * sz;
            for (int dx = 0; dx < sz; ++dx)
            {
                COLORREF c = m_hotbarIcons[slot][srcRow + dx];
                if (c == 0) continue;
                int px = screenX + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
            }
        }
    }

    // 绘制选中框（select.png 的 (3,3) 与选中槽位左上角对齐）
    if (!m_selectPixels.empty() && selectedSlot >= 0 && selectedSlot < HOTBAR_SLOTS)
    {
        // 选中槽位原生左上角 = (3 + slot*20, 3)
        // select 的 (3,3) 对上去 → select 绘制起点 = (slot*20*scale, 0) 相对 hbX,hbY
        int selX = hbX + (int) (selectedSlot * HB_SLOT_STEP * scale);
        int selY = hbY;
        for (int dy = 0; dy < m_selectH; ++dy)
        {
            int py = selY + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            int srcRow = dy * m_selectW;
            for (int dx = 0; dx < m_selectW; ++dx)
            {
                COLORREF c = m_selectPixels[srcRow + dx];
                if (c == 0) continue;  // 全透明跳过
                int px = selX + dx;
                if (px >= 0 && px < m_screenWidth)
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
            }
        }
    }

    // 右下角显示当前选中方块（32x32，loadimage 缩放）
    if (!m_hotbarIconsBig[selectedSlot].empty())
    {
        const int BIG = HB_ICON_SIZE * 2;
        int bx = m_screenWidth - BIG - 8;
        int by = m_screenHeight - BIG - 8;
        for (int dy = 0; dy < BIG; ++dy)
        {
            for (int dx = 0; dx < BIG; ++dx)
            {
                COLORREF c = m_hotbarIconsBig[selectedSlot][dy * BIG + dx];
                if (c == 0) continue;
                int px = bx + dx, py = by + dy;
                if (px >= 0 && px < m_screenWidth && py >= 0 && py < m_screenHeight)
                    m_pBits[py * m_screenWidth + px] = alphaBlend(m_pBits[py * m_screenWidth + px], c);
            }
        }
    }

    // 刷新到屏幕
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);
}

int Renderer::getHotbarBlockType(int slot) const
{
    if (slot < 0 || slot >= HOTBAR_SLOTS) return 1;
    return m_hotbarBlockTypes[slot];
}

// ============================================================================
// 缓冲管理
// ============================================================================

void Renderer::resetBuffers()
{
    std::fill(m_zbuf.begin(), m_zbuf.end(), 1e30);
}

// ============================================================================
// 主渲染入口
// ============================================================================

void Renderer::renderWorld(const World &world, const Camera4D &cam)
{
    if (!m_dibReady) return;

    ++m_frameCount;

    // 1. 清空帧缓冲
    resetBuffers();

    DWORD bg = 0x001E0A0A;
    int total = m_screenWidth * m_screenHeight;
    DWORD *bits = m_pBits;
    for (int i = 0; i < total; ++i) bits[i] = bg;

    // 2. 获取观察平面
    Plane2D plane = cam.getViewPlane();

    // 3. 诊断计数初始化
    m_diagTotal = static_cast<int>(world.totalBlocks());
    m_diagSlice = 0;
    m_diagOccl = 0;
    m_diagGeom = 0;
    m_diagFaces = 0;
    m_diagFaceCull = 0;

    // 4. 收集可见方块（含统计）
    clock_t t0 = clock();
    int preOccl = 0;
    std::vector<IVec4> visibleBlocks = collectVisibleBlocks(world, cam, plane, preOccl);
    m_msCollect = static_cast<double>(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    m_diagSlice = preOccl;
    m_diagOccl = preOccl - static_cast<int>(visibleBlocks.size());

    if (!visibleBlocks.empty())
    {
        // 5. 设置 3D 相机（提前，用于视锥体裁剪）
        Camera3D cam3d;
        {
            double pitch = cam.getPitch();
            double sP = std::sin(pitch), cP = std::cos(pitch);
            // 3D 相机位于相机相对空间原点（多边形顶点已在相机相对坐标中）
            cam3d.posU = 0.0;
            cam3d.posV = 0.0;
            cam3d.posY = 0.0;
            cam3d.dirU = 0.0;
            cam3d.dirV = cP;
            cam3d.dirY = sP;
        }

        // 6. 4D→3D：方块 → 三角形（含视锥体裁剪）
        {
            std::vector<Tri3D> allTris;
            allTris.reserve(visibleBlocks.size() * 12);

            double half = m_blockHalf, sp = half * 2.0;
            const Vec4 &camPos = cam.getPos();

            // 预计算视锥体裁剪用的 camera right/up
            double rU = cam3d.dirV, rV = -cam3d.dirU;
            double rLen = std::sqrt(rU * rU + rV * rV);
            rU /= rLen; rV /= rLen;
            double upU = rV * cam3d.dirY, upV = -rU * cam3d.dirY, upY = rU * cam3d.dirV - rV * cam3d.dirU;

            clock_t tLoop = clock();
            clock_t tBlock2Tri = 0;
            for (const auto &blk : visibleBlocks)
            {
                // 方块中心在 3D 空间的近似坐标
                double bu = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.p);
                double bv = vec3Dot(Vec3(blk.x * sp - camPos.x, blk.z * sp - camPos.z, blk.w * sp - camPos.w), plane.q);
                double by = blk.y * sp - camPos.y;

                // 变换到相机空间
                double dU = bu - cam3d.posU;
                double dV = bv - cam3d.posV;
                double dY = by - cam3d.posY;
                double camZ = cam3d.dirU * dU + cam3d.dirV * dV + cam3d.dirY * dY;

                // 在相机后方或太远则跳过
                if (camZ < cam3d.nearPlane || camZ > cam3d.farPlane) continue;

                // 视锥体水平/垂直检查（加 margin）
                double margin = half * 3.0;
                double camX = rU * dU + rV * dV;
                double camY = upU * dU + upV * dV + upY * dY;
                double halfH = std::tan(cam3d.fov * 0.5) * camZ;
                double halfW = halfH * m_screenWidth / m_screenHeight;
                if (camX < -halfW - margin || camX > halfW + margin) continue;
                if (camY < -halfH - margin || camY > halfH + margin) continue;

                clock_t t0 = clock();
                int bt = world.get(blk);
                int topId = blockTexId(bt, 0);
                int sideId = blockTexId(bt, 1);
                int bottomId = blockTexId(bt, 2);
                size_t before = allTris.size();
                blockToTriangles(blk.x, blk.y, blk.z, blk.w, cam, plane, topId, sideId, bottomId, allTris, world);
                if (allTris.size() > before) ++m_diagGeom;
                tBlock2Tri += clock() - t0;
            }
            m_msFrustum = static_cast<double>((clock() - tLoop) - tBlock2Tri) * 1000.0 / CLOCKS_PER_SEC;
            m_msBlock2Tri = static_cast<double>(tBlock2Tri) * 1000.0 / CLOCKS_PER_SEC;

            m_diagFaces = static_cast<int>(allTris.size());

            if (!allTris.empty())
            {
                // 7. 光栅化
                clock_t t2 = clock();
                rasterizeTriangles(allTris, cam3d);
                m_msRaster = static_cast<double>(clock() - t2) * 1000.0 / CLOCKS_PER_SEC;
            }
        }
    }

    // 7.5. 目标方块线框（与面片共享深度缓冲）
    if (m_hasTarget)
        drawBlockOutline(m_targetBlock, cam);

    // 8. 输出 DIB 到屏幕
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);

    // 9. 绘制 HUD 和准星
    drawHUD(cam);
    drawCrosshair();

    // 10. FPS 统计
    ++m_fpsFrames;
    clock_t now = clock();
    double elapsed = static_cast<double>(now - m_fpsTime) / CLOCKS_PER_SEC;
    if (elapsed >= 1.0)
    {
        m_fps = static_cast<int>(m_fpsFrames / elapsed);
        m_fpsFrames = 0;
        m_fpsTime = now;
    }
}

// ============================================================================
// 准星
// ============================================================================

void Renderer::drawCrosshair()
{
    int cx = m_screenWidth / 2;
    int cy = m_screenHeight / 2;
    int len = 8;

    setlinecolor(RGB(255, 255, 255));
    line(cx - len, cy, cx + len, cy);
    line(cx, cy - len, cx, cy + len);
}

// ============================================================================
// HUD
// ============================================================================

void Renderer::drawHUD(const Camera4D &cam)
{
    const Vec4 &pos = cam.getPos();
    const Vec4 &r = cam.getRight();
    const Vec4 &f = cam.getForward();
    const Vec4 &o = cam.getOver();

    wchar_t buf[256];
    HDC hdc = GetImageHDC();
    HFONT oldHudFont = m_hFont ? (HFONT) SelectObject(hdc, m_hFont) : nullptr;
    SetBkMode(hdc, TRANSPARENT);

    // ========================================
    // 左上角：XZW 三维坐标系可视化
    // ========================================
    const int vpX = 10, vpY = 10, vpW = 150, vpH = 140;
    const int ox = vpX + vpW / 2, oy = vpY + vpH / 2 + 5;
    const double vs = 45.0;

    // 黑底
    setfillcolor(BLACK);
    solidrectangle(vpX, vpY, vpX + vpW, vpY + vpH);

    auto proj3 = [&](double vx, double vz, double vw) -> POINT
    {
        const double k = 0.35355339;
        return {
            static_cast<int>(ox + vz * vs - vx * vs * k),
            static_cast<int>(oy - vw * vs + vx * vs * k)
        };
    };

    POINT oPt = proj3(0, 0, 0);

    // ---- n 的垂直平面（半透明粉色四边形） ----
    Vec4 n3 = Vec4(o.x, 0.0, o.z, o.w);
    double nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }

    Vec4 u3, v3;
    if (std::abs(n3.x) < 0.9)      u3 = Vec4(1.0, 0.0, 0.0, 0.0);
    else if (std::abs(n3.z) < 0.9) u3 = Vec4(0.0, 0.0, 1.0, 0.0);
    else                           u3 = Vec4(0.0, 0.0, 0.0, 1.0);
    double dotUN = u3.x * n3.x + u3.z * n3.z + u3.w * n3.w;
    u3 = Vec4(u3.x - dotUN * n3.x, 0.0, u3.z - dotUN * n3.z, u3.w - dotUN * n3.w);
    double uLen = std::sqrt(u3.x * u3.x + u3.z * u3.z + u3.w * u3.w);
    if (uLen > 1e-9) { u3.x /= uLen; u3.z /= uLen; u3.w /= uLen; }
    v3.x = n3.z * u3.w - n3.w * u3.z;
    v3.z = n3.w * u3.x - n3.x * u3.w;
    v3.w = n3.x * u3.z - n3.z * u3.x;

    const double pHalf = 1.1;
    POINT pCorners[4] = {
        proj3(u3.x * pHalf + v3.x * pHalf,  u3.z * pHalf + v3.z * pHalf,  u3.w * pHalf + v3.w * pHalf),
        proj3(u3.x * pHalf - v3.x * pHalf,  u3.z * pHalf - v3.z * pHalf,  u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf - v3.x * pHalf, -u3.z * pHalf - v3.z * pHalf, -u3.w * pHalf - v3.w * pHalf),
        proj3(-u3.x * pHalf + v3.x * pHalf, -u3.z * pHalf + v3.z * pHalf, -u3.w * pHalf + v3.w * pHalf)
    };
    setfillcolor(RGB(80, 60, 90));
    setlinecolor(RGB(150, 100, 180));
    fillpolygon(pCorners, 4);

    // ---- 坐标轴 ----
    auto drawArrow2 = [](POINT from, POINT to, COLORREF clr)
    {
        setlinecolor(clr);
        line(from.x, from.y, to.x, to.y);
    };

    POINT xPt = proj3(1.2, 0, 0);
    POINT zPt = proj3(0, 1.2, 0);
    POINT wPt = proj3(0, 0, 1.2);
    drawArrow2(oPt, xPt, RGB(255, 100, 100));
    drawArrow2(oPt, zPt, RGB(100, 255, 100));
    drawArrow2(oPt, wPt, RGB(100, 150, 255));

    settextcolor(RGB(255, 100, 100)); TextOutW(hdc, xPt.x - 20, xPt.y + 2, L"X", 1);
    settextcolor(RGB(100, 255, 100)); TextOutW(hdc, zPt.x + 2, zPt.y - 8, L"Z", 1);
    settextcolor(RGB(100, 150, 255)); TextOutW(hdc, wPt.x - 8, wPt.y - 18, L"W", 1);

    // i（forward，黄色）
    Vec4 i3 = Vec4(f.x, 0.0, f.z, f.w);
    double iLen = vec4Length(i3);
    if (iLen > 1e-9) { i3 = vec4Scale(i3, 1.0 / iLen); }
    POINT iP = proj3(i3.x, i3.z, i3.w);
    drawArrow2(oPt, iP, RGB(255, 220, 50));
    settextcolor(RGB(255, 220, 50)); TextOutW(hdc, iP.x + 3, iP.y - 10, L"i", 1);

    // j（right，青色）
    Vec4 j3 = Vec4(r.x, 0.0, r.z, r.w);
    double jLen = vec4Length(j3);
    if (jLen > 1e-9) { j3 = vec4Scale(j3, 1.0 / jLen); }
    POINT jP = proj3(j3.x, j3.z, j3.w);
    drawArrow2(oPt, jP, RGB(50, 220, 220));
    settextcolor(RGB(50, 220, 220)); TextOutW(hdc, jP.x + 3, jP.y - 10, L"j", 1);

    // n（over，粉色法向）
    n3 = Vec4(o.x, 0.0, o.z, o.w);
    nLen = vec4Length(n3);
    if (nLen > 1e-9) { n3 = vec4Scale(n3, 1.0 / nLen); }
    POINT nP = proj3(n3.x, n3.z, n3.w);
    drawArrow2(oPt, nP, RGB(255, 120, 255));
    settextcolor(RGB(255, 120, 255)); TextOutW(hdc, nP.x + 3, nP.y - 10, L"n", 1);

    // ========================================
    // FPS + 诊断（右上角，F3 切换）
    // ========================================
    if (m_showHUD)
    {
        settextcolor(RGB(255, 255, 255));
        swprintf(buf, 256, L"FPS: %d", m_fps);
        TextOutW(hdc, m_screenWidth - 100, 10, buf, (int) wcslen(buf));

        settextcolor(RGB(255, 255, 100));
        swprintf(buf, 256, L"方块总数: %d", m_diagTotal);
        TextOutW(hdc, m_screenWidth - 200, 30, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"切片通过: %d", m_diagSlice);
        TextOutW(hdc, m_screenWidth - 200, 48, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"遮挡通过: %d", m_diagOccl);
        TextOutW(hdc, m_screenWidth - 200, 66, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"几何生成: %d", m_diagGeom);
        TextOutW(hdc, m_screenWidth - 200, 84, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"渲染面数: %d", m_diagFaces);
        TextOutW(hdc, m_screenWidth - 200, 102, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"面剔除: %d", m_diagFaceCull);
        TextOutW(hdc, m_screenWidth - 200, 120, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"区块: %d/%d", m_diagChunkPass, m_diagChunkTotal);
        TextOutW(hdc, m_screenWidth - 200, 138, buf, (int) wcslen(buf));

        settextcolor(RGB(180, 220, 255));
        swprintf(buf, 256, L"收集: %.1fms", m_msCollect);
        TextOutW(hdc, m_screenWidth - 200, 156, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"视锥裁剪: %.1fms", m_msFrustum);
        TextOutW(hdc, m_screenWidth - 200, 174, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"方块->三角形: %.1fms", m_msBlock2Tri);
        TextOutW(hdc, m_screenWidth - 200, 192, buf, (int) wcslen(buf));
        swprintf(buf, 256, L"光栅化: %.1fms", m_msRaster);
        TextOutW(hdc, m_screenWidth - 200, 210, buf, (int) wcslen(buf));
        settextcolor(RGB(255, 255, 255));
    } // m_showHUD

    // ========================================
    // 坐标信息（左侧，坐标系下方）
    // ========================================
    int infoY = vpY + vpH + 5;
    swprintf(buf, 256, L"%.1f,  %.1f,  %.1f,  %.1f",
        pos.x, pos.y, pos.z, pos.w);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));
    infoY += 18;

    const double rad2deg = 180.0 / 3.1415926535;
    swprintf(buf, 256, L"俯仰角: %+.1f", cam.getPitch() * rad2deg);
    TextOutW(hdc, 10, infoY, buf, (int) wcslen(buf));

    if (oldHudFont)
        SelectObject(hdc, oldHudFont);
}

// ============================================================================
// 方块收集
// ============================================================================

static bool blockIntersectsPlane(int bx, int /*by*/, int bz, int bw,
    const Vec4 &camPos, const Plane2D &plane, double half, double sp)
{
    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    double minDot = 0.0, maxDot = 0.0;
    double nx = plane.n.x, nz = plane.n.z, nw = plane.n.w;

    if (nx > 0) { minDot += nx * x0; maxDot += nx * x1; }
    else { minDot += nx * x1; maxDot += nx * x0; }
    if (nz > 0) { minDot += nz * z0; maxDot += nz * z1; }
    else { minDot += nz * z1; maxDot += nz * z0; }
    if (nw > 0) { minDot += nw * w0; maxDot += nw * w1; }
    else { minDot += nw * w1; maxDot += nw * w0; }

    return minDot <= 0.0 && maxDot >= 0.0;
}

std::vector<IVec4> Renderer::collectVisibleBlocks(const World &world,
    const Camera4D &cam, const Plane2D &plane,
    int &outPreOccl)
{
    std::vector<IVec4> result;
    outPreOccl = 0;

    const Vec4 &camPos = cam.getPos();
    double sp = m_blockHalf * 2.0;

    // ---- 遍历所有区块：先做 Chunk 级平面交会快测 ----
    int chunksTotal = 0, chunksPass = 0;
    for (const auto &[chunkPos, chunk] : world.getChunks())
    {
        if (chunk.empty()) continue;
        ++chunksTotal;

        // Chunk xzw 包围盒 vs 视平面
        {
            double half = m_blockHalf;
            int sz = World::CHUNK_SIZE;
            double cx0 = (chunk.cx() * sz) * sp - camPos.x - half;
            double cx1 = (chunk.cx() * sz + sz - 1) * sp - camPos.x + half;
            double cz0 = (chunk.cz() * sz) * sp - camPos.z - half;
            double cz1 = (chunk.cz() * sz + sz - 1) * sp - camPos.z + half;
            double cw0 = (chunk.cw() * sz) * sp - camPos.w - half;
            double cw1 = (chunk.cw() * sz + sz - 1) * sp - camPos.w + half;

            double cmin = 0, cmax = 0;
            double nx = plane.n.x, nz = plane.n.z, nw = plane.n.w;
            if (nx > 0) { cmin += nx * cx0; cmax += nx * cx1; }
            else { cmin += nx * cx1; cmax += nx * cx0; }
            if (nz > 0) { cmin += nz * cz0; cmax += nz * cz1; }
            else { cmin += nz * cz1; cmax += nz * cz0; }
            if (nw > 0) { cmin += nw * cw0; cmax += nw * cw1; }
            else { cmin += nw * cw1; cmax += nw * cw0; }

            if (cmin > 0.0 || cmax < 0.0) continue;  // 区块不与视平面相交
        }
        ++chunksPass;

        for (const auto &[localPos, type] : chunk.blocks())
        {
            IVec4 blk = chunk.localToWorld(localPos.x, localPos.y, localPos.z, localPos.w);
            int bx = blk.x, by = blk.y, bz = blk.z, bw = blk.w;

            if (!blockIntersectsPlane(bx, by, bz, bw, camPos, plane, m_blockHalf, sp))
                continue;
            ++outPreOccl;

            // 遮挡剔除：8 个方向都有相邻方块则不可见
            if (world.get(IVec4(bx + 1, by, bz, bw)) &&
                world.get(IVec4(bx - 1, by, bz, bw)) &&
                world.get(IVec4(bx, by + 1, bz, bw)) &&
                world.get(IVec4(bx, by - 1, bz, bw)) &&
                world.get(IVec4(bx, by, bz + 1, bw)) &&
                world.get(IVec4(bx, by, bz - 1, bw)) &&
                world.get(IVec4(bx, by, bz, bw + 1)) &&
                world.get(IVec4(bx, by, bz, bw - 1)))
                continue;

            result.push_back(IVec4(bx, by, bz, bw));
        } // inner: chunk blocks
    } // outer: world chunks

    m_diagChunkTotal = chunksTotal;
    m_diagChunkPass = chunksPass;

    return result;
}

// ============================================================================
// 目标方块线框（与面片一起参与深度缓冲）
// ============================================================================

void Renderer::drawBlockOutline(const IVec4 &blockPos, const Camera4D &cam)
{
    const Vec4 &camPos = cam.getPos();
    double half = m_blockHalf;
    double sp = half * 2.0;

    int bx = blockPos.x, by = blockPos.y, bz = blockPos.z, bw = blockPos.w;

    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    Plane2D camPlane = cam.getViewPlane();
    camPlane.offset = 0.0;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return;

    double yLow = by * sp - camPos.y - half;
    double yHigh = by * sp - camPos.y + half;
    int n = poly.n;

    // 3D 相机（与 renderWorld 一致）
    Camera3D cam3d;
    {
        double pitch = cam.getPitch();
        double sP = std::sin(pitch), cP = std::cos(pitch);
        cam3d.posU = 0.0; cam3d.posV = 0.0; cam3d.posY = 0.0;
        cam3d.dirU = 0.0;
        cam3d.dirV = cP;
        cam3d.dirY = sP;
    }

    // 所有边用深度测试线段绘制（直接写入 DIB + z-buffer）
    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;

        // 顶面边
        drawOutlineEdge3D(poly.u[i], poly.v[i], yHigh,
            poly.u[j], poly.v[j], yHigh, cam3d);
        // 底面边
        drawOutlineEdge3D(poly.u[i], poly.v[i], yLow,
            poly.u[j], poly.v[j], yLow, cam3d);
        // 垂直边
        drawOutlineEdge3D(poly.u[i], poly.v[i], yLow,
            poly.u[i], poly.v[i], yHigh, cam3d);
    }
}

void Renderer::drawOutlineEdge3D(double u0, double v0, double y0,
    double u1, double v1, double y1,
    const Camera3D &cam3d)
{
    int sx0, sy0, sx1, sy1;
    double z0, z1;

    if (!project3D(u0, v0, y0, cam3d, m_screenWidth, m_screenHeight, sx0, sy0, z0)) return;
    if (!project3D(u1, v1, y1, cam3d, m_screenWidth, m_screenHeight, sx1, sy1, z1)) return;

    int dx = std::abs(sx1 - sx0);
    int dy = std::abs(sy1 - sy0);
    int steps = std::max(dx, dy);
    if (steps <= 0) return;

    double invSteps = 1.0 / steps;
    for (int i = 0; i <= steps; ++i)
    {
        double t = i * invSteps;
        int sx = static_cast<int>(sx0 + (sx1 - sx0) * t + 0.5);
        int sy = static_cast<int>(sy0 + (sy1 - sy0) * t + 0.5);
        double z = z0 + (z1 - z0) * t;

        if (sx < 0 || sx >= m_screenWidth || sy < 0 || sy >= m_screenHeight)
            continue;

        int idx = sy * m_screenWidth + sx;
        if (z <= m_zbuf[idx])
        {
            m_pBits[idx] = RGB(0, 0, 0);
            m_zbuf[idx] = z;  // 写入 z-buffer 防止后续面片覆盖线框
        }
    }
}

// ============================================================================
// 4D→3D：方块 → 三角形
// ============================================================================

void Renderer::blockToTriangles(int bx, int by, int bz, int bw,
    const Camera4D &cam, const Plane2D &plane,
    int topTexId, int sideTexId, int bottomTexId,
    std::vector<Tri3D> &outTris,
    const World &world)
{
    const Vec4 &camPos = cam.getPos();
    double half = m_blockHalf;
    double sp = half * 2.0;

    double x0 = bx * sp - camPos.x - half;
    double x1 = bx * sp - camPos.x + half;
    double z0 = bz * sp - camPos.z - half;
    double z1 = bz * sp - camPos.z + half;
    double w0 = bw * sp - camPos.w - half;
    double w1 = bw * sp - camPos.w + half;

    Plane2D camPlane = plane;
    camPlane.offset = 0.0;

    PolyOnPlane poly = intersectCubePlane(x0, x1, z0, z1, w0, w1, camPlane);
    if (!poly.valid()) return;

    double yLow = by * sp - camPos.y - half;
    double yHigh = by * sp - camPos.y + half;

    // ±Y 面剔除：相邻方块存在则跳过该面
    bool cullTop = world.get(IVec4(bx, by + 1, bz, bw)) != 0;
    bool cullBottom = world.get(IVec4(bx, by - 1, bz, bw)) != 0;
    if (cullTop) ++m_diagFaceCull;
    if (cullBottom) ++m_diagFaceCull;

    int n = poly.n;
    const auto &pu = poly.u;
    const auto &pv = poly.v;
    const auto &ox = poly.ox;
    const auto &oz = poly.oz;
    double invSp = 1.0 / sp;  // 1/方块边长

    // 顶面（yHigh）—— UV 来自方块原始 (x,z) 坐标，与视角无关
    if (!cullTop)
    {
        for (int i = 1; i < n - 1; ++i)
        {
            Tri3D t;
            t.u[0] = pu[0];     t.v[0] = pv[0];     t.y[0] = yHigh;
            t.u[1] = pu[i];     t.v[1] = pv[i];     t.y[1] = yHigh;
            t.u[2] = pu[i + 1]; t.v[2] = pv[i + 1]; t.y[2] = yHigh;
            t.tu[0] = (ox[0] - x0) * invSp;     t.tv[0] = (oz[0] - z0) * invSp;
            t.tu[1] = (ox[i] - x0) * invSp;     t.tv[1] = (oz[i] - z0) * invSp;
            t.tu[2] = (ox[i + 1] - x0) * invSp; t.tv[2] = (oz[i + 1] - z0) * invSp;
            t.color = RGB(128, 128, 128);
            t.depth = yHigh;
            t.texId = topTexId;
            outTris.push_back(t);
        }
    }

    // 底面（yLow）
    if (!cullBottom)
    {
        for (int i = 1; i < n - 1; ++i)
        {
            Tri3D b;
            b.u[0] = pu[0];     b.v[0] = pv[0];     b.y[0] = yLow;
            b.u[2] = pu[i];     b.v[2] = pv[i];     b.y[2] = yLow;
            b.u[1] = pu[i + 1]; b.v[1] = pv[i + 1]; b.y[1] = yLow;
            b.tu[0] = (ox[0] - x0) * invSp;     b.tv[0] = (oz[0] - z0) * invSp;
            b.tu[2] = (ox[i] - x0) * invSp;     b.tv[2] = (oz[i] - z0) * invSp;
            b.tu[1] = (ox[i + 1] - x0) * invSp; b.tv[1] = (oz[i + 1] - z0) * invSp;
            b.color = RGB(128, 128, 128);
            b.depth = yLow;
            b.texId = bottomTexId;
            outTris.push_back(b);
        }
    }

    // 侧面：n 条边，每条边 2 个三角形
    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;

        Tri3D t1;
        t1.u[0] = pu[i]; t1.v[0] = pv[i]; t1.y[0] = yLow;
        t1.u[1] = pu[j]; t1.v[1] = pv[j]; t1.y[1] = yLow;
        t1.u[2] = pu[i]; t1.v[2] = pv[i]; t1.y[2] = yHigh;
        t1.tu[0] = 0.0; t1.tv[0] = 1.0;
        t1.tu[1] = 1.0; t1.tv[1] = 1.0;
        t1.tu[2] = 0.0; t1.tv[2] = 0.0;
        t1.color = RGB(128, 128, 128);
        t1.depth = (yLow + yHigh) * 0.5;
        t1.texId = sideTexId;
        outTris.push_back(t1);

        Tri3D t2;
        t2.u[0] = pu[j]; t2.v[0] = pv[j]; t2.y[0] = yLow;
        t2.u[1] = pu[j]; t2.v[1] = pv[j]; t2.y[1] = yHigh;
        t2.u[2] = pu[i]; t2.v[2] = pv[i]; t2.y[2] = yHigh;
        t2.tu[0] = 1.0; t2.tv[0] = 1.0;
        t2.tu[1] = 1.0; t2.tv[1] = 0.0;
        t2.tu[2] = 0.0; t2.tv[2] = 0.0;
        t2.color = RGB(128, 128, 128);
        t2.depth = (yLow + yHigh) * 0.5;
        t2.texId = sideTexId;
        outTris.push_back(t2);
    }
}

// ============================================================================
// 3D→2D 光栅化
// ============================================================================

void Renderer::rasterizeTriangles(const std::vector<Tri3D> &tris,
    const Camera3D &cam3d)
{
    for (const auto &tri : tris)
        rasterizeTriangle(tri, cam3d);
}

void Renderer::rasterizeTriangle(const Tri3D &tri, const Camera3D &cam3d)
{
    int sx[3], sy[3];
    double sz[3];
    bool valid[3];

    for (int i = 0; i < 3; ++i)
    {
        valid[i] = project3D(tri.u[i], tri.v[i], tri.y[i],
            cam3d, m_screenWidth, m_screenHeight,
            sx[i], sy[i], sz[i]);
    }

    if (!valid[0] && !valid[1] && !valid[2]) return;

    // 按 y 排序
    int idx[3] = { 0, 1, 2 };
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);
    if (sy[idx[1]] > sy[idx[2]]) std::swap(idx[1], idx[2]);
    if (sy[idx[0]] > sy[idx[1]]) std::swap(idx[0], idx[1]);

    int y0 = sy[idx[0]], y1 = sy[idx[1]], y2 = sy[idx[2]];
    int x0 = sx[idx[0]], x1 = sx[idx[1]], x2 = sx[idx[2]];
    double z0 = sz[idx[0]], z1 = sz[idx[1]], z2 = sz[idx[2]];

    // 透视校正：插值 tu/z, tv/z, 1/z
    double tu0 = tri.tu[idx[0]], tu1 = tri.tu[idx[1]], tu2 = tri.tu[idx[2]];
    double tv0 = tri.tv[idx[0]], tv1 = tri.tv[idx[1]], tv2 = tri.tv[idx[2]];
    double tuz0 = tu0 / z0, tuz1 = tu1 / z1, tuz2 = tu2 / z2;
    double tvz0 = tv0 / z0, tvz1 = tv1 / z1, tvz2 = tv2 / z2;
    double ooz0 = 1.0 / z0, ooz1 = 1.0 / z1, ooz2 = 1.0 / z2;

    if (y2 < 0 || y0 >= m_screenHeight) return;
    if (y0 == y2) return;

    // 上半部分：y0 → y1
    if (y1 > y0)
    {
        double invDyTop = 1.0 / static_cast<double>(y1 - y0);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x1 - x0) * invDyTop;
        double dxR = static_cast<double>(x2 - x0) * invDyFull;
        double dzL = (z1 - z0) * invDyTop;
        double dzR = (z2 - z0) * invDyFull;
        double dtuL = (tuz1 - tuz0) * invDyTop, dtuR = (tuz2 - tuz0) * invDyFull;
        double dtvL = (tvz1 - tvz0) * invDyTop, dtvR = (tvz2 - tvz0) * invDyFull;
        double doL = (ooz1 - ooz0) * invDyTop, doR = (ooz2 - ooz0) * invDyFull;

        int yStart = std::max(y0, 0);
        int yEnd = std::min(y1, m_screenHeight);

        for (int y = yStart; y < yEnd; ++y)
        {
            double t = static_cast<double>(y - y0);
            int xL = static_cast<int>(x0 + dxL * t);
            int xR = static_cast<int>(x0 + dxR * t);
            double zL = z0 + dzL * t, zR = z0 + dzR * t;
            double tuL = tuz0 + dtuL * t, tuR = tuz0 + dtuR * t;
            double tvL = tvz0 + dtvL * t, tvR = tvz0 + dtvR * t;
            double ooL = ooz0 + doL * t, ooR = ooz0 + doR * t;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); std::swap(tuL, tuR); std::swap(tvL, tvR); std::swap(ooL, ooR); }
            drawScanline(y, xL, xR, zL, zR, tuL, tvL, ooL, tuR, tvR, ooR, tri.texId, tri.color);
        }
    }

    // 下半部分：y1 → y2
    if (y2 > y1)
    {
        double invDyBot = 1.0 / static_cast<double>(y2 - y1);
        double invDyFull = 1.0 / static_cast<double>(y2 - y0);
        double dxL = static_cast<double>(x2 - x0) * invDyFull;
        double dxR = static_cast<double>(x2 - x1) * invDyBot;
        double dzL = (z2 - z0) * invDyFull;
        double dzR = (z2 - z1) * invDyBot;
        double dtuL = (tuz2 - tuz0) * invDyFull, dtuR = (tuz2 - tuz1) * invDyBot;
        double dtvL = (tvz2 - tvz0) * invDyFull, dtvR = (tvz2 - tvz1) * invDyBot;
        double doL = (ooz2 - ooz0) * invDyFull, doR = (ooz2 - ooz1) * invDyBot;

        int yStart = std::max(y1, 0);
        int yEnd = std::min(y2, m_screenHeight);

        for (int y = yStart; y < yEnd; ++y)
        {
            double tFull = static_cast<double>(y - y0);
            double tBot = static_cast<double>(y - y1);
            int xL = static_cast<int>(x0 + dxL * tFull);
            int xR = static_cast<int>(x1 + dxR * tBot);
            double zL = z0 + dzL * tFull, zR = z1 + dzR * tBot;
            double tuL = tuz0 + dtuL * tFull, tuR = tuz1 + dtuR * tBot;
            double tvL = tvz0 + dtvL * tFull, tvR = tvz1 + dtvR * tBot;
            double ooL = ooz0 + doL * tFull, ooR = ooz1 + doR * tBot;
            if (xL > xR) { std::swap(xL, xR); std::swap(zL, zR); std::swap(tuL, tuR); std::swap(tvL, tvR); std::swap(ooL, ooR); }
            drawScanline(y, xL, xR, zL, zR, tuL, tvL, ooL, tuR, tvR, ooR, tri.texId, tri.color);
        }
    }
}

void Renderer::drawScanline(int y, int x0, int x1, double z0, double z1,
    double tu0, double tv0, double ooz0, double tu1, double tv1, double ooz1,
    int texId, COLORREF color)
{
    if (y < 0 || y >= m_screenHeight) return;

    // 裁剪到屏幕边界，同时修正插值参数
    if (x0 < 0)
    {
        int origX1 = x1; if (origX1 <= x0) return;
        double t = (0.0 - x0) / (double) (origX1 - x0);
        z0 += (z1 - z0) * t;
        tu0 += (tu1 - tu0) * t;
        tv0 += (tv1 - tv0) * t;
        ooz0 += (ooz1 - ooz0) * t;
        x0 = 0;
    }
    if (x1 > m_screenWidth)
    {
        int origX0 = x0; if (x1 <= origX0) return;
        double t = (double) (m_screenWidth - origX0) / (double) (x1 - origX0);
        z1 = z0 + (z1 - z0) * t;
        tu1 = tu0 + (tu1 - tu0) * t;
        tv1 = tv0 + (tv1 - tv0) * t;
        ooz1 = ooz0 + (ooz1 - ooz0) * t;
        x1 = m_screenWidth;
    }
    if (x0 >= x1) return;

    int rowBase = y * m_screenWidth;
    double invDx = 1.0 / static_cast<double>(x1 - x0);
    double dz = (z1 - z0) * invDx;
    double dtu = (tu1 - tu0) * invDx;
    double dtv = (tv1 - tv0) * invDx;
    double doo = (ooz1 - ooz0) * invDx;

    for (int x = x0; x < x1; ++x)
    {
        double t = static_cast<double>(x - x0);
        double z = z0 + dz * t;
        int idx = rowBase + x;
        if (z < m_zbuf[idx])
        {
            m_zbuf[idx] = z;
            if (texId >= 0)
            {
                double oo = ooz0 + doo * t;
                double tu = (tu0 + dtu * t) / oo;
                double tv = (tv0 + dtv * t) / oo;
                m_pBits[idx] = sampleTexture(texId, tu, tv);
            }
            else
                m_pBits[idx] = color;
        }
    }
}

// ============================================================================
// GUI：背景捕获与高斯模糊
// ============================================================================

void Renderer::captureBackground()
{
    if (!m_dibReady) return;

    int total = m_screenWidth * m_screenHeight;
    m_background.resize(total);
    for (int i = 0; i < total; ++i)
        m_background[i] = m_pBits[i];
    m_backgroundReady = true;
}

void Renderer::applyGaussianBlur()
{
    if (!m_backgroundReady) return;

    int w = m_screenWidth, h = m_screenHeight;
    std::vector<DWORD> temp(w * h);

    // 7-tap binomial kernel: [1, 6, 15, 20, 15, 6, 1] / 64
    // 4 次完整迭代 → 等效于 ~25-tap 核，sigma ≈ 3.5，强模糊
    constexpr int K = 7;
    constexpr int weights[K] = { 1, 6, 15, 20, 15, 6, 1 };
    constexpr int radius = K / 2;
    constexpr int ITERATIONS = 4;

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        // ---- 水平模糊 ----
        for (int y = 0; y < h; ++y)
        {
            int rowBase = y * w;
            for (int x = 0; x < w; ++x)
            {
                int sumR = 0, sumG = 0, sumB = 0;
                int weightSum = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    DWORD c = m_background[rowBase + sx];
                    int wt = weights[k + radius];
                    sumR += GetRValue(c) * wt;
                    sumG += GetGValue(c) * wt;
                    sumB += GetBValue(c) * wt;
                    weightSum += wt;
                }
                sumR /= weightSum;
                sumG /= weightSum;
                sumB /= weightSum;
                temp[rowBase + x] = RGB(sumR, sumG, sumB);
            }
        }

        // ---- 垂直模糊 ----
        for (int y = 0; y < h; ++y)
        {
            int rowBase = y * w;
            for (int x = 0; x < w; ++x)
            {
                int sumR = 0, sumG = 0, sumB = 0;
                int weightSum = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    DWORD c = temp[sy * w + x];
                    int wt = weights[k + radius];
                    sumR += GetRValue(c) * wt;
                    sumG += GetGValue(c) * wt;
                    sumB += GetBValue(c) * wt;
                    weightSum += wt;
                }
                sumR /= weightSum;
                sumG /= weightSum;
                sumB /= weightSum;
                m_background[rowBase + x] = RGB(sumR, sumG, sumB);
            }
        }
    }
}

void Renderer::drawBackground()
{
    if (!m_backgroundReady || !m_dibReady) return;
    int total = m_screenWidth * m_screenHeight;
    for (int i = 0; i < total; ++i)
        m_pBits[i] = m_background[i];
}

void Renderer::flushToScreen()
{
    if (!m_dibReady) return;
    HDC hdc = GetImageHDC();
    BitBlt(hdc, 0, 0, m_screenWidth, m_screenHeight, m_memDC, 0, 0, SRCCOPY);
}

// ============================================================================
// GUI：图片绘制
// ============================================================================

void Renderer::drawImageCentered(IMAGE *img)
{
    if (!img || !m_dibReady) return;

    DWORD *buf = GetImageBuffer(img);
    int iw = img->getwidth();
    int ih = img->getheight();
    if (!buf || iw <= 0 || ih <= 0) return;

    int ox = (m_screenWidth - iw) / 2;
    int oy = (m_screenHeight - ih) / 2;

    for (int y = 0; y < ih; ++y)
    {
        int py = oy + y;
        if (py < 0 || py >= m_screenHeight) continue;
        int srcRow = y * iw;
        int dstRow = py * m_screenWidth;
        for (int x = 0; x < iw; ++x)
        {
            int px = ox + x;
            if (px < 0 || px >= m_screenWidth) continue;
            DWORD c = buf[srcRow + x];
            // 跳过全透明（黑色背景也算——如果图片有 alpha，EasyX 会预乘到黑色）
            // 这里简单跳过纯黑像素，适用于 Minecraft 风格 GUI
            if (c == 0 || c == RGB(0, 0, 0)) continue;
            m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
        }
    }
}

// ============================================================================
// GUI：按钮绘制
// ============================================================================

void Renderer::drawButton(int x, int y, int w, int h,
    IMAGE *imgNormal, IMAGE *imgHover, IMAGE *imgActive,
    const wchar_t *text, bool hovered, bool pressed)
{
    if (!m_dibReady) return;

    IMAGE *useImg = imgNormal;
    if (pressed && imgActive) useImg = imgActive;
    else if (hovered && imgHover) useImg = imgHover;

    if (useImg)
    {
        // 贴图已用 loadimage 预缩放到按钮大小，1:1 复制
        DWORD *buf = GetImageBuffer(useImg);
        int iw = useImg->getwidth();
        int ih = useImg->getheight();
        if (buf && iw > 0 && ih > 0)
        {
            int copyW = iw < w ? iw : w;
            int copyH = ih < h ? ih : h;
            for (int dy = 0; dy < copyH; ++dy)
            {
                int py = y + dy;
                if (py < 0 || py >= m_screenHeight) continue;
                int dstRow = py * m_screenWidth;
                int srcRow = dy * iw;
                for (int dx = 0; dx < copyW; ++dx)
                {
                    int px = x + dx;
                    if (px < 0 || px >= m_screenWidth) continue;
                    DWORD c = buf[srcRow + dx];
                    if (c == 0 || c == RGB(0, 0, 0)) continue;
                    m_pBits[dstRow + px] = alphaBlend(m_pBits[dstRow + px], c);
                }
            }
        }
    }
    else
    {
        // 无贴图时绘制纯色按钮
        COLORREF bg = pressed ? RGB(80, 80, 80) : (hovered ? RGB(140, 140, 140) : RGB(100, 100, 100));
        for (int dy = 0; dy < h; ++dy)
        {
            int py = y + dy;
            if (py < 0 || py >= m_screenHeight) continue;
            int dstRow = py * m_screenWidth;
            for (int dx = 0; dx < w; ++dx)
            {
                int px = x + dx;
                if (px < 0 || px >= m_screenWidth) continue;
                m_pBits[dstRow + px] = bg;
            }
        }
        // 边框
        COLORREF border = hovered ? RGB(255, 255, 255) : RGB(180, 180, 180);
        for (int dx = 0; dx < w; ++dx)
        {
            if (y >= 0 && y < m_screenHeight) m_pBits[y * m_screenWidth + (x + dx)] = border;
            int by = y + h - 1;
            if (by >= 0 && by < m_screenHeight) m_pBits[by * m_screenWidth + (x + dx)] = border;
        }
        for (int dy = 0; dy < h; ++dy)
        {
            if (y + dy >= 0 && y + dy < m_screenHeight) m_pBits[(y + dy) * m_screenWidth + x] = border;
            int bx = x + w - 1;
            if (bx >= 0 && bx < m_screenWidth) m_pBits[(y + dy) * m_screenWidth + bx] = border;
        }
    }

    // 绘制文字（使用大号 Minecraft AE 字体）
    if (text && text[0])
    {
        int tx = x + w / 2;
        int ty = y + h / 2;
        HFONT oldFont = m_hFontLarge ? (HFONT) SelectObject(m_memDC, m_hFontLarge) : nullptr;
        SetBkMode(m_memDC, TRANSPARENT);
        SetTextColor(m_memDC, RGB(255, 255, 255));
        SIZE textSize;
        GetTextExtentPoint32W(m_memDC, text, (int) wcslen(text), &textSize);
        TextOutW(m_memDC, tx - textSize.cx / 2, ty - textSize.cy / 2, text, (int) wcslen(text));
        if (oldFont)
            SelectObject(m_memDC, oldFont);
    }
}
