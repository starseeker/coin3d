/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Phase 3 Implementation Modernization: Modern C++17 Utility Functions
 * Implementation
 \**************************************************************************/

#include <Inventor/tools/SbModernUtils.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbName.h>
#include <cstring>

using namespace SbModernUtils;

/* ********************************************************************** */

std::optional<SoNode*> 
SbModernUtils::findNodeByName(const SbName & name)
{
    SoNode* node = SoNode::getByName(name);
    if (node) {
        return std::optional<SoNode*>(node);
    }
    return std::nullopt;
}

bool 
SbModernUtils::nameEquals(const SbName & name, std::string_view str)
{
    const char* nameStr = name.getString();
    if (!nameStr) {
        return str.empty();
    }
    
    size_t nameLen = strlen(nameStr);
    return nameLen == str.length() && 
           std::equal(nameStr, nameStr + nameLen, str.begin());
}

/* ********************************************************************** */

SoNodeRef::SoNodeRef(SoNode* node) : node_(node)
{
    if (node_) {
        node_->ref();
    }
}

SoNodeRef::~SoNodeRef()
{
    if (node_) {
        node_->unref();
    }
}

SoNodeRef::SoNodeRef(SoNodeRef&& other) noexcept : node_(other.node_)
{
    other.node_ = nullptr;
}

SoNodeRef& 
SoNodeRef::operator=(SoNodeRef&& other) noexcept
{
    if (this != &other) {
        if (node_) {
            node_->unref();
        }
        node_ = other.node_;
        other.node_ = nullptr;
    }
    return *this;
}

SoNode* 
SoNodeRef::release()
{
    SoNode* result = node_;
    node_ = nullptr;
    return result;
}

SoNodeRef 
SbModernUtils::makeNodeRef(SoNode* node)
{
    return SoNodeRef(node);
}