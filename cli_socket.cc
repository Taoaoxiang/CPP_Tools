#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
/* close */
#include <string>
/* to_string */
#include <cstring>
/* bzero */
#include <sys/types.h>
#include <sys/socket.h>
/* socklen_t, socket, listen, 
 * accept, write, send */
#include <netinet/in.h>
/* sockaddr_in, htons */
#include <netdb.h> 
#include <fstream>
#include <chrono>
#include <ctime>

#define LENGTH 4096

using namespace std;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

inline char *str2arr(const string &str1)
{
    char *cstr = new char[str1.length()+1];
    strcpy(cstr, str1.c_str());
    return cstr;
}

inline char *nanotime(void)
{
    chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = chrono::duration_cast<chrono::nanoseconds>(duration);
    return str2arr(to_string(nanoseconds.count()));
}

int main(int argc, char *argv[])
{
    int sockfd, n;
    unsigned portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    if (argc < 6) {
       cout << "Usage: " << argv[0]
            << " [hostname] [PORT_NO] [database] [fps] [#Result] [QUERY.smi] " << endl;
        cout << "Example: " << argv[0]
             << " localhost 33888 emolecules tree 200 query.smi" << endl;
       exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        cout << "ERROR, no such host" << endl;
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    // Send name of database and type of fingerprints.
    char* db_name = argv[3];
    char* fps_name = argv[4];
    char* num_result = argv[5];
    char* now_Time = nanotime();
    //cout << now_Time << endl;
    n = send(sockfd, now_Time, 256, 0);
    n = send(sockfd, db_name, 256, 0);
    n = send(sockfd, fps_name, 256, 0);
    n = send(sockfd, num_result, 256, 0);

    // Send the QUERY.smi to server.
    string fs_name = argv[6];
    char send_buf[LENGTH];
    FILE *fs = fopen(argv[6], "r");
    if(fs == NULL) {
        cout <<"ERROR: File not found!\nFile: " << fs_name << endl;
        exit(1);
    }
    bzero(send_buf, LENGTH);
    int fs_block_sz;
    while((fs_block_sz = fread(send_buf, sizeof(char), LENGTH, fs)) > 0) {
        if(send(sockfd, send_buf, fs_block_sz, 0) < 0) {
            cout <<"ERROR: Failed to send file!\nFile: " << fs_name << endl;
                break;
        }
        bzero(send_buf, LENGTH);
    }
    //cout <<"[Client] File was Sent!" << endl;


    // Receive .dat file from server
    char revbuf[LENGTH];
    int fr_block_sz = 0;
    FILE *fr_dat = fopen("Top_result.dat", "w");
    bzero(revbuf, LENGTH);
    fr_block_sz = 0;
    while((fr_block_sz = recv(sockfd, revbuf, LENGTH, 0)) > 0) {
        int write_sz = fwrite(revbuf, sizeof(char), fr_block_sz, fr_dat);
        if(write_sz < fr_block_sz) {
            error("File write failed on server.\n");
        }
        bzero(revbuf, LENGTH);
        if (fr_block_sz == 0) {
            break;
        }
    }
    if(fr_block_sz < 0) {
        if (errno == EAGAIN) {
            printf("recv() timed out.\n");
        } else {
            cout << "recv() failed " << endl;
            exit(1);
        }
    }
    fclose(fr_dat);
    //cout << "====END====\n" << endl;
    close(sockfd);
    return 0;
}
