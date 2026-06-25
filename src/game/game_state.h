#pragma once

/**
 * @brief 游戏整体状态机
 *
 * 定义在独立头文件中，供 game_screens.cpp 等模块使用。
 */
enum class GameState
{
    Login,         // 登录页（选择模式）
    Gameplay,      // 正常游戏
    Inventory,     // 背包界面（E 键打开，2×2 合成）
    CraftingTable, // 工作台界面（右键工作台打开，3×3 合成）
    Furnace,       // 熔炉界面（右键熔炉打开）
    Paused         // 暂停菜单（Esc 键打开）
};
