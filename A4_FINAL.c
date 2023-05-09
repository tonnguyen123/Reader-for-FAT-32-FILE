#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "fat32.h"
#include <unistd.h>
#include <ctype.h>


/*Struct for FSInfo*/
struct FSInfo_str{
    uint32_t FSI_LeadSig;
    char FSI_Reserved1[480];
    uint32_t FSI_StructSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint32_t FSI_Reserved2;
    uint32_t FSI_TrailSig;
};
typedef struct FSInfo_str FSInfo;


/*Struct for Fat32Dir*/
struct fat32Dir_str{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate; 
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
};
typedef struct fat32Dir_str fat32Dir;


/*Struct to keep a list of Folders or file*/

struct simple_Dir{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
    fat32Dir list[10000000];
};
typedef struct simple_Dir simpleDir;



/*Struct for either file or system just containg the name only*/
struct FILE{
    char name [1000];
};
typedef struct FILE file;

/*Buffer to store all information about all folders*/
fat32Dir allFolders[1000000];
int actualsize;
int arrayPos = 0;

fat32Dir chosenFolder;


typedef uint32_t DWORD;

fat32BootSector FAT32sys;

FSInfo fat32FSI;

simpleDir Volume;


int RootDir_Secs;
int FirstDatSec;
int FirstSecOfClus;
int DatSec;


int FATstart;

int CountOfClus;

int FATsz;
int totalSec;

uint16_t signal;

uint32_t freeClus;


/*function to calculate the first sector of the clsuter*/
int firstSecInClus(int N){
    return ((N - 2) * FAT32sys.BPB_SecPerClus) + FirstDatSec;
}


/*Caculate the position on FAT*/
int fatOffset(int N){
    return N * 4;
}

/*This is to cacluate the fat sec number*/
int thisFatSecN(int N){
    int thisFATSecNum = FAT32sys.BPB_RsvdSecCnt + (fatOffset(N)/FAT32sys.BPB_BytesPerSec);
    return thisFATSecNum;
}
/*This is to cacluate the fat offset*/
int thisFATEOff(int N){
    int thisFATEnOffset = fatOffset(N)%FAT32sys.BPB_BytesPerSec;
    return thisFATEnOffset;
}



/*This function is called by a recursive function below to print out info of Directory*/
void printDirectory(int start, int fd){
        char file[12];
        
       
        int i;
        uint8_t att;
       int skip = lseek(fd,start,SEEK_SET);
       int rd = read(fd,&file,11*sizeof(char));
       if(skip == -1){
           printf("error lseek()\n");
       }
       int skipOff = start;
       while(skipOff <= start + FAT32sys.BPB_BytesPerSec){
           

            skip = lseek(fd,0,SEEK_CUR);
            rd = read(fd,&att,sizeof(uint8_t));
            if(rd == -1){
                printf("error reading file\n");
            }
            if(att != 0x0F && att != 0x02){

                
                if(file[0] != '.' && file[0] != 0x00 && file[8] != ' ' && file[9] != ' ' && file[10] != ' '){
                    printf("--");
                    for(i = 0; i < (int)sizeof(file); i++){
                        if(i < 8){
                            if(file[i] != ' '){
                                
                                printf("%c",file[i]);
                                if(i == 7){
                                    printf(".");
                                }
                            }
                            else if(i == 7){
                                if(file[i] == ' '){
                                    printf(".");
                                }
        
                            }
                        }

                        else{
                            printf("%c",file[i]);
                        }
                    }
                    printf("\n");
                }
            }
           
       skipOff += 32;
       skip = lseek(fd,skipOff,SEEK_SET);
       rd = read(fd,&file,11*sizeof(char));
       }
       
}





/*This function is to calculate position of the sector in the disk*/
int sector_in_byte(int sector){
    int offsetByte = sector * FAT32sys.BPB_BytesPerSec;
    return offsetByte;
}


