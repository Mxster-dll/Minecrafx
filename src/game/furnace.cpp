#include "furnace.h"

FurnaceManager::FurnaceManager() {}

bool FurnaceManager::update(FurnaceManager::State &st, double dt)
{
    if (dt <= 0.0) return st.active;

    // 未激活：检查是否可以开始烧制
    if (!st.active)
    {
        if (st.inputType == BLOCK_AIR || st.inputCount <= 0) return false;
        if (smeltResult(st.inputType) == BLOCK_AIR) return false;
        if (st.fuelType == BLOCK_AIR || st.fuelCount <= 0) return false;
        double fv = fuelValue(st.fuelType);
        if (fv <= 0.0) return false;

        // 开始烧制
        st.active = true;
        st.burnProgress = 0.0;
        st.burnTimeRemain += fv;
        st.fuelCapacity += fv;
        // 消耗一个燃料
        st.fuelCount--;
        if (st.fuelCount <= 0) { st.fuelType = BLOCK_AIR; st.fuelCount = 0; }
    }

    // 正在燃烧（burnTimeRemain 始终消耗，burnProgress 仅有原料时推进）
    if (st.active)
    {
        constexpr double SMELT_TIME = 10.0;
        st.burnTimeRemain -= dt / SMELT_TIME;  // 燃料始终消耗

        bool hasInput = (st.inputType != BLOCK_AIR && st.inputCount > 0 &&
            smeltResult(st.inputType) != BLOCK_AIR);
        bool outputFull = (st.outputType != BLOCK_AIR && st.outputCount > 0 &&
            st.outputType != smeltResult(st.inputType));

        // 仅当输入有效且输出格未满（可堆叠同种除外）时推进烧制
        if (hasInput && !outputFull)
        {
            st.burnProgress += dt / SMELT_TIME;

            if (st.burnProgress >= 1.0)
            {
                int result = smeltResult(st.inputType);
                if (st.outputType == BLOCK_AIR || st.outputType == result)
                {
                    st.outputType = result;
                    st.outputCount++;
                }
                st.inputCount--;
                if (st.inputCount <= 0) { st.inputType = BLOCK_AIR; st.inputCount = 0; }
                st.burnProgress -= 1.0;
            }
        }

        // 燃料耗尽（输出格满时不自动续燃料）
        if (st.burnTimeRemain <= 0.0)
        {
            if (!outputFull && st.fuelType != BLOCK_AIR && st.fuelCount > 0)
            {
                double fv = fuelValue(st.fuelType);
                if (fv > 0.0)
                {
                    st.burnTimeRemain += fv;
                    st.fuelCapacity += fv;
                    st.fuelCount--;
                    if (st.fuelCount <= 0) { st.fuelType = BLOCK_AIR; st.fuelCount = 0; }
                }
            }
            if (st.burnTimeRemain <= 0.0)
            {
                st.active = false;
                st.burnProgress = 0.0;
                return false;
            }
        }

        return true;
    }

    return false;
}
