//
// TM & (c) 2019 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXRuntime/Private/PvtRelationship.h>

namespace MaterialX
{

RT_DEFINE_RUNTIME_OBJECT(PvtRelationship, RtObjType::RELATIONSHIP, "PvtRelationship")

PvtRelationship::PvtRelationship(const RtToken& name, PvtPrim* parent) :
    PvtObject(name, parent)
{
    setTypeBit<PvtRelationship>();
}

void PvtRelationship::addTarget(const PvtObject* target)
{
    // Linear search not ideal for performance, but we need the relationship ordering
    // so must maintain a vector here.
    // If performance ever gets noticable we could add and extra set/map, 
    // if we can affor the extra storage.
    for (auto it = _targets.begin(); it != _targets.end(); ++it)
    {
        if (it->get() == target)
        {
            // Relationship already exists
            return;
        }
    }
    _targets.push_back(target->hnd());
}

void PvtRelationship::removeTarget(const PvtObject* target)
{
    for (auto it = _targets.begin(); it != _targets.end(); ++it)
    {
        if (it->get() == target)
        {
            _targets.erase(it);
            break;
        }
    }
}

}
