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

//return 1 if something is different
int compareStrings(char *first, char* second, int n){
    int i;
    for(i = 0; i < n; i++){
        if(first[i] != second[i]){
            return 1;
        }
    }
    return 0;
}

//fills an array with the name
//use this instead of strncpy
void fill_name(uint16_t name_len, char *fill, char *name){
    int i;
    for(i = 0; i < name_len; i++){
        fill[i] = name[i];
    }
    fill[i] = '\0';
}

//prints the name
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


//returns D if directory else F
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

    if(type & 4){
        return 'D';
    }
    else{
        return 'F';
    }

}



//sorts a 2d array of strings
void sort_strings(char list[FILES_PER_DIR][FILE_NAME_SIZE], int size){

    char temp[100];

    int j, i;
    for(j = 0; j < size - 1; j++){
        for(i = j + 1; i < size; i++ ){
            if(strcmp(list[j], list[i]) > 0){
                strcpy(temp, list[j]);
                strcpy(list[j], list[i]);
                strcpy(list[i], temp);
            }
        }
    }
}


//returns the inode number of the file we are looking for specified by name
//returns 0 if name is not found
uint32_t get_inode_num(char *name, uint32_t initial_inode){
    uint8_t inode[128];
    uint8_t dir_entry[FILE_NAME_SIZE + 9];
    struct ext2_inode *temp;
    struct ext2_dir_entry *dir;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(initial_inode, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(initial_inode,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    //cast it as an inode struct
    temp = (struct ext2_inode *)inode;

    uint32_t block = 0;
    uint32_t offset = 0;
    uint32_t total = 0;
    uint32_t data_block = temp->i_block[block] * 2;
    char file_name[FILE_NAME_SIZE];
    uint32_t inode_num;

    while((data_block)!= 0){
        offset = 0;


        while( total < 1024){

            read_data(data_block, offset, dir_entry, 100);
            dir = (struct ext2_dir_entry *)dir_entry;

            fill_name(dir->name_len, file_name, dir->name);
            //printf("file name: %s\n",file_name);
            if(strncmp(file_name, name, dir->name_len + 1) == 0){
                //printf("inode: %d\n", dir->inode);
                inode_num = dir->inode;
                return inode_num;
            }

            total += dir->rec_len;
            if(offset + dir->rec_len > 511){
                data_block += 1;
                offset = offset + dir->rec_len - 512;
            }
            else{
                offset += dir->rec_len;
            }
            if(offset >= 512){
                break;
            }
            // read_data(data_block, offset, dir_entry, 100);
            // dir = (struct ext2_dir_entry *)dir_entry;
        }
        block+=1;
        data_block = temp->i_block[block] * 2;
    }
    return 0;
}




//prints information given name and initial inode
void print_one_directory(char *name, uint32_t initial_inode){
    uint8_t inode[128];
    uint8_t dir_entry[FILE_NAME_SIZE + 9];
    struct ext2_inode *temp;
    struct ext2_dir_entry *dir;

    // get the number of inodes_per_group
    uint32_t s_inodes_per_group = get_inodes_per_group();

    //get the block group that the inode is in (root : 2) should be 0
    uint32_t block_group = get_block_group(initial_inode, s_inodes_per_group);

    //get the inode table for this block_group
    uint32_t inode_table = get_inode_table(block_group);

    //index of the inode that we want. should be 2
    uint32_t index = get_index(initial_inode,s_inodes_per_group);

    //get the block that contains our inode
    uint32_t containing_block = get_containing_block(index);

    //read data into inode array
    read_data((inode_table + block_group * 8192) * 2 + containing_block,
        INODE_SIZE * (index % 4), inode, INODE_SIZE);

    //cast it as an inode struct
    temp = (struct ext2_inode *)inode;

    int block = 0;
    int offset = 0;
    int total = 0;
    int size;
    uint32_t data_block = temp->i_block[block] * 2;
    // data_block = temp->i_block[1] * 2;
    // printf("data_block: %d", data_block);

    uint32_t inode_num;
    //printf("name: %s\n", name);

    while((data_block )!= 0){
        offset = 0;
        total = 0;
        // read_data(data_block, offset, dir_entry, 100);
        // dir = (struct ext2_dir_entry *)dir_entry;


        while( total < 1024){
            char file_name[FILE_NAME_SIZE];
            read_data(data_block, offset, dir_entry, 100);
            dir = (struct ext2_dir_entry *)dir_entry;

            fill_name(dir->name_len, file_name, dir->name);


            //printf("strncmp: %d\n",strncmp(file_name, name, dir->name_len + 1));

            //printf("file_name: %s data_block: %d\n",file_name, data_block);
            if(strncmp(file_name, name, dir->name_len + 1) == 0){
                //printf("file name: %s  name: %s name len: %d\n", file_name, name, dir->name_len);
                inode_num = dir->inode;
                char file_type = get_file_type(inode_num);
                uint32_t file_size = get_file_size(inode_num);
                if(file_type == 'D'){
                    file_size = 0;
                }
                printf("%-20s %-20d %-20c \n", name, file_size, file_type);
                //printf("%-30s %-30d %-30c %d\n", name, file_size, file_type, inode_num);
                return;

            }
            // if(compareStrings(file_name,dir->name,dir->name_len + )){
            //     inode_num = dir->inode;
            // }
            total += dir->rec_len;
            if(offset + dir->rec_len > 511){
                data_block += 1;
                offset = offset + dir->rec_len - 512;
            }
            else{
                offset += dir->rec_len;
            }
            if(offset >= 512){
                break;
            }

        }
        block+=1;
        // printf("data_block: %d\n", data_block);
        data_block = temp->i_block[block] * 2;
        // printf("data_block: %d\n", data_block);
        //data_block = 34742;
        // printf("data_block: %d\n", data_block);

    }





}


//this prints out all of the information of files inside a directory
void print_directory_entries(uint32_t inode_number){
    uint8_t inode[128];
    uint8_t dir_entry[FILE_NAME_SIZE + 9];
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

    //printf("inode: %d\n",inode_number);
    if(get_file_type(inode_number) == 'F'){
        printf("Not a directory.\n");
        exit(0);
    }

    printf("%s %20s %20s\n" , "name" , "size" , "type");



    uint32_t block = 0;
    uint32_t total = 0;
    uint32_t offset = 0;
    uint32_t file_ind = 0;
    uint32_t data_block = temp->i_block[0] * 2;
    char files[FILES_PER_DIR][FILE_NAME_SIZE];

    while((data_block )!= 0){

        offset = 0;
        total = 0;
        while(total < 1024){
            //printf("offset: %d\n",offset);
            read_data(data_block, offset, dir_entry, 100);
            dir = (struct ext2_dir_entry *)dir_entry;

            //fill the name on to the 2d array of names
            fill_name(dir->name_len , files[file_ind++], dir->name);
            // strncpy(files[file_ind++],dir->name, dir->name_len);
            // printf("offset: %d\n",offset);
            // printf("dir->inode: %d\n",dir->inode);
            // printf("dir->name_le : %d\n",dir->name_len);
            // printf("dir->dir_len: %d\n",dir->rec_len);
            // printf("file name: %s\n", files[file_ind - 1]);
            // printf("file ind.: %d\n", file_ind - 1);
            //increment the total bytes read in the block
            total += dir->rec_len;

            // printf("name :%s\n",files[file_ind-1]);
            // printf("offset: %d rec_len: %d total: %d  inode: %d name: %s\n",offset, dir->rec_len, total,dir->inode, files[file_ind - 1]);

            if(offset + dir->rec_len > 511){

                data_block += 1;
                offset = offset + dir->rec_len - 512;
            }
            else{

                offset += dir->rec_len;
            }
            if(offset >= 511){
                break;
            }


            //printf("offset: %d rec_len: %d total: %d  inode: %d name: %s\n",offset, dir->rec_len, total,dir->inode, files[file_ind - 1]);



        }


        block+=1;
        //printf("block %d\n",block);
        data_block = temp->i_block[block] * 2;



    }
    sort_strings(files,file_ind);

    int i;
    for(i = 0; i < file_ind ; i++){
        //printf("files: %s\n",files[i]);
        print_one_directory(files[i], inode_number);

    }
    //exit(0);
}

//prints everything in the directory given a path and number of elements in the path
void print_path_directories(char path[PATH_LENGTH][FILE_NAME_SIZE], int size){
    int inode = 2;

    int i;
    for(i =0; i < size ; i++){

        inode = get_inode_num(path[i], inode);

    }

    print_directory_entries(inode);

}
