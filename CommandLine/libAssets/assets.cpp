/** Copyright (C) 2018 Robert B. Colton
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/

#include "assets.h"

#include <zip.h>

#include <map>
#include <vector>

using namespace buffers;
using namespace buffers::resources;
using namespace google::protobuf;

using std::string;
using TypeCase = TreeNode::TypeCase;
using ResourceMap = std::vector<const Message*>;
using TypeMap = std::map<TypeCase, ResourceMap>;

#define irrr(res) if (res) { /*idpr("Error occurred; see scrollback for details.",-1);*/ return res; }

inline void writei(int x, FILE *f) {
  fwrite(&x,4,1,f);
}

static int module_write_sprites(const ResourceMap& map, FILE *gameModule) {
  // Now we're going to add sprites
  //edbg << es->spriteCount << " Adding Sprites to Game Module: " << flushl;

  //Magic Number
  fwrite("SPR ",4,1,gameModule);

  //Indicate how many
  int sprite_count = map.size();
  fwrite(&sprite_count,4,1,gameModule);

  int sprite_maxid = 0;
  for (auto msg : map) {
    auto spr = static_cast<const Sprite*>(msg);
    if (spr->id() > sprite_maxid)
      sprite_maxid = spr->id();
  }
  fwrite(&sprite_maxid,4,1,gameModule);

  for (auto msg : map) {
    auto spr = static_cast<const Sprite*>(msg);
    writei(spr->id(),gameModule); //id

    // Track how many subimages we're copying
    int subCount = spr->subimages_size();

    int swidth = 0, sheight = 0;
    for (int ii = 0; ii < subCount; ii++)
    {
      if (!swidth and !sheight) {
        swidth =  spr->width();
        sheight = spr->height();
      }
      else if (swidth != spr->width() or sheight != spr->height()) {
        //user << "Subimages of sprite `" << es->sprites[i].name << "' vary in dimensions; do not want." << flushl;
        return 14;
      }
    }
    if (!(swidth and sheight and subCount)) {
      //user << "Subimages of sprite `" << es->sprites[i].name << "' have zero size." << flushl;
      return 14;
    }

    writei(swidth,gameModule);            // width
    writei(sheight,gameModule);           // height
    writei(spr->origin_x(),gameModule);    // xorig
    writei(spr->origin_y(),gameModule);    // yorig
    writei(spr->bbox_top(),gameModule);    // BBox Top
    writei(spr->bbox_bottom(),gameModule); // BBox Bottom
    writei(spr->bbox_left(),gameModule);   // BBox Left
    writei(spr->bbox_right(),gameModule);  // BBox Right
    writei(spr->bbox_mode(),gameModule);   // BBox Mode
    writei(spr->shape(),gameModule);       // Mask shape

    writei(subCount,gameModule); //subimages

    for (int ii = 0; ii < subCount; ii++)
    {
      //strans = es->sprites[i].subImages[ii].transColor, fwrite(&idttrans,4,1,exe); //Transparent color
      writei(swidth * sheight * 4,gameModule); //size when unpacked
      //writei(es->sprites[i].subImages[ii].image.dataSize,gameModule); //size when unpacked
      //fwrite(es->sprites[i].subImages[ii].image.data, 1, es->sprites[i].subImages[ii].image.dataSize, gameModule); //sprite data
      writei(0,gameModule);
    }
  }

  //edbg << "Done writing sprites." << flushl;
  return 0;
}

static int module_write_sounds(const ResourceMap& map, FILE *gameModule) {

}

static int module_write_backgrounds(const ResourceMap& map, FILE *gameModule) {

}

static int module_write_fonts(const ResourceMap& map, FILE *gameModule) {

}

static int module_write_paths(const ResourceMap& map, FILE *gameModule) {

}

static void flatten_resources(TypeMap& typeMap, TreeNode* root) {
  for (int i = 0; i < root->child_size(); i++) {
    buffers::TreeNode* child = root->mutable_child(i);
    if (child->has_folder())
      flatten_resources(typeMap, child);
    else
      typeMap[child->type_case()].push_back(child);
  }
}

int game_write_assets(const Game& game, bool exe, const string& gameFname) {
  TypeMap typeMap;

  // because we still need to write something even when they're empty
  // let's just force them to exist
  std::vector<TypeCase> requiredTypes = { TypeCase::kSprite, TypeCase::kSound, TypeCase::kBackground, TypeCase::kFont, TypeCase::kPath };
  for (auto& type : requiredTypes) {
    // insert it
    typeMap[type];
  }

  flatten_resources(typeMap, const_cast<TreeNode*>(&game.root()));

  FILE *gameModule;
  int resourceblock_start = 0;
  if (exe) {
    gameModule = fopen(gameFname.c_str(),"ab");
    if (!gameModule) {
      //user << "Failed to append resources to the game. Did compile actually succeed?" << flushl;
      //idpr("Failed to add resources.",-1);
      return 12;
    }
    fseek(gameModule,0,SEEK_END); //necessary on Windows for no reason.
    resourceblock_start = ftell(gameModule);
    if (resourceblock_start < 128) {
      //user << "Compiled game is clearly not a working module; cannot continue" << flushl;
      //idpr("Failed to add resources.",-1);
      return 13;
    }
  } else {
    gameModule = fopen(gameFname.c_str(),"wb");
    if (!gameModule) {
      //user << "Failed to write resources to compiler-specified file, `" << resname << "`. Write permissions to valid path?" << flushl;
      //idpr("Failed to write resources.",-1);
      return 12;
    }
  }

  // Start by setting off our location with a DWord of NULLs
  fwrite("\0\0\0",1,4,gameModule);
  //idpr("Adding Sprites",90);
  irrr(module_write_sprites(typeMap[TypeCase::kSprite], gameModule));
  //edbg << "Finalized sprites." << flushl;
  //idpr("Adding Sounds",93);
  irrr(module_write_sounds(typeMap[TypeCase::kSound], gameModule));
  irrr(module_write_backgrounds(typeMap[TypeCase::kBackground], gameModule));
  irrr(module_write_fonts(typeMap[TypeCase::kFont], gameModule));
  irrr(module_write_paths(typeMap[TypeCase::kPath], gameModule));
  // Tell where the resources start
  fwrite("\0\0\0\0res0",8,1,gameModule);
  fwrite(&resourceblock_start,4,1,gameModule);
  // Close the game module; we're done adding resources
  //idpr("Closing game module and running if requested.",99);
  //edbg << "Closing game module and running if requested." << flushl;
  fclose(gameModule);
  return 0;
}