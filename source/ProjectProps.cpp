#include "pch.h"
#include "ProjectProps.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#if LOGGING
#include <fstream>
std::ofstream lg;
#define LOG(x) lg << x << std::endl;
#else
#define LOG(x)
#endif

namespace
{
        // GTA:SA exposes 20000 streaming slots; accessing beyond that corrupts memory.
        constexpr int kMaxStreamingModelIndex = 20000;

        bool HasInvalidTrailingCharacters(char* endPtr)
        {
                if (endPtr == nullptr)
                {
                        return false;
                }

                while (*endPtr != '\0')
                {
                        if (!std::isspace(static_cast<unsigned char>(*endPtr)))
                        {
                                return true;
                        }
                        ++endPtr;
                }

                return false;
        }
}

ProjectProps::ProjectProps()
{
        static bool bInit = true;

        Events::processScriptsEvent += []()
	{
		if (bInit)
		{
			lg.open("ProjectProps.log");

			SI_Error rc = ini.LoadFile("ProjectProps.ini");
			if (rc < 0)
			{
				LOG("ProjectProps.ini not found.")
				return;
			}

			ini.GetAllSections(sections);
			bInit = false;
		}

		for (CObject* pObj : CPools::ms_pObjectPool)
		{
			ProcessDynamicObject(pObj);
		}
        };
}

int ProjectProps::GetIniInt(const char* section, const char* key, int defaultValue)
{
        const char* rawValue = ini.GetValue(section, key, nullptr);

        if (!rawValue || rawValue[0] == '\0')
        {
                return defaultValue;
        }

        char* endPtr = nullptr;
        long parsed = std::strtol(rawValue, &endPtr, 10);

        if (endPtr == rawValue || HasInvalidTrailingCharacters(endPtr)
                || parsed < std::numeric_limits<int>::min()
                || parsed > std::numeric_limits<int>::max())
        {
                LOG("Invalid integer value '" << rawValue << "' for [" << section << "] " << key << ". Using default " << defaultValue);
                return defaultValue;
        }

        return static_cast<int>(parsed);
}

float ProjectProps::GetIniFloat(const char* section, const char* key, float defaultValue)
{
        const char* rawValue = ini.GetValue(section, key, nullptr);

        if (!rawValue || rawValue[0] == '\0')
        {
                return defaultValue;
        }

        char* endPtr = nullptr;
        float parsed = std::strtof(rawValue, &endPtr);

        if (endPtr == rawValue || HasInvalidTrailingCharacters(endPtr) || !std::isfinite(parsed))
        {
                LOG("Invalid float value '" << rawValue << "' for [" << section << "] " << key << ". Using default " << defaultValue);
                return defaultValue;
        }

        return parsed;
}

bool ProjectProps::IsManagedObjectValid(const CObject* object)
{
        if (!object)
        {
                return false;
        }

        for (CObject* candidate : CPools::ms_pObjectPool)
        {
                if (candidate == object)
                {
                        return true;
                }
        }

        return false;
}

void ProjectProps::ResetManagedObjectState(ExtendedData& data)
{
        data.m_pEntity = nullptr;
        data.m_bProcessed = false;
        data.m_bShown = false;
        data.m_bIgnore = false;
}

