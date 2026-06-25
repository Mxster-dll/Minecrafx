#pragma once

int terrainHeight(int x, int z, int w);

void generateSurvivalWorld(class World &world, int MX, int MZ, int MW);

void generateCreativeWorld(class World &world, int CX, int CZ, int CW);

void initSurvivalInventory(class Inventory &inv);

void initCreativeInventory(class Inventory &inv);