#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // OpenRelTable::openRel() returns the rel-id on success.
  // Valid rel-ids are non-negative; error codes are negative.
  if (ret >= 0) {
    return SUCCESS;
  }

  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  // RELATIONCAT and ATTRIBUTECAT must always remain open.
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
      strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // Get the rel-id if the relation is currently open.
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(
    char oldRelName[ATTR_SIZE],
    char newRelName[ATTR_SIZE]
) {

    // System catalogs cannot be renamed or used as a new relation name
    if (
        strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0
    ) {
        return E_NOTPERMITTED;
    }

    // Schema can be modified only when the relation is closed
    if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Actual catalog modification is handled by BlockAccess
    return BlockAccess::renameRelation(oldRelName, newRelName);
}

int Schema::renameAttr(
    char relName[ATTR_SIZE],
    char oldAttrName[ATTR_SIZE],
    char newAttrName[ATTR_SIZE]
) {

    // Attributes of system catalogs cannot be renamed
    if (
        strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0
    ) {
        return E_NOTPERMITTED;
    }

    // Relation must be closed before altering its schema
    if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Actual attribute catalog modification happens below
    return BlockAccess::renameAttribute(
        relName,
        oldAttrName,
        newAttrName
    );
}