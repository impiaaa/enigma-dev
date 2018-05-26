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

#include "yyp.h"

#define RAPIDJSON_HAS_STDSTRING 1

#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

using CppType = google::protobuf::FieldDescriptor::CppType;

using namespace buffers;
using namespace buffers::resources;

namespace yyp {
std::ostream out(nullptr);
std::ostream err(nullptr);

namespace {

std::vector<std::string> SplitString(const std::string &str, char delimiter) {
  std::vector<std::string> vec;
  std::stringstream sstr(str);
  std::string tmp;
  while (std::getline(sstr, tmp, delimiter)) vec.push_back(tmp);

  return vec;
}

} // Anonymous namespace

void PackRes(std::string &dir, int id, rapidjson::Value::ValueType &node, google::protobuf::Message *m, int depth) {
  const google::protobuf::Descriptor *desc = m->GetDescriptor();
  const google::protobuf::Reflection *refl = m->GetReflection();
  for (int i = 0; i < desc->field_count(); i++) {
    const google::protobuf::FieldDescriptor *field = desc->field(i);
    const google::protobuf::OneofDescriptor *oneof = field->containing_oneof();
    if (oneof && refl->HasOneof(*m, oneof)) continue;
    const google::protobuf::FieldOptions opts = field->options();

    if (field->name() == "id") {
      id += opts.GetExtension(buffers::id_start);
      out << "Setting " << field->name() << " (" << field->type_name() << ") as " << id << std::endl;
      refl->SetInt32(m, field, id);
    } else {
      const std::string yypName = opts.GetExtension(buffers::yyp);
      const std::string gmxName = opts.GetExtension(buffers::gmx);
      std::string alias = yypName;
      bool isSplit = false;
      const bool isFilePath = opts.GetExtension(buffers::file_path);
      rapidjson::Value::ValueType &child = node;

      if (yypName == "YYP_DEPRECATED") {
        continue;
      } else if (yypName.empty() && gmxName != "GMX_DEPRECATED") {
        alias = gmxName;
      }

      if (alias.empty()) alias = field->name();

      // this is for 0,0 crap
      const std::string splitMarker = "YYP_SPLIT/";
      size_t splitPos = alias.find(splitMarker);
      isSplit = splitPos != std::string::npos;

      // if it's not a split then we deal with yoyo's useless nesting
      if (!isSplit && alias != "EGM_NESTED") {  // and our useless nesting
        if (!child.HasMember(alias)) {
          continue;
        }
        child = child[alias];
      }

      if (field->is_repeated()) {  // Repeated fields (are usally messages or file_paths(strings)
        out << "Appending (" << field->type_name() << ") to " << field->name() << std::endl;

        switch (field->cpp_type()) {
          //TODO: Handle repeated fields which are likely JSON arrays

          /* I don't code options for repeated fields other than messages and strings because we don't need them atm
           * BUT incase someone tries to add one to a proto in the future I added this warning to prevent the reader
           * from exploding and gives them a warning of why their shit don't work and a clue where to implement a fix */
          default: {
            err << "Error: missing condition for repeated type: " << field->type_name()
                          << ". Instigated by: " << field->type_name() << std::endl;
            break;
          }
        }
      } else {  // Now we parse individual proto fields to individual json fields (and attributes)
        std::string splitValue;

        if (isSplit) { // if data use a comma delimiter eg. (0,7,9)
          std::vector<std::string> split = SplitString(node.GetString(), ',');
          splitValue = split[static_cast<int>(alias.back()) - '0'];
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        child.Accept(writer);

        std::string value = (isSplit) ? splitValue : buffer.GetString();
        out << "Setting " << field->name() << " (" << field->type_name() << ") as " << value << std::endl;

        switch (field->cpp_type()) {
          // If field is a singular message we need to recurse into this method again
          case CppType::CPPTYPE_MESSAGE: {
            google::protobuf::Message *msg = refl->MutableMessage(m, field);
            PackRes(dir, 0, child, msg, depth + 1);
            break;
          }
          case CppType::CPPTYPE_INT32: {
            refl->SetInt32(m, field, (isSplit) ? std::stoi(splitValue) : child.GetInt());
            break;
          }
          case CppType::CPPTYPE_INT64: {
            refl->SetInt64(m, field, (isSplit) ? std::stoi(splitValue) : child.GetInt64());
            break;
          }
          case CppType::CPPTYPE_UINT32: {
            refl->SetUInt32(m, field, (isSplit) ? std::stoi(splitValue) : child.GetUint());
            break;
          }
          case CppType::CPPTYPE_UINT64: {
            refl->SetUInt64(m, field, (isSplit) ? std::stoi(splitValue) : child.GetUint64());
            break;
          }
          case CppType::CPPTYPE_DOUBLE: {
            refl->SetDouble(m, field, (isSplit) ? std::stod(splitValue) : child.GetDouble());
            break;
          }
          case CppType::CPPTYPE_FLOAT: {
            refl->SetFloat(m, field, (isSplit) ? std::stof(splitValue) : child.GetFloat());
            break;
          }
          case CppType::CPPTYPE_BOOL: {
            refl->SetBool(m, field, (isSplit) ? (std::stof(splitValue) != 0) : (child.GetBool()));
            break;
          }
          case CppType::CPPTYPE_ENUM: {
            refl->SetEnum(
              m, field,
              field->enum_type()->FindValueByNumber(
                (isSplit) ? std::stoi(splitValue) : child.GetInt()));
            break;
          }
          case CppType::CPPTYPE_STRING: {
            std::string value = (isSplit) ? splitValue : child.GetString();
            if (isFilePath) {  // again gotta prepend the yyp's path & fix the string to be posix compatible
              //value = YYPPath2FilePath(dir, value);
            }
            refl->SetString(m, field, value);
            break;
          }
        }
      }
    }
  }
}

buffers::Project *LoadYYP(std::string fName) {
  std::ifstream ifs(fName);
  if (!ifs) {
    err << "Could not open YYP for reading: " << fName << std::endl;
    return nullptr;
  }
  std::string yypDir = fName.substr(0, fName.find_last_of("/\\\\") + 1);
  rapidjson::IStreamWrapper isw(ifs);
  rapidjson::Document doc;
  doc.ParseStream(isw);

  const auto &resourcesArray = doc["resources"].GetArray();
  std::unordered_map<std::string, const rapidjson::Value::ConstObject> treeMap;
  for (const auto &res : resourcesArray) {
    const auto &key = res["Key"].GetString();
    const auto &value = res["Value"].GetObject();
    treeMap.insert(std::make_pair(key, value));
  }

  std::vector<TreeNode*> roots;
  std::unordered_map<std::string, TreeNode*> nodes;
  std::vector<std::pair<TreeNode*, std::vector<std::string> > > parents;
  std::unordered_map<TreeNode::TypeCase, int> idCount;
  for (const auto &treeEntry : treeMap) {
    const auto &key = treeEntry.first;
    const auto &value = treeEntry.second;
    const auto &resourcePath = value["resourcePath"].GetString();
    const auto &resourceType = value["resourceType"].GetString();

    const std::string nodePath = yypDir + resourcePath;
    std::ifstream ifsNode(nodePath);
    if (!ifsNode) {
      err << "Could not open YYP resource for reading: " << nodePath << std::endl;
      continue;
    }
    rapidjson::IStreamWrapper iswNode(ifsNode);
    rapidjson::Document nodeDoc;
    nodeDoc.ParseStream(iswNode);

    TreeNode* node = new TreeNode();
    nodes[key] = node;
    if (strcmp(resourceType, "GMFolder") == 0) {
      node->set_folder(true);
      node->set_name(nodeDoc["folderName"].GetString());
      const auto filterType = nodeDoc["filterType"].GetString();
      if (strcmp(filterType, "root") == 0)
        roots.push_back(node);
      const auto &children = nodeDoc["children"];
      const auto &childrenArray = children.GetArray();
      std::vector<std::string> childrenVector;
      childrenVector.resize(childrenArray.Size());
      for (const auto &child : childrenArray) {
        childrenVector.emplace_back(child.GetString());
      }
      parents.push_back(std::make_pair(node, childrenVector));
    } else {
      node->set_folder(false);
      node->set_name(nodeDoc["name"].GetString());

      using FactoryFunction = std::function<google::protobuf::Message *(TreeNode*)>;
      using FactoryMap = std::unordered_map<std::string, FactoryFunction>;

      static const FactoryMap factoryMap({
        { "GMSound",      &TreeNode::mutable_sound      },
        { "GMTileSet",    &TreeNode::mutable_background },
        { "GMSprite",     &TreeNode::mutable_sprite     },
        { "GMShader",     &TreeNode::mutable_shader     },
        { "GMScript",     &TreeNode::mutable_script     },
        { "GMFont",       &TreeNode::mutable_font       },
        { "GMObject",     &TreeNode::mutable_object     },
        { "GMTimeline",   &TreeNode::mutable_timeline   },
        { "GMRoom",       &TreeNode::mutable_room       },
        { "GMPath",       &TreeNode::mutable_path       }
      });

      auto createFunc = factoryMap.find(resourceType);
      if (createFunc != factoryMap.end()) {
        auto *res = createFunc->second(node);
        PackRes(yypDir, idCount[node->type_case()]++, nodeDoc, res, 0);
      } else {
        err << "Unsupported resource type: " << resourceType << " " << node->name() << std::endl;
      }
    }
  }

  if (roots.empty()) {
    err << "The project contained no root tree nodes and could not finish loading" << std::endl;
    return nullptr;
  }

  for (const auto &parent : parents) {
    TreeNode* node = parent.first;
    const auto &children = parent.second;
    for (const auto &child : children) {
      const auto &key = child;
      const auto &childNodeIt = nodes.find(key);
      if (childNodeIt == nodes.end()) {
        err << "Could not find child '" << key << "'" << std::endl;
        continue;
      }
      TreeNode *childNode = childNodeIt->second;
      node->mutable_child()->AddAllocated(childNode);
    }
  }

  buffers::Project *proj = new buffers::Project();
  buffers::Game *game = proj->mutable_game();
  game->set_allocated_root(roots[0]);

  return proj;
}

}  //namespace yyp