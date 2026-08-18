#include "Buffer/BlockBuffer.h"
#include "Disk_Class/Disk.h"

#include <cstdio>
#include <cstring>

int main() {
    // Initializes access to the NITCbase disk.
    Disk disk_run;

    // The first block of the Relation Catalog.
    RecBuffer relCatBuffer(RELCAT_BLOCK);

    HeadInfo relCatHeader;

    // Read the Relation Catalog block header.
    int status = relCatBuffer.getHeader(&relCatHeader);

    if (status != SUCCESS) {
        printf("Failed to read Relation Catalog header.\n");
        return 1;
    }

    // Read every relation stored in the Relation Catalog.
    for (int i = 0; i < relCatHeader.numEntries; i++) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];

        status = relCatBuffer.getRecord(relCatRecord, i);

        if (status != SUCCESS) {
            printf(
                "Failed to read Relation Catalog record at slot %d.\n",
                i
            );
            return 1;
        }

        const char *currentRelationName =
            relCatRecord[RELCAT_REL_NAME_INDEX].sVal;

        printf("Relation: %s\n", currentRelationName);

        /*
         * Start from the first Attribute Catalog block.
         *
         * Each block header contains rblock, which points to the
         * next Attribute Catalog block.
         */
        int attrCatBlockNum = ATTRCAT_BLOCK;

        while (attrCatBlockNum != -1) {
            // Represents the current Attribute Catalog block.
            RecBuffer attrCatBuffer(attrCatBlockNum);

            HeadInfo attrCatHeader;

            status = attrCatBuffer.getHeader(&attrCatHeader);

            if (status != SUCCESS) {
                printf(
                    "Failed to read Attribute Catalog block %d.\n",
                    attrCatBlockNum
                );
                return 1;
            }

            // Read every attribute record in the current block.
            for (int j = 0; j < attrCatHeader.numEntries; j++) {
                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

                status = attrCatBuffer.getRecord(attrCatRecord, j);

                if (status != SUCCESS) {
                    printf(
                        "Failed to read slot %d from "
                        "Attribute Catalog block %d.\n",
                        j,
                        attrCatBlockNum
                    );
                    return 1;
                }

                const char *attributeRelationName =
                    attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal;

                /*
                 * Print the attribute only when it belongs to
                 * the relation currently being processed.
                 */
                if (strcmp(
                        attributeRelationName,
                        currentRelationName
                    ) == 0) {

                    const char *attributeType =
                        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal
                                == NUMBER
                            ? "NUM"
                            : "STR";

                    printf(
                        "  %s: %s\n",
                        attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                        attributeType
                    );
                }
            }

            // Follow the link to the next Attribute Catalog block.
            attrCatBlockNum = attrCatHeader.rblock;
        }

        printf("\n");
    }

    return 0;
}