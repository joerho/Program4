#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext2.h"

FILE *fp;
//the block argument is in terms of SD card 512 byte sectors
void read_data(uint32_t block, uint16_t offset, uint8_t* data, uint16_t size) {
   if (offset > 511) {
      printf ("Offset greater than 511.\n");
      exit(0);
   }

   fseek(fp,block*512 + offset,SEEK_SET);
   fread(data,size,1,fp);
}



//gets the start of the inode table at a given block_group
uint32_t get_inode_table(uint32_t block_group){
    uint8_t group_desc[32];
    uint32_t block_group_desc_number = get_block_group_desc(block_group);
    read_data(block_group_desc_number, 0, group_desc, 32 );
    struct ext2_group_desc *temp;
    temp =  (struct ext2_group_desc *)group_desc;
    return temp->bg_inode_table;
}

//grabs the number of inodes_per_group from the super_block
uint32_t get_inodes_per_group(){
    uint8_t super_block[84];
    read_data(2, 0, super_block, 84);
    struct ext2_super_block *temp;
    temp = (struct ext2_super_block *)super_block;
    return temp->s_inodes_per_group;
}

// From an inode address (remember that they start at 1), we can determine which group the inode is in.
uint32_t get_block_group(uint32_t inode, uint32_t s_inodes_per_group){
    return (inode - 1)/(s_inodes_per_group);
}

//Once we know which group an inode resides in,
//we can look up the actual inode by first retrieving that block group's inode table's starting address
uint32_t get_index(uint32_t inode, uint32_t s_inodes_per_group){
    return (inode - 1) % (s_inodes_per_group);
}

//Next, we have to determine which block contains our inode.
uint32_t get_containing_block(uint32_t index){
    return (index * INODE_SIZE) / (512);
}

// this returns the byte position to the group_desc_table depending on the block_group index
uint32_t get_block_group_desc(uint32_t block_group){
    return 4 + (block_group * 16384);
}


// splits file path into a 2d array
int split_file_path(char *path, char out[PATH_LENGTH][FILE_NAME_SIZE]){
    int i = 0;
    char *p = strtok(path, "/");

    while(p != NULL){
        strcpy(out[i++],p);
        p = strtok(NULL, "/");
    }
    return i;
}


void fill_name(uint16_t name_len, char *fill, char *name){
    int i;
    for(i = 0; i < name_len; i++){
        fill[i] = name[i];
    }
    fill[i] = '\0';
}


void get_name(uint16_t name_len, char *name){
    int i;
    char temp[100];
    for(i = 0; i < name_len; i++){
        temp[i] = name[i];

    }
    temp[i] = '\0';
    printf("%s\n", temp);
}

uint32_t get_file_size(uint32_t inode_number){
    uint8_t inode[128];
    struct ext2_inode *temp;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(inode_number, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(inode_number,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    temp = (struct ext2_inode *)inode;

    uint32_t size = temp->i_size;

    return size;
}


//returns 1 if directory else
char get_file_type(uint32_t inode_number){
    uint8_t inode[128];
    struct ext2_inode *temp;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(inode_number, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(inode_number,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    temp = (struct ext2_inode *)inode;

    uint16_t type = temp->i_mode;

    type = type >> 12;

    if(type == 4){
        return 'D';
    }

    return 'F';
}




void sort_strings(char list[FILES_PER_DIR][FILE_NAME_SIZE], int size){

    char temp[100];


    for(int j = 0; j < size - 1; j++){
        for(int i = j + 1; i < size; i++ ){
            if(strcmp(list[j], list[i]) > 0){
                strcpy(temp, list[j]);
                strcpy(list[j], list[i]);
                strcpy(list[i], temp);
            }
        }
    }
}

//should only work with the root directory
void print_one_directory(char *name){
    uint8_t inode[128];
    uint8_t dir_entry[9];
    struct ext2_inode *temp;
    struct ext2_dir_entry *dir;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(EXT2_ROOT_INO, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(EXT2_ROOT_INO,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    //cast it as an inode struct
    temp = (struct ext2_inode *)inode;

    int block = 0;
    int offset = 0;
    uint32_t data_block ;
    char file_name[FILE_NAME_SIZE];
    uint32_t inode_num;

    while((data_block = temp->i_block[block] * 2 )!= 0){
        offset = 0;
        read_data(data_block, offset, dir_entry, 100);
        dir = (struct ext2_dir_entry *)dir_entry;

        while(dir->inode != 0){

            fill_name(dir->name_len, file_name, dir->name);
            if(strcmp(file_name, name) == 0){
                inode_num = dir->inode;
            }
            offset += dir->rec_len;
            if(offset > 511){
                data_block += 1;
                offset = 0;
            }
            read_data(data_block, offset, dir_entry, 100);
            dir = (struct ext2_dir_entry *)dir_entry;
        }
        block+=1;
    }

    char file_type = get_file_type(inode_num);
    uint32_t file_size = get_file_size(inode_num);

    printf("%-20s %-20d %c\n", name, file_size, file_type);



}


void print_directory_entries(uint32_t inode_number){
    uint8_t inode[128];
    uint8_t dir_entry[9];
    struct ext2_inode *temp;
    struct ext2_dir_entry *dir;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(inode_number, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(inode_number,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    //cast it as an inode struct
    temp = (struct ext2_inode *)inode;

    printf("%s %20s %20s\n" , "name" , "size" , "type");



    int block = 0;
    int offset = 0;
    int file_ind = 0;
    uint32_t data_block ;
    char files[FILES_PER_DIR][FILE_NAME_SIZE];


    while((data_block = temp->i_block[block] * 2 )!= 0){

        offset = 0;
        read_data(data_block, offset, dir_entry, 100);
        dir = (struct ext2_dir_entry *)dir_entry;

        while(dir->inode != 0){

            //strncpy(files[file_ind], dir->name, dir->name_len);
            fill_name(dir->name_len, files[file_ind], dir->name);
            file_ind ++;
            offset += dir->rec_len;
            if(offset > 511){
                data_block += 1;
                offset = 0;
            }
            read_data(data_block, offset, dir_entry, 100);
            dir = (struct ext2_dir_entry *)dir_entry;
        }


        block+=1;
    }


    sort_strings(files,file_ind);
    for(int i = 0; i < file_ind; i++){
        print_one_directory(files[i]);
    }



}
