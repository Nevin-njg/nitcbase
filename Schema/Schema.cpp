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