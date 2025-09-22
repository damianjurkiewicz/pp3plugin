#include "pch.h"
#include "EntityExtender.h"

struct ExtendedData
{
	bool m_bProcessed = false;
	CEntity* m_pEntity = nullptr;
	bool m_bShown = false;
	bool m_bIgnore = false;

	ExtendedData(CEntity* obj) {};
};

enum VisibilityFlags
{
	ALWAYS_VISIBLE,
	VISIBLE_BROKEN,
	VISIBLE_LIFTED
};

class ProjectProps
{
private:
        inline static CSimpleIniA ini;
        inline static CSimpleIniA::TNamesDepend sections;
        inline static EntityExtender<ExtendedData> xData;
        static int GetIniInt(const char* section, const char* key, int defaultValue);
        static float GetIniFloat(const char* section, const char* key, float defaultValue);
        static bool IsManagedObjectValid(const CObject* object);
        static void ResetManagedObjectState(ExtendedData& data);

public:
        ProjectProps();
        static void ProcessDynamicObject(CObject* pObj);
	static void ProcessStaticObject(CBuilding* pBuilding);
	static void PlaceRelativeBuilding(CEntity* tar, CEntity* src, CVector off);

} pp;