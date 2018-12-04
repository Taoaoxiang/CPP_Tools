#include <cstdio>
#include <cstdlib>
/* atoi, atol */
#include <iostream>
//using std::cout; using std::endl;
#include <sys/types.h>
#include <sys/stat.h>
/* mkdir */
#include <sys/socket.h>
/* socklen_t, socket, listen, accept */
#include <netinet/in.h>
/* sockaddr_in, htons */
#include <string>
/* to_string */
#include <cstring>
/* bzero */
#include <unistd.h>
/* close */
#include <map>
/* map */
#include <fstream>
#include <sstream>
#include <vector>

#define LENGTH 4096

// OpenEye headers
#include <openeye.h>
#include <oeplatform.h>
using namespace OEPlatform;
#include <oesystem.h>
using namespace OESystem;
#include <oechem.h>
using namespace OEChem;
#include <oegraphsim.h>
using namespace OEGraphSim;

using namespace std;

void fpsearch (int sock, int m1);
void dbCheck (string v3, string v4, int m1);

//void fpsearch(int);
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

class SSDatabase
{
    public:
        string smi;
        string idx;
        string fpbin;
};

// Global variable db
map< string, map< string, SSDatabase> > db;

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, pid;
    unsigned portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 3) {
    //fps 33888 fps_DB.in emolecules tree ts3
        cout << "Usage: \n" << argv[0] 
             << " [PORT_NO] [DB.in] " << endl;
        cout << "Example: \n" << argv[0] 
             << " 33888 fps_DB.in " << endl;
        exit(1);
    }
    // Create a tmp folder if not exist
    if ((mkdir("FPS_SERVER_TMP", 0777) == -1) && (errno != EEXIST)) {
        cout << "Permission denied." << endl;
        exit(1);
    }

    // Define map for entire database.
    //const string qfname = argv[1];
    //const string ts1 = argv[3];
    //const string ts2 = argv[4];
    //const string ts3 = argv[5];
    //const string ofname = argv[5];
    /* memtype * 1 memory-mapped * 2 in-memory * 3 CUDA */
    int memtype = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    // Set to zero 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    // Define the port number
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    // Listen for connections on a socket
    listen(sockfd, 128);
    clilen = sizeof(cli_addr);

    // Read DB.in file to global variable ::db.
    OEWallTimer timer;
    ifstream fin(argv[2]);
    string line;
    while (getline(fin, line)) {
        if (line[0] == '#') continue;
        SSDatabase database;
        timer.Start();
        istringstream ss(line);
        string var1, var2, var3, var4, var5;
        ss >> var1 >> var2 >> var3 >> var4 >> var5;
        ::db[var2][var1] = database;
        ::db[var2][var1].smi = var3;
        ::db[var2][var1].idx = var3 + ".idx";
        ::db[var2][var1].fpbin = var4;
        // Check DB_smi and DB_fpbin
        dbCheck(var3, var4, memtype);
        OEThrow.Info("%5.2f sec to initialize %s.", timer.Elapsed(), ::db[var2][var1].fpbin.c_str());
    }
    //cout << "TEST out: " << ::db[ts2][ts1].idx << endl;

    while (true) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        pid = fork();
        if (pid == 0) {
            close(sockfd);
            fpsearch(newsockfd, memtype);
            exit(0);
        } else if (pid < 0) {
            error("ERROR on fork");
            exit(1);
        } else {
            close(newsockfd);
        }
    }
    close(sockfd);
    return 0;
}

void dbCheck (string v3, string v4, int m1)
{
    OEMolDatabase moldb;
    if (!moldb.Open(v3))
        OEThrow.Fatal("Cannot open molecule database!");

    OEFastFPDatabase fpbin(v4, m1);
    if (!fpbin.IsValid())
        OEThrow.Fatal("Cannot open DB_fpbin!\nDB_fpbin: %s", v4.c_str());
    if (!OEAreCompatibleDatabases(moldb, fpbin))
        OEThrow.Fatal("DB_smi and DB_fpbin are not compatible!\nDB_smi: %s\nDB_fpbin%s", v3.c_str(), v4.c_str());
}

