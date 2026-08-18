#include "Buffer/BlockBuffer.h"
#include "Disk_Class/Disk.h"

#include <cstdio>
#include <cstring>

int main() {
    // Initialize access to the NITCbase disk.
    Disk disk_run;

    // Start searching from the first Attribute Catalog block.
    int attrCatBlockNum = ATTRCAT_BLOCK;

    bool attributeFound = false;

    while (attrCatBlockNum != -1) {
        RecBuffer attrCatBuffer(attrCatBlockNum);

        HeadInfo attrCatHeader;

        int status = attrCatBuffer.getHeader(&attrCatHeader);

        if (status != SUCCESS) {
            printf(
                "Failed to read Attribute Catalog block %d.\n",
                attrCatBlockNum
            );

            return 1;
        }

        // Search every Attribute Catalog record in this block.
        for (int slotNum = 0;
             slotNum < attrCatHeader.numEntries;
             slotNum++) {

            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

            status = attrCatBuffer.getRecord(
                attrCatRecord,
                slotNum
            );

            if (status != SUCCESS) {
                printf(
                    "Failed to read slot %d from block %d.\n",
                    slotNum,
                    attrCatBlockNum
                );

                return 1;
            }

            const char *relationName =
                attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal;

            const char *attributeName =
                attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal;

            // Find the Students.Class Attribute Catalog record.
            if (strcmp(relationName, "Students") == 0 &&
                strcmp(attributeName, "Class") == 0) {

                // Change the attribute name in memory.
                strcpy(
                    attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                    "Batch"
                );

                // Write the modified record back to disk.
                status = attrCatBuffer.setRecord(
                    attrCatRecord,
                    slotNum
                );

                if (status != SUCCESS) {
                    printf("Failed to rename the attribute.\n");
                    return 1;
                }

                printf(
                    "Attribute renamed successfully: "
                    "Students.Class -> Students.Batch\n"
                );

                attributeFound = true;
                break;
            }
        }

        if (attributeFound) {
            break;
        }

        // Move to the next linked Attribute Catalog block.
        attrCatBlockNum = attrCatHeader.rblock;
    }

    if (!attributeFound) {
        printf(
            "Attribute Students.Class was not found.\n"
        );
        return 1;
    }

    return 0;
}