void GotoClus(int firstClus,int FD){  /*Using recursive to print out all files in one folder*/
    if(firstClus >= 0x0FFFFFF8){
        printf("End of the directory\n");
    }

    else{
    int nextClus;
    int currentClus;
    currentClus = firstClus;

    

    int firstSector = firstSecInClus(firstClus);  //first sector 1107
    //printf("first sector is %d\n", firstSector);
    int SecToOffset = sector_in_byte(firstSector);
    //printf("first offset is %d\n", SecToOffset);
    printDirectory(SecToOffset,FD);

    /*-----------------------------------*/

    int distance = currentClus * 4;   //position in fat table
    int NextClusterOffset = FATstart + distance;
   

    int skip = lseek(FD,NextClusterOffset,SEEK_SET); 
    int rd = read(FD,&nextClus,4*sizeof(uint32_t)); 
    
    if(rd == -1){
        printf("error reading\n");
    }
    if(skip == -1){
        printf("error lseek()\n");
    }
    currentClus = nextClus;  //cluster 8010
    currentClus = currentClus &0x0FFFFFFF;
    firstSector = firstSecInClus(currentClus);   //get first sector of 8010

    
    SecToOffset = sector_in_byte(firstSector); //convert 1st sector of cluster 8010 to offset
   
    printDirectory(SecToOffset,FD);
    GotoClus(currentClus,FD);

    }

    

}


void info(int fd){
    /*load all information of Boot Sector*/
    int rd = 0;

    int skip = lseek(fd,3,SEEK_SET);
    rd = read(fd, &FAT32sys.BS_OEMName[0], 8 * sizeof(char));

    if(rd == -1){
                printf("error reading file\n");
            }

    if(skip == -1){
        printf("error lseek()\n");
    }


   
    rd = read(fd, &FAT32sys.BPB_BytesPerSec, 2 * sizeof(uint16_t));
  //good

    

    skip = lseek(fd,13,SEEK_SET);
    

    rd = read(fd, &FAT32sys.BPB_SecPerClus, 1 * sizeof(uint8_t));

      //good 

    skip = lseek(fd,14,SEEK_SET);

    rd = read(fd, &FAT32sys.BPB_RsvdSecCnt, 2 * sizeof(uint16_t));

     //good

    skip = lseek(fd,16,SEEK_SET);

    rd = read(fd, &FAT32sys.BPB_NumFATs, 1 * sizeof(uint8_t));
    //good

    skip = lseek(fd,32,SEEK_SET);
    rd = read(fd, &FAT32sys.BPB_TotSec32, 4 * sizeof(uint32_t));
    

     totalSec = FAT32sys.BPB_TotSec32;
     
    skip = lseek(fd,36,SEEK_SET);

    rd = read(fd, &FAT32sys.BPB_FATSz32, 4 * sizeof(uint32_t));
     //good

    skip = lseek(fd,71,SEEK_SET);
    read(fd, &FAT32sys.BS_VolLab[0], 11 * sizeof(char));
   


    skip = lseek(fd,44,SEEK_SET);
    rd = read(fd, &FAT32sys.BPB_RootClus, 4 * sizeof(uint32_t));
     

    /*For root directory, N = FAT32sys.BPB_RootClus*/

      skip = lseek(fd,71,SEEK_SET);
    rd = read(fd, &FAT32sys.BS_VolLab[0], 11 * sizeof(char));
    

    RootDir_Secs = 0;   /*RootDirSectors*/
    FirstDatSec = FAT32sys.BPB_RsvdSecCnt + (FAT32sys.BPB_NumFATs * FAT32sys.BPB_FATSz32) + RootDir_Secs;

    FATsz = FAT32sys.BPB_FATSz32;

   

    DatSec = totalSec - (FAT32sys.BPB_RsvdSecCnt + (FAT32sys.BPB_NumFATs * FATsz) + RootDir_Secs);

    CountOfClus = DatSec/FAT32sys.BPB_SecPerClus;

   FATstart = thisFatSecN(2) * FAT32sys.BPB_BytesPerSec;
   
   

   int FSIAddress = FAT32sys.BPB_BytesPerSec * FAT32sys.BPB_FSInfo + 488;



    

    skip = lseek(fd,FSIAddress,SEEK_SET);
    rd = read(fd, &fat32FSI.FSI_Free_Count , 4 * sizeof(uint32_t));

}

