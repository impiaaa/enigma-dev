#ifndef ASSET_INDEX_H
#define ASSET_INDEX_H

#include "AssetEnum.h"

#include <string>
#include <map>

namespace enigma_user {

#ifdef JUST_DEFINE_IT_RUN
int asset_get_index(std::string resource, enigma_user::AssetType type);
int asset_get_index(std::string resource);
#else
int asset_get_index(std::string resource, enigma_user::AssetType type = asset_any);
#endif
int asset_get_type(std::string resource);

}

namespace enigma {

extern std::map<enigma_user::AssetType, std::map<std::string, int>> assetMap;

}

#endif
