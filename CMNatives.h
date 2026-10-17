#pragma once
#include "..\CoreSDK\inc\natives.h"

namespace INVENTORY
{
	NATIVE_DECL int _PLAYER_INVENTORY_GET_ITEM_COUNT(int inventoryId, Any* guid, BOOL p2) { return invoke<int>(0xC97E0D2302382211, guid, p2); }
}