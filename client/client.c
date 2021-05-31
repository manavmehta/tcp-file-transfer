#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <dirent.h> 
#include <sys/stat.h>

#define MAXLINE 4096
#define BACKLOGS 1024
#define TRUE 1
#define FALSE 0
#define u16 uint16_t // unsigned 16 bit int
#define li long int
#define LS "ls"
#define GET "get"
#define QUIT "quit"
#define HISTORY "history"

#define GRN  "\x1B[32m"
#define RED  "\x1B[31m"
#define WHT   "\x1B[37m"
#define BLU  "\x1B[34m"
#define YEL  "\x1B[33m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define RESET "\x1B[0m"


// removes leading and trailing whitespaces [✔]
void rm_lt_spaces(char *str){
    int left = 0, right = strlen(str) - 1;

    while (isspace((unsigned char) str[left]))
        left++;

    while ((right >= left) && isspace((unsigned char) str[right]))
        right--;

	int i;

    for (i = left; i <= right; i++)
        str[i - left] = str[i];

    str[i - left] = '\0';
    
    return;
}

// function to get size of the file. [✔]
// assumption :: st_size actually gives off_t which is mostly long int. easier to print li than off_t which needs typecasting
li findSize(const char *file_name) { 
    struct stat st;

    if(stat(file_name, &st)==0) return (st.st_size);

    else return -1;
}

// prompt [✔]
int get_prompt(char *buffer){
    memset(buffer, 0, (int)sizeof(buffer)); 	//clear buffer

    printf(GRN "≈> " RESET);

    //get user input
    if(fgets(buffer, 1024, stdin) == NULL) // fgets(buff, size, stdin) reding from stdin
        return -1;

    return 1;
}

int get_port_str(char *str, char *ip, int n5, int n6){
    int i = 0;
    char ip_temp[1024];
    strcpy(ip_temp, ip);

    for(i = 0; i < strlen(ip); i++){
        if(ip_temp[i] == '.'){
            ip_temp[i] = ',';
        }
    }

    sprintf(str, "PORT %s,%d,%d", ip_temp, n5, n6);
    return 1;
}

// check consecutive spaces
int check_command(char *command){
	int i = 0, len = strlen(command), count = 0;
    bool space = false;

	for(i = 0; i < len; i++){
		if(isspace(command[i]) == 0)
			space = false;

        // else if this char is a space and last was not, count++
        else{
			if(space == false){
				count++;
			}
		}
	}

	if(count < 2) return 1;
	else return -1;
}

// returns cmd_code corresponding to the command entered, stores the entered command into the arg [✔]
int get_cmd_code(char *command){
	int cmd_code;
    bool flag = false;
	char temp_cmd[1024];

	while(!flag){

    	if(get_prompt(command) < 0){ // if NULL command
    		printf(RED "Invalid command.\nPlease try again.\n" RESET);
            memset(command, '\0', (int)sizeof(command)); // clear command
    		continue;
    	}

        if(strlen(command) < 2){ // if not NULL, but unsupported
    		printf(RED "Unsupported command entered.\nPlease try again.\n" RESET);
            memset(command, '\0', (int)sizeof(command)); // clear command
            continue;
        }
        
        rm_lt_spaces(command);
        strcpy(temp_cmd, command);

        if(check_command(temp_cmd) < 0){
            	printf(RED "Unsupported format...\nPlease try again...\n" RESET);
            	memset(command, '\0', (int)sizeof(command));
            	memset(temp_cmd, '\0', (int)sizeof(temp_cmd));
            	continue;
        }
    	
    	
    	char *delimiters = " \t\r\n\v\f";
    	char *str;
        str = strtok(temp_cmd, delimiters);
    	if((strcmp(str, LS) == 0) || (strcmp(str, GET) == 0) || (strcmp(str, QUIT) == 0) || (strcmp(str, HISTORY)) == 0){
    		flag = true;

            // store corresponding command code
            if(strcmp(str, LS) == 0) cmd_code = 1;
            else if(strcmp(str, GET) == 0) cmd_code = 2;
            else if(strcmp(str, QUIT) == 0) cmd_code = 4;
            else if(strcmp(str, HISTORY) == 0) cmd_code = 5;

            // printf("\nEntered command's code = %d\n\n", cmd_code); // debug statement
    	}
        else{
    		printf(RED "Unsupported command entered.\nPlease try again.\n" RESET);
            memset(command, '\0', strlen(command));
           	memset(temp_cmd, '\0', sizeof(temp_cmd));
    		continue;
    	}
    }
	return cmd_code;
}