void fpsearch (int sock, int m1) 
{
    cout << "===START===" << endl;
    // Receive parameters from client
    int n;
    char buffer[256];
    OEWallTimer timer;    
    timer.Start();
    // 1. Receive nanoseconds from client, create a folder
    bzero(buffer,256);
    n = recv(sock, buffer, 256, 0);
    if (n < 0) error("ERROR reading from socket");
    char buTime[256] = "FPS_SERVER_TMP/";
    strncat (buTime, buffer, sizeof(buffer));
    cout << "(NanoTime): " << buffer << endl;
    mkdir(buTime, 0777);
    // 2. Receive database from client
    bzero(buffer,256);
    n = recv(sock, buffer, 256, 0);
    string buDB(buffer);
    cout << "(Database): " << buDB << endl;
    // 3. Receive fingerprints from client
    bzero(buffer,256);
    n = recv(sock, buffer, 256, 0);
    string buFPS(buffer);
    cout << "(FPS):      " << buFPS << endl;
    // 4. Receive number of results from client
    bzero(buffer,256);
    n = recv(sock, buffer, 256, 0);
    long buNO = atol(buffer);
    cout << "(#Result):  " << buNO << endl;

    // 5. Receive the Query file from client
    char revbuf[LENGTH];
    char query_smi[256];
    strncpy (query_smi, buTime, sizeof(buTime));
    strncat (query_smi, "/query.smi", sizeof(query_smi));
    FILE *fr = fopen(query_smi, "w");
    bzero(revbuf, LENGTH);
    int fr_block_sz = 0;
    while((fr_block_sz = recv(sock, revbuf, LENGTH, 0)) > 0) {
        int write_sz = fwrite(revbuf, sizeof(char), fr_block_sz, fr);
        if(write_sz < fr_block_sz) {
            error("File write failed on server.\n");
        }
        bzero(revbuf, LENGTH);
        if (fr_block_sz == 0 || fr_block_sz != LENGTH) {
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
    fclose(fr);

    // Open query.smi and read query molecule.
    oemolistream ifs;
    ifs.open(query_smi);
    OEGraphMol query;
    if (!OEReadMolecule(ifs, query)){
        cout << "Cannot open QUERY molecule!\n" 
             << "====END====\n" << endl;
        return;
    }
    // Read database and fpbin
    OEMolDatabase moldb;
    if (!moldb.Open(::db[buFPS][buDB].smi)){
        cout << "Cannot open TARGET DATABASE.smi!\n" 
             << "====END====\n" << endl;
        return;
    }
    OEFastFPDatabase fpdb(::db[buFPS][buDB].fpbin, m1);
    OEFPDatabaseOptions defopts(buNO, OESimMeasure::Tanimoto);
    // Output to .dat file.
    char result_datafile[256];
    strncpy (result_datafile, buTime, sizeof(buTime));
    strncat (result_datafile, "/Top_result.dat", sizeof(result_datafile));
    ofstream result_data;
    string data_str = "";
    // Similarity search
    OEIter<OESimScore> si = fpdb.GetSortedScores((const OEMolBase&)query, defopts);
    OEThrow.Info("(Search):  %5.2f sec.", timer.Elapsed());
    // Write ID, Tanimoto score, and SMILES to tab seperated .dat file.
    OEGraphMol hit;
    for ( ; si; ++si) {
        if (moldb.GetMolecule(hit, (unsigned int)si->GetIdx())) {
            data_str = data_str + hit.GetTitle() +'\t'+ OENumberToString(si->GetScore()) 
                       +'\t'+ OEMolToSmiles(hit) + '\n';
        }
    }
    result_data.open(result_datafile);
    result_data << data_str;
    result_data.close();

    // Send .dat file back to client.
    timer.Start();
    //char * buffer = (char *)malloc(LENGTH);
    char send_buf[LENGTH];
    bzero(send_buf, LENGTH);
    int fs_block_sz;
    FILE *fs_dat = fopen(result_datafile, "r");
    bzero(send_buf, LENGTH);
    while((fs_block_sz = fread(send_buf, sizeof(char), LENGTH, fs_dat)) > 0) {
        if(send(sock, send_buf, fs_block_sz, 0) < 0) {
            cout <<"ERROR: Failed to send file!\nFile: " << result_datafile << endl;
            //cout << errno << endl;
                break;
        }
        bzero(send_buf, LENGTH);
    }
    OEThrow.Info("(Send):    %5.2f sec.", timer.Elapsed());
    cout << "====END====\n" << endl;
}
