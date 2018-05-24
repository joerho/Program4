#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>

extern FILE *fp;


int main(int argc, char *argv[]){

    if((fp = fopen("testimage.ext2", "rb")) == NULL){
        printf("error opening file \n");
        exit(1);
    }



    if(argc == 2){

        print_directory_entries(2);
        //print_directory_entries(15432);

    }
    if(argc == 3){
        char out[PATH_LENGTH][FILE_NAME_SIZE];
        int size;
        size = split_file_path(argv[2], out);
        print_path_directories(out,size);

    }
    if(argc == 4){


    }

    return 1;




}