int convert(u16 port, int *n5, int *n6){
    int i = 0;
    int x = 1;
    *n5 = 0;
    *n6 = 0;
    int temp = 0;
    for(i = 0; i< 8; i++){
        temp = port & x;
        *n6 = (*n6)|(temp);
        x = x << 1; 
    }

    port = port >> 8;
    x = 1;

    for(i = 8; i< 16; i++){
        temp = port & x;
        *n5 = ((*n5)|(temp));
        x = x << 1; 
    }
    return 1;
}

int get_ip_port(int fd, char *ip, int *port){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    getsockname(fd, (struct sockaddr*) &addr, &len);
    sprintf(ip, inet_ntoa(addr.sin_addr));
    *port = (u16)ntohs(addr.sin_port);
    return 1;
}

int get_filename(char *input, char *fileptr){
    char tmp[1024];
    char *filename = NULL;
    strcpy(tmp, input);
    rm_lt_spaces(tmp);
    filename = strtok(tmp, " ");
    filename = strtok(NULL, " ");

    if(filename == NULL){
        fileptr = "\0";
        return -1;
    }
    else{
        strncpy(fileptr, filename, strlen(filename));
        return 1;
    }
}

int _ls(int sockfd, int datafd, char *input){
    
    char filelist[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp;
    memset(filelist, '\0', (int)sizeof(filelist));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));

    fd_set rdset;
    int maxfdp1, data_finished = FALSE, control_finished = FALSE;

    if(get_filename(input, filelist) < 0){
        // printf("No Filelist Detected...\n");
        sprintf(str, "LIST");
    }else{
        sprintf(str, "LIST %s", filelist);
    }

    memset(filelist, '\0', (int)sizeof(filelist));

    FD_ZERO(&rdset);
    FD_SET(sockfd, &rdset);
    FD_SET(datafd, &rdset);

    if(sockfd > datafd){
        maxfdp1 = sockfd + 1;
    }else{
        maxfdp1 = datafd + 1;
    }

    write(sockfd, str, strlen(str));
    while(1){
        if(control_finished == FALSE){FD_SET(sockfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &rdset)){
            read(sockfd, recvline, MAXLINE);
            printf("%s\n", recvline);
            temp = strtok(recvline, " ");
            if(atoi(temp) != 200){
                printf("Exiting...\n");
                break;
            }
            control_finished = TRUE;
            memset(recvline, '\0', (int)sizeof(recvline));
            FD_CLR(sockfd, &rdset);
        }

        if(FD_ISSET(datafd, &rdset)){
            printf(CYN "Response:\n" RESET);
            while(read(datafd, recvline, MAXLINE) > 0){
                printf("%s", recvline); 
                memset(recvline, '\0', (int)sizeof(recvline)); 
            }

            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    memset(filelist, '\0', (int)sizeof(filelist));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));
    return 1;
}

int _history(int sockfd, int datafd, char *input){
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    long int total_size = 0;
    if (d) 
    {
        while ((dir = readdir(d)) != NULL) 
        {
            if(dir->d_name[0]=='o' && dir->d_name[1]=='u' && dir->d_name[2]=='t')
            {
                long int size = findSize(dir->d_name);
                printf("%s    %ld bytes\n", dir->d_name, size);
                total_size+=size;
            }
        }
        closedir(d);
    }
    char filelist[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp;
    memset(filelist, '\0', (int)sizeof(filelist));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));

    fd_set rdset;
    int maxfdp1, data_finished = FALSE, control_finished = FALSE;


    sprintf(str, "LIST");


    memset(filelist, '\0', (int)sizeof(filelist));

    FD_ZERO(&rdset);
    FD_SET(sockfd, &rdset);
    FD_SET(datafd, &rdset);

    if(sockfd > datafd){
        maxfdp1 = sockfd + 1;
    }else{
        maxfdp1 = datafd + 1;
    }

    write(sockfd, str, strlen(str));
    while(1){
        if(control_finished == FALSE){FD_SET(sockfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &rdset)){
            control_finished = TRUE;
            memset(recvline, '\0', (int)sizeof(recvline));
            FD_CLR(sockfd, &rdset);
        }

        if(FD_ISSET(datafd, &rdset)){
            
            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    memset(filelist, '\0', (int)sizeof(filelist));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));
    printf("Total size of data trasferred is %ld bytes.\n", total_size);
    return 1;
}