void listAll(int fd, int print){
    /*Print all files and directories in the volume*/

    
    char DIR_Name[12];

    uint8_t att;

   
    int lowFirstClus;
    int skip;
    int rd;


    

    int OffsetRootStart = FirstDatSec * FAT32sys.BPB_BytesPerSec;

    int startPos = OffsetRootStart;
    
    

    skip = lseek(fd,OffsetRootStart, SEEK_SET);

    rd = read(fd,&DIR_Name[0],11*sizeof(char));

    if(rd == -1){
                printf("error reading file\n");
            }

    if(skip == -1){
        printf("error lseek()\n");
    }

    int endPos = startPos + FAT32sys.BPB_BytesPerSec;
    

    while(startPos <= endPos){
        if(DIR_Name[0] != (char) 0XE5 && DIR_Name[0] != (char)0xFFFFFFE5){
            

            skip = lseek(fd,0,SEEK_CUR);
            rd = read(fd,&att,sizeof(uint8_t));

            int highPos = startPos + 20;
            skip = lseek(fd,highPos,SEEK_SET);
            rd = read(fd,&highPos,2 *sizeof(uint16_t));
        

            int lowPos = startPos + 26;
            

            skip = lseek(fd,lowPos,SEEK_SET);
            rd = read(fd,&lowFirstClus,2 *sizeof(uint16_t));

            int FD = fd;

            if(att == 0x08){
                if(print == 1){
                    printf("Volume ID is %s\n", DIR_Name);
                }
                
                
            }
            else if(att == 0x10){
                if(print == 1){
                    printf("-Directory is %s \n", DIR_Name);
                }
                

                fat32Dir Directory;
                stpncpy(Directory.DIR_Name,&DIR_Name[0], 8);
                Directory.DIR_Name[8] = '\0';
                Directory.DIR_Attr = att;
                Directory.DIR_FstClusLO = lowFirstClus;
                Directory.DIR_FstClusHI = highPos;
              
                Volume.list[arrayPos] = Directory;
                if(print == 1){
                     GotoClus(lowFirstClus,FD);
                }
               
                arrayPos++;
            }
            else if(att != 0x02 && strcmp(DIR_Name,"") != 0 && isspace(DIR_Name[0])){
                fat32Dir Directory;
                stpncpy(Directory.DIR_Name,&DIR_Name[0], 8);
                Directory.DIR_Name[8] = '\0';
                Directory.DIR_Attr = att;
                Directory.DIR_FstClusLO = lowFirstClus;
                Directory.DIR_FstClusHI = highPos;
                if(print == 1){
                     printf("-The file is %s\n", DIR_Name);
                }
               
                Volume.list[arrayPos] = Directory;

            }
            
        }
        startPos += 32;
        skip = lseek(fd,startPos, SEEK_SET);
       rd = read(fd,&DIR_Name[0],11*sizeof(char));
    }
    startPos = 0; /*Reset the start position for the start*/
}

/*Check this is a File or Folder*/
int isAfile(char *a){
    int result = 0;
    int i;
    for(i = 0; i < 11; i++){
        if(a[i] == '.'){
            result = 1;
        }
    }
    return result;
}


/*Check if the folder is chosen for 'get' command exists or not*/
int checkFolder(char *b){

    int found = 0;
    int matchChar = 0;
    int k;
    for(k = 0; k < arrayPos && found == 0; k++){
        int j = 0;;
        for(j = 0; j < (int) strlen(Volume.list[k].DIR_Name); j++ ){
            if(b[j] == Volume.list[k].DIR_Name[j]){
            matchChar++;
        }

        else{
            if(matchChar == (int) strlen(b) && Volume.list[k].DIR_Name[j] == ' '){
                found = 1;
                chosenFolder = Volume.list[k];
            }
        }

        }
        
    }
   
        return found;

}


/*This function is to compare 2 file's name to find the position of the file on the disk*/
int compare2String(char *UserInput, char *diskFile){
    

    int DiFiL = (int) strlen(diskFile);
    int UsFiL = (int)strlen(UserInput);

    int found = 0;
    int matchChar = 0;
    int k; 

    for(k = 0; k < DiFiL && found == 0; k++){
        if(UserInput[k] == diskFile[k]){
            matchChar++;
        }
        else{
            if(matchChar == ( UsFiL- 4) && UserInput[k] == '.'){
                if(UserInput[k + 1] == diskFile[DiFiL - 1] && UserInput[k + 2] == diskFile[DiFiL - 2] &&  UserInput[k + 3] == diskFile[DiFiL - 3]){
                    
                    found = 1;
                }
            }
        }
    }
   
    
    return found;

}

