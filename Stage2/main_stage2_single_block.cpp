#include "Buffer/BlockBuffer.h"
#include "Disk_Class/Disk.h"

#include <cstdio>
#include <cstring>

int main(int argc, char *argv[]) {
    Disk disk_run;

    // Objects representing the catalog record blocks.
    RecBuffer relCatBuffer(RELCAT_BLOCK);
    RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

    HeadInfo relCatHeader;
    HeadInfo attrCatHeader;

    // Read the headers of both catalog blocks.
    relCatBuffer.getHeader(&relCatHeader);
    attrCatBuffer.getHeader(&attrCatHeader);

    // Read every relation catalog record.
    for (int i = 0; i < relCatHeader.numEntries; i++) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];

        relCatBuffer.getRecord(relCatRecord, i);

        printf(
            "Relation: %s\n",
            relCatRecord[RELCAT_REL_NAME_INDEX].sVal
        );

        // Search the attribute catalog for this relation's attributes.
        for (int j = 0; j < attrCatHeader.numEntries; j++) {
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

            attrCatBuffer.getRecord(attrCatRecord, j);

            if (strcmp(
                    attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                    relCatRecord[RELCAT_REL_NAME_INDEX].sVal
                ) == 0) {

                const char *attrType =
                    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER
                        ? "NUM"
                        : "STR";

                printf(
                    "  %s: %s\n",
                    attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                    attrType
                );
            }
        }

        printf("\n");
    }

    return 0;
}