int _get(int sockfd, int datafd, char *input){
    char filename[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp, temp1[1024];
    memset(filename, '\0', (int)sizeof(filename));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));
    int n = 0, p = 0;

    fd_set rdset;
    int maxfdp1, data_finished = FALSE, control_finished = FALSE;

    

    if(get_filename(input, filename) < 0){
        printf("No filename Detected...\n");
        char send[1024];
        sprintf(send, "SKIP");
        write(sockfd, send, strlen(send));
        memset(send, '\0', (int)sizeof(send));
        read(sockfd, send, 1000);
        printf("Server Response: %s\n", send);
        return -1;
    }else{
        sprintf(str, "RETR %s", filename);
    }   
    printf("File: %s\n", filename);
    sprintf(temp1, "from-server-%s", filename);
    memset(filename, '\0', (int)sizeof(filename));


    FD_ZERO(&rdset);
    FD_SET(sockfd, &rdset);
    FD_SET(datafd, &rdset);


    if(sockfd > datafd){
        maxfdp1 = sockfd + 1;
    }else{
        maxfdp1 = datafd + 1;
    }

    
    FILE *fp;
    if((fp = fopen(temp1, "w")) == NULL){
        perror("file error");
        return -1;
    }

    write(sockfd, str, strlen(str));
    while(1){
        if(control_finished == FALSE){FD_SET(sockfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &rdset)){
            memset(recvline, '\0', (int)sizeof(recvline));
            read(sockfd, recvline, MAXLINE);
            printf("Server Control Response: %s\n", recvline);
            temp = strtok(recvline, " ");
            if(atoi(temp) != 200){
                printf("File Error...\nExiting...\n");
                break;
            }
            control_finished = TRUE;
            memset(recvline, '\0', (int)sizeof(recvline));
            FD_CLR(sockfd, &rdset);
        }

        if(FD_ISSET(datafd, &rdset)){
            //printf("Server Data Response:\n");
            memset(recvline, '\0', (int)sizeof(recvline));
            while((n = read(datafd, recvline, MAXLINE)) > 0){
                fseek(fp, p, SEEK_SET);
                fwrite(recvline, 1, n, fp);
                p = p + n;
                //printf("%s", recvline); 
                memset(recvline, '\0', (int)sizeof(recvline)); 
            }
            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    memset(filename, '\0', (int)sizeof(filename));
    memset(recvline, '\0', (int)sizeof(recvline));
    memset(str, '\0', (int)sizeof(str));
    fclose(fp);
    return 1;
}

// int main(int argc, char **argv){

// 	int server_port;
//     int sockfd, listenfd, datafd;
//     int cmd_code, n5, n6, x;
//     u16 port;
// 	struct sockaddr_in server_addr, data_addr;
// 	char command[1024], ip[50], str[MAXLINE+1];

// 	if(argc != 3){
// 		printf(RED "Invalid number of arguments. Please follow the format.\n");
// 		printf("Format: ./client <server-ip> <server-port>\n" RESET);
// 		exit(-1);
// 	}

// 	//get server port
// 	sscanf(argv[2], "%d", &server_port); // store argv[2] into server_port

//     //set up control connection using sockfd as control socket descriptor
//     if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
//     	perror("socket connection error occured\n");
//     	exit(-1);
//     }
                
//     memset(&server_addr, '\0', sizeof(server_addr));
//     server_addr.sin_family = AF_INET; // address family -> trivially, we use AF_INET.
//     server_addr.sin_port = htons(server_port); // 16 bit port no, conv from network to host byte order

//     // convert host addr into AF_INET addr family as store in server_addr.sin_addr
//     if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1){
//         // conversion successful only if inet_pton returns 1
//     	perror("inet_pton error. Could not parse host address.");
//     	exit(-1);
//     }

//     // connect socket ref by sockfd to server_addr (address and port sprecified)
//     if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
//     	perror("connection error occured.\n");
//     	exit(-1);
//     }


//     // set up data connection
//     listenfd = socket(AF_INET, SOCK_STREAM, 0); // TCP(SOCK_STREAM) will only break connection when one party exits or network error occurs

//     memset(&data_addr, '\0', sizeof(data_addr));
//     data_addr.sin_family = AF_INET;
//     data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//     data_addr.sin_port = htons(0);

//     // bind socket to the data_addr port
//     bind(listenfd, (struct sockaddr*) &data_addr, sizeof(data_addr));

//     // listen for connections on socket
//     listen(listenfd, BACKLOGS); // max pending connections
    
//     // get ip address from control port
//     get_ip_port(sockfd, ip, (int *)&x);

//     // x = 0;
//     // printf(YEL "x: %d\n", x);
//     printf(YEL "ip: %s\n" RESET, ip);
//     //get data connection port from listenfd
//     get_ip_port(listenfd, str, (int *)&port);
    
//     printf(YEL "Port: %d\n" RESET,  port);
//     printf(YEL "str: %s\n" RESET, str);
//     convert(port, &n5, &n6);

//     while(1){

//         memset(command, '\0', strlen(command));

//         // get prompt
//         cmd_code = get_cmd_code(command);
        
//         // case: quit
//         if(cmd_code == 4){
//             char quit[100];
//             sprintf(quit, "QUIT");
//             write(sockfd, quit, strlen(quit));
//             memset(quit, '\0', (int)sizeof(quit));
//             read(sockfd,quit, 100);
//             printf(CYN "Server : %s\n" RESET, quit);
//             break;
//         }

//         // printf("command: %s\n", command);

//         //send PORT n1,n2,n3,n4,n5,n6
//         memset(str, '\0', (int)sizeof(str));
//         get_port_str(str, ip, n5, n6);

//         write(sockfd, str, strlen(str));
//         memset(str, '\0', (int)sizeof(str));
//         datafd = accept(listenfd, (struct sockaddr*)NULL, NULL);

//         // printf(CYN "Data connection Established...\n" RESET);

//         if(cmd_code == 1)
//         {
//             if(_ls(sockfd, datafd, command) < 0){
//                 close(datafd);
//                 continue;
//             }
//         }
//         else if(cmd_code == 2){
//             if(_get(sockfd, datafd, command) < 0){
//                 close(datafd);
//                 continue;
//             }
//         }
//         else if(cmd_code == 5){
//             if(_history(sockfd, datafd, command) < 0){
//                 close(datafd);
//                 continue;
//             }
//         }
//         close(datafd);
//     }
//     close(sockfd);	
// 	return TRUE;
// }


int main(int argc, char **argv){

	int server_port, controlfd, listenfd, datafd, code, n5, n6, x=rand()%10000;
    uint16_t port;
	struct sockaddr_in servaddr, data_addr;
	char command[1024], ip[50], str[MAXLINE+1];


	if(argc != 3){
		printf(RED "Invalid number of arguments. Please follow the format.\n");
		printf("Format: ./client <server-ip> <server-port>\n" RESET);
		exit(-1);
	}

	//get server port
	sscanf(argv[2], "%d", &server_port);

    //set up control connection--------------------------------------------------
    if ( (controlfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket error");
    	exit(-1);
    }
                
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(server_port);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){
    	perror("inet_pton error");
    	exit(-1);
    }
        
    if (connect(controlfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
    	perror("connect error");
    	exit(-1);
    }


    //set up data connection------------------------------------------------------
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&data_addr, sizeof(data_addr));
    data_addr.sin_family      = AF_INET;
    data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    data_addr.sin_port        = htons(0);

    bind(listenfd, (struct sockaddr*) &data_addr, sizeof(data_addr));

    listen(listenfd, BACKLOGS);
    
    //get ip address from control port
    get_ip_port(controlfd, ip, (int *)&x);
    //x = 0;
    // printf("x: %d\n", x);
    printf(YEL "ip: %s\n" RESET, ip);
    //get data connection port from listenfd
    get_ip_port(listenfd, str, (int *)&port);
    
    printf(YEL "Port: %d\n" RESET,  port);

    convert(port, &n5, &n6);

    while(1){

        bzero(command, strlen(command));
        //get command from user
        code = get_cmd_code(command);
        
        //user has entered quit
        if(code == 4){
            char quit[1024];
            sprintf(quit, "QUIT");
            write(controlfd, quit, strlen(quit));
            bzero(quit, (int)sizeof(quit));
            read(controlfd,quit, 1024);
            printf("Server Response: %s\n", quit);
            break;
        }
        // printf("command: %s\n", command);

        //send PORT n1,n2,n3,n4,n5,n6
        bzero(str, (int)sizeof(str));
        get_port_str(str, ip, n5, n6);

        write(controlfd, str, strlen(str));
        bzero(str, (int)sizeof(str));
        datafd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        printf("Data connection Established...\n");

        if(code == 1)
        {
            if(_ls(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }
        else if(code == 2){
            if(_get(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }
        else if(code == 5){
            if(_history(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }
        close(datafd);
    }
    close(controlfd);	
	return TRUE;
}