/*This function is to write information of the file on the disk to another text file in same directory of the program*/
void writeToFile (char *fileName, int firstCLusint , int fd){
    int FD;
    FD = open(fileName, O_CREAT | O_WRONLY, 0600);

    char content[513];
    int firstSec = firstSecInClus(firstCLusint);
    int PrintStart = firstSec * FAT32sys.BPB_BytesPerSec;
    

    int skip,rd;
    //int found = 0;
    
    skip = lseek(fd,PrintStart,SEEK_SET);
    rd = read(fd,&content,512 * sizeof(char));

     if(rd == -1){
                printf("error reading file\n");
        }

        if(skip == -1){
        printf("error lseek()\n");
    }
    
    write(FD,content,513);


    uint32_t nextCLus;
    int currOffPos = FATstart + firstCLusint * 4;
    skip = lseek(fd,currOffPos,SEEK_SET);
    rd = read(fd,&nextCLus,4 * sizeof(uint32_t));
    uint32_t curr = nextCLus;

    while(nextCLus < 0x0FFFFFF8){
        firstSec = firstSecInClus(nextCLus);
        PrintStart = firstSec * FAT32sys.BPB_BytesPerSec;
        skip = lseek(fd,PrintStart,SEEK_SET);
        rd = read(fd,&content,512 * sizeof(char));
         write(FD,content,512);

        currOffPos = FATstart + curr * 4;

        skip = lseek(fd,currOffPos,SEEK_SET);
        rd = read(fd,&nextCLus,4 * sizeof(uint32_t));
        curr = nextCLus;

    }
    
}


/*This function is to read a sector*/
void findFileAndPrint(int clusterN, int fd, char *fileName){
    char currentFile[12];

    int firstSec = firstSecInClus(clusterN);
    int startPos = FAT32sys.BPB_BytesPerSec * firstSec;
    int endPos = startPos + FAT32sys.BPB_BytesPerSec;
    

    uint16_t lowClus;

    int skip,rd;
    int found = 0;
    
    skip = lseek(fd,startPos,SEEK_SET);
    rd = read(fd,&currentFile,11 * sizeof(char));

    int skipOff = startPos;
    while(skipOff <= endPos && found != 1){
        skip = lseek(fd,skipOff + 26,SEEK_SET);
        rd = read(fd,&lowClus,2*sizeof(uint16_t));

    
        if(rd == -1){
                printf("error reading file\n");
        }

        if(skip == -1){
        printf("error lseek()\n");
        }

        if(compare2String(fileName,currentFile) == 1){
           
            found = 1;
            writeToFile(fileName,lowClus,fd);
        }
        else{
            skipOff += 32;
            skip = lseek(fd,skipOff,SEEK_SET);
            rd = read(fd,&currentFile,11*sizeof(char));
        }


    }

}

int main(int argc, char *argv[]){
    (void) argc;
    
    int fd = open(argv[1], O_RDONLY);
    
    info(fd);

    if(strcmp(argv[2],"info") == 0){
        
        printf("Drive name is %s\n",FAT32sys.BS_VolLab);
        printf("Free space volume in kB is %d kB\n", fat32FSI.FSI_Free_Count * FAT32sys.BPB_BytesPerSec * FAT32sys.BPB_SecPerClus/1024);
        printf("Each cluster has %d sector and each cluster equals to %d bytes\n", FAT32sys.BPB_SecPerClus, FAT32sys.BPB_SecPerClus * FAT32sys.BPB_BytesPerSec);
        int usable = (FAT32sys.BPB_TotSec32 * FAT32sys.BPB_BytesPerSec - (512 + FAT32sys.BPB_NumFATs * FAT32sys.BPB_FATSz32 * FAT32sys.BPB_BytesPerSec + FAT32sys.BPB_RsvdSecCnt * FAT32sys.BPB_BytesPerSec))/1024;
        printf("Usable Storage in kB is %d\n", usable);
    }


     else if(strcmp(argv[2],"list") == 0){
         
        listAll(fd,1);
    }

    else if(strcmp(argv[2],"get") == 0){
        listAll(fd,0);

        file PathInfo[100];

        char *token;
        int count = 0;
        token = strtok(argv[3],"/");
        while(token != NULL){
        file temp;
        strcpy(temp.name,token);

        strcpy(PathInfo[count].name, temp.name);     
        count++;
        token = strtok(NULL,"/n");
        }

        int m;
        file Folders[10];
        file FILES[1];
        file addFile;
        int c = 0;
        for(m = 0 ; m < count; m++){
            if(isAfile(PathInfo[m].name) == 1){
                
                strcpy(addFile.name, PathInfo[m].name);
                FILES[0] = addFile;
            }
            else{
                strcpy(addFile.name, PathInfo[m].name);
                Folders[c] = addFile;
                 c++;
            }
        }
        c = 0;
       

       int exist = checkFolder(Folders[0].name);
       
       if(exist == 0){
           printf("infomation typed in is not correct. Please try again\n");
       }
       else{
           
           findFileAndPrint(chosenFolder.DIR_FstClusLO,fd,FILES[0].name);
	  printf("The file is written in the same folder\n");

       }
       

    }
    close(fd);

}
