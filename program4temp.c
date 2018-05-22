#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>

//global filepointer for the ext2 file
extern FILE *fp;



int main(int argc, char *argv[]){

    //open the file as a binary file
    if((fp = fopen("testimage.ext2", "rb")) == NULL){
        printf("error opening file \n");
        exit(1);
    }


    // create a super_block struct
    struct ext2_super_block *super_block = (struct ext2_super_block *)malloc(
        sizeof(struct ext2_super_block));




    //read data onto the super_block struct
    read_data(2, 0, (uint8_t *)super_block, sizeof(struct ext2_super_block));


    //first case: no extra arguments
    //print all files in the root directory (inode 2)
    if(argc == 2){
        //initialize a group_desc struct
        struct ext2_group_desc *group_desc = (struct ext2_group_desc *)malloc(
            sizeof(struct ext2_group_desc));

        //get the block_group index for the root inode (2)
        uint32_t block_group = get_block_group(EXT2_ROOT_INO, super_block);

        //we must get the block_group_descriptor for this particular block_group
        uint32_t block_group_descriptor = get_block_group_desc(block_group);

        //use read_data?
        read_data(block_group_descriptor, 0, (uint8_t*)group_desc,
            sizeof(struct ext2_group_desc));

        //index of the inode that we want (root: 2)
        uint32_t index = get_index(EXT2_ROOT_INO, super_block);

        //set the inode struct to the beginning of the inode table
        struct ext2_inode *inode = (struct ext2_inode *)malloc(
            sizeof(struct ext2_inode));

        //get the right inode into the struct
        read_data(10  , 128 , (uint8_t *)inode, sizeof(struct ext2_inode));
        // printf("%x", inode->i_mode);

    }

    //printf(" %d\n", );




    return 0;
}
