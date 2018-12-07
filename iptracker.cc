/*
 * Rule:
 *     ./iptracker STATUS IP App JobID Time Duration Note?
 * Example:
 *     ./iptracker SUBMIT 192.168.1.1 ezSMDock 10000000000000000000000000000001 DEBUG
 *     ./iptracker START 192.168.1.1 ezSMDock 10000000000000000000000000000001 DEBUG
 *     ./iptracker END 192.168.1.1 ezSMDock 10000000000000000000000000000001 DEBUG
 * File Format:
 * IP\tApp\tJobID\tTimeSubmit\tTimeStart\tTimeEnd\tDuration
 */
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <chrono>
#include <thread>
#include <cstring>

using namespace std;

inline char *str2arr(const string &str1, const string &str2)
{
    string str3 = str1 + str2;
    char *cstr = new char[str3.length()+1];
    strcpy(cstr, str3.c_str());
    return cstr;
}

vector<string> split(const string& s)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (tokenStream >> token){
        tokens.push_back(token);
    }
    return tokens;
}

vector<string> split_d(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

inline bool lock_CHECK(const char *fileName)
{
    ifstream infile(fileName);
    return infile.good();
}

void lock_STATUS(const char *lock, unsigned int t)
{
    ofstream fo_lock;
    while (lock_CHECK(lock)){
        chrono::seconds dura(t);
        this_thread::sleep_for(dura);
    }
    fo_lock.open(lock);
    fo_lock << "ON HOLD!\n";
    fo_lock.close();
}

void job_STATUS(const string &folder, const char *log_name, const string &job, 
                const string &id, const string &uni, time_t t, const string &de_note)
{
    ifstream fi;
    const char *tmp_log = str2arr(folder, "/tmp_iptracker.log");
    const char *tmp_lock = str2arr(folder, "/tmp_iptracker.lock");
    ofstream tmp_fo, log;
    string tmp_str, r_out;
    if (job == "SUBMIT"){
        log.open(log_name, ios::app);
        log << uni 
            << left << setw(13) << t
            << left << setw(13) << "0000000000"
            << left << setw(13) << "0000000000"
            << left << setw(13) << "00000"
            << left << setw(8)  << de_note
            << endl;
        log.close();
    } else if (job == "START"){
        lock_STATUS(tmp_lock, 1);
        fi.open(log_name);
        tmp_fo.open(tmp_log);
        while (getline(fi, r_out)){
            size_t pos = r_out.find(id);
            if (pos != string::npos) {
                vector<string> vs = split(r_out);
                vs[4] = to_string(t);
                tmp_fo << left << setw(17) << vs[0]
                       << left << setw(16) << vs[1]
                       << left << setw(35) << vs[2]
                       << left << setw(13) << vs[3]
                       << left << setw(13) << vs[4]
                       << left << setw(13) << "0000000000"
                       << left << setw(13) << "00000"
                       << left << setw(8)  << de_note
                       << endl;
            } else {
                tmp_fo << r_out << '\n';
            }
        }
        fi.close();
        tmp_fo.close();
        rename(tmp_log, log_name);
        remove(tmp_lock);
    } else if (job == "END"){
        lock_STATUS(tmp_lock, 1);
        fi.open(log_name);
        tmp_fo.open(tmp_log);
        while (getline(fi, r_out)){
            size_t pos = r_out.find(id);
            if (pos != string::npos) {
                vector<string> vs = split(r_out);
                vs[5] = to_string(t);
                long v6 = difftime(stol(vs[5]), stol(vs[4]));
                tmp_fo << left << setw(17) << vs[0]
                       << left << setw(16) << vs[1]
                       << left << setw(35) << vs[2]
                       << left << setw(13) << vs[3]
                       << left << setw(13) << vs[4]
                       << left << setw(13) << vs[5]
                       << left << setw(13) << v6
                       << left << setw(8)  << de_note
                       << endl;
            } else {
                tmp_fo << r_out << '\n';
            }
        }
        fi.close();
        tmp_fo.close();
        rename(tmp_log, log_name);
        remove(tmp_lock);
    } else {
        cout << "ERROR: argv[1] must be SUBMIT, START, or END." << endl;
    }
}

void log_FILE(const char *log_name)
{
    ifstream non_log(log_name);
    ofstream log;
    // If log file doesn't exist, create one.
    if (!non_log){
        log.open(log_name);
        log << "#This file logs applications usage of ezCADD website, generated by iptracker.\n"
            << left << setw(17) << "IP_Address" 
            << left << setw(16) << "AppName"
            << left << setw(35) << "JobID (DirectoryName)"
            << left << setw(13) << "TimeSubmit"
            << left << setw(13) << "TimeStart"
            << left << setw(13) << "TimeEnd"
            << left << setw(13) << "Duration(s)"
            << left << setw(8)  << "Note"
            << endl;
        log.close();
    }
}

int main(int argc, char *argv[])
{
    if (argc < 7){
        cout << "Example: "<< argv[0] <<" [(Status) SUBMIT/START/END] [(IP) 192.168.1.1] [(App) ezSMDock] [(ID) 10000000000000000000000000000001] [DEBUG/Anynotes] [logPATH]\n";
        return 1;
    }
    time_t now_Time = time(0);
    string str_out, job_status, job_id;
    ostringstream str_uni;
    string de_note = argv[5];
    job_status = argv[1];
    job_id = argv[4];
    string folder_name = argv[6];
    const char *log_name = str2arr(folder_name, "/iptracker.log");
    str_uni << left << setw(17) << argv[2]
            << left << setw(16) << argv[3]
            << left << setw(35) << argv[4];
    log_FILE(log_name);
    job_STATUS(folder_name, log_name, job_status, job_id, str_uni.str(), now_Time, de_note);
    
    return 0;
}
