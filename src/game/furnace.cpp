#include "furnace.h"

FurnaceManager::FurnaceManager() {}

bool FurnaceManager::update(FurnaceManager::State &st, double dt)
{
    if (dt <= 0.0) return st.active;

    if (!st.active)
    {
        if (st.inputType == BLOCK_AIR || st.inputCount <= 0) return false;
        if (smeltResult(st.inputType) == BLOCK_AIR) return false;
        if (st.fuelType == BLOCK_AIR || st.fuelCount <= 0) return false;
        double fv = fuelValue(st.fuelType);
        if (fv <= 0.0) return false;

        st.active = true;
        st.burnProgress = 0.0;
        st.burnTimeRemain += fv;
        st.fuelCapacity += fv;

        st.fuelCount--;
        if (st.fuelCount <= 0) { st.fuelType = BLOCK_AIR; st.fuelCount = 0; }
    }

    if (st.active)
    {
        constexpr double SMELT_TIME = 10.0;
        st.burnTimeRemain -= dt / SMELT_TIME;

        bool hasInput = (st.inputType != BLOCK_AIR && st.inputCount > 0 &&
            smeltResult(st.inputType) != BLOCK_AIR);
        bool outputFull = (st.outputType != BLOCK_AIR && st.outputCount > 0 &&
            st.outputType != smeltResult(st.inputType));

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