void ProjectProps::ProcessDynamicObject(CObject* pObj)
{
        if (!pObj)
        {
                return;
        }

        const std::string modelSection = std::to_string(pObj->m_nModelIndex);
        for (auto it = sections.begin(); it != sections.end(); ++it)
        {
                if (!it->pItem || modelSection != it->pItem)
                {
                        continue;
                }

                ExtendedData& xdata = xData.Get(pObj);

                if (xdata.m_bIgnore)
                {
                        continue;
                }

                int visibleFlagValue = GetIniInt(it->pItem, "visibleFlag", 0);
                if (visibleFlagValue < ALWAYS_VISIBLE || visibleFlagValue > VISIBLE_LIFTED)
                {
                        LOG("Visibility flag out of range for section [" << it->pItem << "] - using default value");
                        visibleFlagValue = ALWAYS_VISIBLE;
                }
                VisibilityFlags visibleFlag = static_cast<VisibilityFlags>(visibleFlagValue);

                if (xdata.m_bProcessed)
                {
                        CObject* managedObject = static_cast<CObject*>(xdata.m_pEntity);
                        if (!IsManagedObjectValid(managedObject))
                        {
                                LOG("Managed object for section [" << it->pItem << "] is no longer valid. Resetting state.");
                                ResetManagedObjectState(xdata);
                                continue;
                        }

                        if (visibleFlag == VISIBLE_BROKEN || visibleFlag == VISIBLE_LIFTED)
                        {
                                if (xdata.m_bShown)
                                {
                                        if ((pObj->m_nObjectFlags.bIsBroken == 0 && visibleFlag == VISIBLE_BROKEN)
                                                || (pObj->bOnSolidSurface == 0 && visibleFlag == VISIBLE_LIFTED))
                                        {
                                                managedObject->Remove();
                                                xdata.m_bShown = false;
                                        }
                                }
                                else
                                {
                                        if ((pObj->m_nObjectFlags.bIsBroken == 1 && visibleFlag == VISIBLE_BROKEN)
                                                || (pObj->bOnSolidSurface == 1 && visibleFlag == VISIBLE_LIFTED))
                                        {
                                                managedObject->Add();
                                                xdata.m_bShown = true;
                                        }
                                }
                        }
                }
                else
                {
                        int requestModel = GetIniInt(it->pItem, "model", -1);

                        if (requestModel < 0)
                        {
                                xdata.m_bIgnore = true;
                                continue;
                        }

                        if (requestModel >= kMaxStreamingModelIndex)
                        {
                                LOG("Model index " << requestModel << " for section [" << it->pItem << "] exceeds streaming limits. Skipping entry.");
                                xdata.m_bIgnore = true;
                                continue;
                        }

                        float offsetX = GetIniFloat(it->pItem, "X", 0.0f);
                        float offsetY = GetIniFloat(it->pItem, "Y", 0.0f);
                        float offsetZ = GetIniFloat(it->pItem, "Z", 0.0f);
                        float rotation = GetIniFloat(it->pItem, "rot", 0.0f);

                        int chanceValue = GetIniInt(it->pItem, "chance", 100);
                        if (chanceValue <= 0)
                        {
                                LOG("Chance value for section [" << it->pItem << "] must be positive. Using default value of 100.");
                                chanceValue = 100;
                        }

                        int chance = std::max(0, 100 / chanceValue);
                        int magicNum = rand() % (chance + 1);

                        if (magicNum != 1)
                        {
                                xdata.m_bIgnore = true;
                                continue;
                        }

                        CStreaming::RequestModel(requestModel, eStreamingFlags::PRIORITY_REQUEST);
                        CStreaming::LoadAllRequestedModels(true);

                        if (CStreaming::ms_aInfoForModel[requestModel].m_nLoadState != eStreamingLoadState::LOADSTATE_LOADED)
                        {
                                continue;
                        }

                        CObject* createdObject = CObject::Create(requestModel);
                        if (!createdObject)
                        {
                                LOG("Failed to create object for section [" << it->pItem << "] with model " << requestModel << ".");
                                continue;
                        }

                        xdata.m_pEntity = createdObject;
                        CObject::PlacePhysicalRelativeToOtherPhysical(pObj, createdObject, CVector(offsetX, offsetY, offsetZ));
                        createdObject->SetHeading(rotation);

                        if (visibleFlag == VISIBLE_BROKEN || visibleFlag == VISIBLE_LIFTED)
                        {
                                createdObject->Remove();
                        }
                        else
                        {
                                xdata.m_bShown = true;
                        }

                        CStreaming::SetModelIsDeletable(requestModel);
                        xdata.m_bProcessed = true;
                }
        }
}
