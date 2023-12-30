#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>
#include <time.h>
#include <sys/select.h>
#include <unistd.h>
#include <queue>
#include <set>
#include "cs3516sock.h"

#define END_HOST 1
#define ROUTER 0

using namespace std;

void readConfig(int node);
void handleHost();
void handleRouter();
ulong currentTimeMillisLong();
void routerLog(FILE *routerControl, string sourceIp, string destinationIp, int ipIdent, string statusCode, string nextHop);

struct queueNode
{
    char *buffer;
    int buffSize;
    int delay;
    string nextHop;
};

void vqPush(vector<queueNode> vq, queueNode qn);
void vqPop(vector<queueNode> vq);
struct queueNode vqFront(vector<queueNode> vq);

vector<vector<queueNode>> allInterfacesOnTheRouter;

// structure for ip header
struct ip_hdr
{
    u_int8_t ip_vhl;
    u_int8_t ip_tos;
    u_int16_t ip_len;
    u_int16_t ip_id;
    u_int16_t ip_off;
    u_int8_t ip_ttl;
    u_int8_t ip_p;
    u_int16_t ip_sum;
    struct in_addr ip_src;
    struct in_addr ip_dst;
};

// structure for udp header
struct udp_hdr
{
    u_int16_t uh_sport;
    u_int16_t uh_dport;
    u_int16_t uh_ulen;
    u_int16_t uh_sum;
};

// global configuration vars
int queueLen;
int defaultTTL;

// router identification vars
struct routerID
{
    int routerID;
    string routerIP;
};

// host identification vars
struct endHostID
{
    int endHostID;
    string endHostIP;
    string hostOverlayIP;
};

// router-to-router link vars
struct routerToRouter
{
    int router1ID;
    int router1SendDel;
    int router2ID;
    int router2SendDel;
};

// router-to-host link vars
struct routerToHost
{
    int routerID;
    int routerSendDel;
    string overPref;
    int endHostID;
    int hostSendDel;
};

// vectors of defined structures
vector<routerID> routerIDVector;
vector<endHostID> endHostIDVector;
vector<routerToRouter> routerToRouterVector;
vector<routerToHost> routerToHostVector;

// node variables
int node;
int role;
string realIP;
string overlayIP;
struct routerToHost nextHop;
string nextHopIP;
int vectorIndex;
struct in_addr realIPAddr;
struct in_addr overlayIPAddr;
int sendDelay;
int ipId = 0;

int main(int argc, char *argv[])
{
    if (argc != 2) // check command line args
    {
        dieWithError("Error: Incorrect number of arguments!");
    }

    node = atoi(argv[1]);
    printf("Node: %d\n", node);

    readConfig(node); // read the configuration file

    if (role == END_HOST)
    {
        handleHost(); // handle the end host node
    }
    else if (role == ROUTER)
    {
        handleRouter(); // handle the router node
    }

    return 0;
}

void handleHost() // handle host function
{
    printf("Host:\n");
    int sock = create_cs3516_socket(realIPAddr); // create socket with ip address from config
    printf("Socket created: %d\n", sock);

    int fileSent = 0;

    while (1)
    {
        if ((fileSent == 0) && (access("send_config.txt", F_OK) == 0)) // check if file already send and if send config exists
        {
            ifstream hostSendConfigFile("./send_config.txt");
            string hostSendConfig;
            getline(hostSendConfigFile, hostSendConfig);

            int space1 = hostSendConfig.find(' ', 0);
            int space2 = hostSendConfig.find(' ', space1 + 1);

            printf("0 %d %d\n", space1, space2);

            string destinationOverlayIP = hostSendConfig.substr(0, space1);
            string sourcePortUDP = hostSendConfig.substr(space1 + 1, 4);
            string destPortUDP = hostSendConfig.substr(space2 + 1, 4);

            printf("Destination Overlay IP: |%s|, Source UDP Port: |%s|, Destination UDP Port: |%s|\n", destinationOverlayIP.c_str(), sourcePortUDP.c_str(), destPortUDP.c_str());

            FILE *sendBody;
            sendBody = fopen("./send_body.txt", "r");
            fseek(sendBody, 0L, SEEK_END);
            uint32_t fileSize = ftell(sendBody);
            rewind(sendBody);

            char fileSizeBuffer[32];
            struct ip_hdr *ipHdrSize = (struct ip_hdr *)fileSizeBuffer;
            struct udp_hdr *udpHdrSize = (struct udp_hdr *)(fileSizeBuffer + 20);
            char *sizeData = fileSizeBuffer + 28;

            ipHdrSize->ip_dst.s_addr = inet_addr(destinationOverlayIP.c_str());
            ipHdrSize->ip_src.s_addr = inet_addr(overlayIP.c_str());
            ipHdrSize->ip_ttl = defaultTTL;
            ipHdrSize->ip_id = ipId;
            udpHdrSize->uh_sport = htons(stoi(sourcePortUDP));
            udpHdrSize->uh_dport = htons(stoi(destPortUDP));
            memcpy(sizeData, &fileSize, 4);

            printf("nextHopIP.c_str(): %s\n", nextHopIP.c_str());
            cs3516_send(sock, fileSizeBuffer, 32, (unsigned long)inet_addr(nextHopIP.c_str())); // send the file size
            usleep(sendDelay * 1000);
            ipId++;

            ifstream sendData("./send_body.txt");
            int bytesLeft = fileSize;
            int count = 0;
            while (bytesLeft > 0)
            {
                char fileContentsBuffer[1000];
                struct ip_hdr *ipHdrContents = (struct ip_hdr *)fileContentsBuffer;
                struct udp_hdr *udpHdrContents = (struct udp_hdr *)(fileContentsBuffer + 20);
                char *contentsData = fileContentsBuffer + 28;
                memset(contentsData, 0, 972);
                int bytesToRead = min(bytesLeft, 971);

                sendData.read(contentsData, bytesToRead);

                memcpy(ipHdrContents, ipHdrSize, sizeof(ip_hdr));
                memcpy(udpHdrContents, udpHdrSize, sizeof(udp_hdr));
                udpHdrContents = udpHdrSize;
                ipHdrContents->ip_id = ipId;

                cs3516_send(sock, fileContentsBuffer, 1000, (unsigned long)inet_addr(nextHopIP.c_str())); // send the file contents
                usleep(sendDelay * 1000);
                ipId++;
                count++;
                bytesLeft -= bytesToRead;
            }
            printf("(send) File Size: %d Number of Packets: %d\n", fileSize, count);
            fileSent = 1;
        }
        else if (fileSent == 0)
        {
            perror("err");
            printf("Failed to open file! %s\n", get_current_dir_name());
        }

        char destBufferSize[32];
        struct ip_hdr *ipHdr = (struct ip_hdr *)destBufferSize;
        struct udp_hdr *udpHdr = (struct udp_hdr *)(destBufferSize + 20);
        char *data = destBufferSize + 28;

        int actual = cs3516_recv(sock, destBufferSize, 32); // receive the file size

        uint32_t fileSize;
        memcpy(&fileSize, data, 4);

        printf("File received! File size: %u\n", fileSize);
        int count = 1;

        FILE *recvdStatsFile;
        recvdStatsFile = fopen("./received_stats.txt", "a+");
        string recvdOverlayIPAddr = inet_ntoa(ipHdr->ip_dst);
        uint16_t recvdSourcePort = ((udpHdr->uh_sport));
        uint16_t recvdDestPort = ((udpHdr->uh_dport));

        if (recvdStatsFile != NULL)
        {
            char statsBuffer[1000];
            sprintf(statsBuffer, "Overlay IP Address: %s Source Port: %u Destination Port: %u\n", recvdOverlayIPAddr.c_str(), recvdSourcePort, recvdDestPort);
            fputs(statsBuffer, recvdStatsFile);
            printf("Wrote packet contents to stats file!\n");
        }
        else
        {
            printf("Could not find stats file!");
        }

        fclose(recvdStatsFile);

        int expectedNumPackets = (fileSize / 971) + 1;
        set<int> missingPackets;

        for (int i = 1; i < expectedNumPackets; i++)
        {
            missingPackets.insert(i);
        }

        FILE *recvdFile;
        recvdFile = fopen("./received", "a+");

        int bytesLeft = fileSize;
        while (bytesLeft > 0)
        {
            char destBufferContents[1000];
            struct ip_hdr *ipHdrRecv = (struct ip_hdr *)destBufferContents;
            struct udp_hdr *udpHdrRecv = (struct udp_hdr *)(destBufferContents + 20);
            char *dataRecv = destBufferContents + 28;
            int bytesToRead = min(bytesLeft, 972);

            char packetContents[972];

            int actual = cs3516_recv(sock, destBufferContents, 1000); // receive the file contents

            memcpy(&packetContents, dataRecv, 972);
            printf("PACKET CONTENTS: [%s]\n", packetContents);

            if (recvdFile != NULL)
            {
                fputs(packetContents, recvdFile);
                printf("Wrote packet contents to file!\n");
            }
            else
            {
                printf("Could not find file!");
            }

            count++;
            bytesLeft -= bytesToRead;
            missingPackets.erase(ipHdrRecv->ip_id);
        }
        fclose(recvdFile);
        printf("(receive) File Size: %d Number of Packets: %d\n", fileSize, count);
        if (missingPackets.size() > 0)
        {
            printf("(receive) Missing packet IDs: ");

            set<int>::iterator si;
            for (si = missingPackets.begin(); si != missingPackets.end(); ++si)
            {
                printf("%d ", *si);
            }
            printf("\n");
        }
        else
        {
            printf("(receive) No missing packets!\n");
        }
    }
}

void handleRouter()
{
    int sock = create_cs3516_socket(realIPAddr); // create socket with ip address from config
                                                 // printf("Socket created: %d\n", sock);

    vector<pair<int, ulong>> currentActiveQueues;
    ulong timeUntilValidSend = currentTimeMillisLong();

    while (1)
    {
        FILE *routerControl;
        routerControl = fopen("./ROUTER_control.txt", "a+");

        // vector<pair<int, ulong>> currentActiveQueues2;

        // for (auto &activeQueue : currentActiveQueues)
        // {
        //     printf("Here8, currentActiveQueues size: %lu\n", currentActiveQueues.size());
        //     ulong currentTime = currentTimeMillisLong();
        //     if (currentTime > activeQueue.second)
        //     {
        //         printf("Here9 %lu %lu %d\n", allInterfacesOnTheRouter.size(), allInterfacesOnTheRouter[activeQueue.first - 1].size(), activeQueue.first - 1);
        //         struct queueNode toSend = vqFront(allInterfacesOnTheRouter[activeQueue.first - 1]);
        //         printf("Here9.5\n");
        //         vqPop(allInterfacesOnTheRouter.at(activeQueue.first - 1));

        //         struct ip_hdr *ipHdr;
        //         memcpy(ipHdr, toSend.buffer, 20);
        //         cs3516_send(sock, toSend.buffer, toSend.buffSize, (unsigned long)inet_addr(toSend.nextHop.c_str()));                             // send call to end host
        //         routerLog(routerControl, inet_ntoa(ipHdr->ip_src), inet_ntoa(ipHdr->ip_dst), ipHdr->ip_id, "SENT_OKAY", toSend.nextHop.c_str()); // router log success

        //         if (allInterfacesOnTheRouter.at(activeQueue.first - 1).size() > 0)
        //         {
        //             printf("Here10\n");
        //             struct queueNode newQueueNode = vqFront(allInterfacesOnTheRouter[activeQueue.first - 1]);
        //             ulong nextSendTime = currentTimeMillisLong() + newQueueNode.delay;
        //             currentActiveQueues2.push_back(make_pair(activeQueue.first, nextSendTime));
        //         }
        //     }
        //     else
        //     {
        //         printf("Here11\n");
        //         currentActiveQueues2.push_back(activeQueue);
        //     }
        //     printf("Here12\n");
        // }

        // currentActiveQueues = currentActiveQueues2;

        uint32_t fileSize;
        char packetContents[972];
        char destBuffer[1000];
        struct ip_hdr *ipHdr = (struct ip_hdr *)destBuffer;
        struct udp_hdr *udpHdr = (struct udp_hdr *)(destBuffer + 20);
        char *data = destBuffer + 28;

        fd_set readFds;
        struct timeval tv;
        int selNum;
        int selRetVal;
        FD_ZERO(&readFds);
        FD_SET(sock, &readFds);
        selNum = sock + 1;
        tv.tv_sec = 0;
        tv.tv_usec = 10 * 1000;
        selRetVal = select(selNum, &readFds, NULL, NULL, &tv);

        if (selRetVal == 0)
        {
            continue;
        }

        int actual = cs3516_recv(sock, destBuffer, 1000); // receive the file data

        printf("ipHdr->ip_dst.s_addr: %s\n", inet_ntoa(ipHdr->ip_dst));
        printf("udpHdr->uh_sport: %d\n", htons(udpHdr->uh_sport));
        printf("iudpHdr->uh_dport: %d\n", htons(udpHdr->uh_sport));

        memcpy(&fileSize, data, 4);
        fileSize = (fileSize);

        printf("data: %u\n", fileSize);

        memcpy(&packetContents, data, 972);
        printf("PACKET CONTENTS: [%s]\n", packetContents);

        ipHdr->ip_ttl--;

        if (ipHdr->ip_ttl < 1) // if time to live has expired
        {
            routerLog(routerControl, inet_ntoa(ipHdr->ip_src), inet_ntoa(ipHdr->ip_dst), ipHdr->ip_id, "TTL_EXPIRED", "N/A"); // router log ttl expired
            continue;
        }
        else
        {
            printf("Packet TTL is valid, %d hops left!\n", ipHdr->ip_ttl);
        }

        for (auto &routerToHostEl : routerToHostVector)
        {
            string ipWithPrefix = inet_ntoa(ipHdr->ip_dst);
            string findIPSAddr = ipWithPrefix.substr(0, ipWithPrefix.find_last_of('.'));
            int overPrefResult = routerToHostEl.overPref.find(findIPSAddr.c_str());
            printf("it1.overPref: %s, findIPSAddr: %s, overPrefResult: %d\n", routerToHostEl.overPref.c_str(), findIPSAddr.c_str(), overPrefResult);

            if (overPrefResult != string::npos) // if the destination prefix is found
            {
                if (allInterfacesOnTheRouter[routerToHostEl.routerID - 1].size() >= queueLen)
                {
                    routerLog(routerControl, inet_ntoa(ipHdr->ip_src), inet_ntoa(ipHdr->ip_dst), ipHdr->ip_id, "MAX_SENDQ_EXCEEDED", "N/A"); // router log ttl expired
                    continue;
                }
                int delayMS;
                printf("Found overPrefResult! node: %d, it1.routerID: %d\n", node, routerToHostEl.routerID);
                string nextHopString;
                if (node == routerToHostEl.routerID)
                {
                    delayMS = routerToHostEl.routerSendDel;
                    printf("Send to end host!\n"); // last router before sending to host
                    for (auto &endHostIDEl : endHostIDVector)
                    {
                        if (endHostIDEl.endHostID == routerToHostEl.endHostID)
                        {
                            nextHopString = endHostIDEl.endHostIP;
                            break;
                        }
                    }
                    printf("End host IP: %s\n", nextHopString.c_str());
                }
                else
                {
                    printf("Send to router!\n"); // send to another router
                    // int sendDelay;
                    int routerIDElID;
                    for (auto &routerIDEl : routerIDVector)
                    {
                        if (routerIDEl.routerID == routerToHostEl.routerID)
                        {
                            routerIDElID = routerIDEl.routerID;
                            nextHopString = routerIDEl.routerIP;
                            break;
                        }
                    }
                    printf("Router IP: %s\n", nextHopString.c_str());

                    for (auto &routerToRouterEl : routerToRouterVector)
                    {
                        if ((routerToRouterEl.router1ID == node) && (routerToRouterEl.router2ID == routerIDElID))
                        {
                            delayMS = routerToRouterEl.router1SendDel;
                        }
                        else if ((routerToRouterEl.router2ID == node) && (routerToRouterEl.router1ID == routerIDElID))
                        {
                            delayMS = routerToRouterEl.router2SendDel;
                        }
                    }
                }

                printf("Delay in ms: %d\n", delayMS);

                struct queueNode recvdQueueNode = {destBuffer, 1000, delayMS, nextHopString};

                // sending without droptail queueing
                struct queueNode toSend = recvdQueueNode;
                // struct ip_hdr *ipHdr;
                printf("HereA\n");
                memcpy(ipHdr, destBuffer, 20);
                printf("HereB\n");
                cs3516_send(sock, destBuffer, 1000, (unsigned long)inet_addr(nextHopString.c_str()));
                printf("HereC\n");                            // send call to end host
                routerLog(routerControl, inet_ntoa(ipHdr->ip_src), inet_ntoa(ipHdr->ip_dst), ipHdr->ip_id, "SENT_OKAY", toSend.nextHop.c_str()); // router log success
                printf("HereD\n");

                printf("Here1\n");
                if (allInterfacesOnTheRouter[node - 1].size() == 0)
                {
                    printf("Here2\n");
                    ulong nextSendTime = currentTimeMillisLong() + delayMS;
                    printf("Here3\n");
                    currentActiveQueues.push_back(make_pair(routerToHostEl.routerID, nextSendTime));
                    printf("Here4\n");
                }
                printf("Here5 %d\n", allInterfacesOnTheRouter[node - 1].empty());
                printf("Here7 %d\n", allInterfacesOnTheRouter.empty());
                vqPush(allInterfacesOnTheRouter[node - 1], recvdQueueNode); // queue size is 1

                printf("Updating node queues! %lu\n", allInterfacesOnTheRouter[node - 1].size()); // queue size is 0?
                break;
            }
        }
        fclose(routerControl);
    }
}

ulong currentTimeMillisLong()
{
    return (unsigned)(time(NULL) * 1000);
}

void routerLog(FILE *routerControl, string sourceIp, string destinationIp, int ipIdent, string statusCode, string nextHop)
{
    char routerBuffer[1000];
    sprintf(routerBuffer, "%u %s %s %d %s %s\n", (unsigned)time(NULL), sourceIp.c_str(), destinationIp.c_str(), ipIdent, statusCode.c_str(), nextHop.c_str());
    fputs(routerBuffer, routerControl);
}

void readConfig(int node)
{
    ifstream config("./config.txt");
    if (config.is_open())
    {
        string line;
        while (getline(config, line))
        {
            istringstream iss(line);
            vector<string> results(istream_iterator<string>{iss}, istream_iterator<string>());
            int type = atoi(results.at(0).c_str());
            if (type == 0) // if line is global variables
            {
                queueLen = atoi(results.at(1).c_str());
                defaultTTL = atoi(results.at(2).c_str());
            }
            else if (type == 1) // if line is routerID
            {
                int routerID = atoi(results.at(1).c_str());
                string routerIP = results.at(2);
                struct routerID add = {routerID, routerIP};
                routerIDVector.push_back(add);
            }
            else if (type == 2) // if line is endHostID
            {
                int endHostID = atoi(results.at(1).c_str());
                string endHostIP = results.at(2);
                string hostOverlayIP = results.at(3);
                struct endHostID add = {endHostID, endHostIP, hostOverlayIP};
                endHostIDVector.push_back(add);
            }
            else if (type == 3) // if line is routerToRouter
            {
                int router1ID = atoi(results.at(1).c_str());
                int router1SendDel = atoi(results.at(2).c_str());
                int router2ID = atoi(results.at(3).c_str());
                int router2SendDel = atoi(results.at(4).c_str());
                struct routerToRouter add = {router1ID, router1SendDel, router2ID, router2SendDel};
                routerToRouterVector.push_back(add);
            }
            else if (type == 4) // if line is routerToHost
            {
                int routerID = atoi(results.at(1).c_str());
                int routerSendDel = atoi(results.at(2).c_str());
                string overPref = results.at(3);
                int endHostID = atoi(results.at(4).c_str());
                int hostSendDel = atoi(results.at(5).c_str());
                struct routerToHost add = {routerID, routerSendDel, overPref, endHostID, hostSendDel};
                routerToHostVector.push_back(add);
            }
            results.clear();
        }
        config.close();
    }

    bool foundID = false;
    vectorIndex = -1;

    int endHostVectorIndex = 0;
    for (auto &it : endHostIDVector) // get realIP, overlayIP, and role if host
    {
        if (it.endHostID == node)
        {
            vectorIndex = endHostVectorIndex;
            role = END_HOST;
            realIP = it.endHostIP;
            realIPAddr.s_addr = inet_addr(realIP.c_str());
            overlayIP = it.hostOverlayIP;
            overlayIPAddr.s_addr = inet_addr(overlayIP.c_str());
            foundID = true;
            break;
        }
        endHostVectorIndex++;
    }

    int routerVectorIndex = 0;
    for (auto &it : routerIDVector) // get realIP and role if router
    {
        if (it.routerID == node)
        {
            vectorIndex = routerVectorIndex;
            role = ROUTER;
            realIP = it.routerIP;
            realIPAddr.s_addr = inet_addr(realIP.c_str());
            foundID = true;
            break;
        }
        routerVectorIndex++;
    }

    if (role == END_HOST) // get initial next hop if host
    {
        for (auto &it : routerToHostVector)
        {
            if (it.endHostID == node)
            {
                nextHop = it;
                sendDelay = it.hostSendDel;
                break;
            }
        }
        for (auto &it : routerIDVector)
        {
            if (nextHop.routerID == it.routerID)
            {
                nextHopIP = it.routerIP;
            }
        }
    }

    if (role == ROUTER)
    {
        for (auto &it : routerIDVector)
        {
            struct vector<queueNode> emptyQueue;
            emptyQueue.reserve(queueLen);
            printf("eq size: %lu, eq cap: %lu\n", emptyQueue.size(), emptyQueue.capacity());
            allInterfacesOnTheRouter.push_back(emptyQueue);
        }
    }

    if (!foundID) // if node was not found
    {
        dieWithError("Could not find node ID!");
    }

    if (vectorIndex == -1)
    {
        dieWithError("Vector index not updated!");
    }

    printf("queueLen: %d\n", queueLen);
    printf("defaultTTL: %d\n", defaultTTL);
    printf("realIP: %s\n", realIP.c_str());

    if (role == END_HOST)
    {
        printf("overlayIP: %s\n", overlayIP.c_str());
    }

    allInterfacesOnTheRouter.resize(routerToHostVector.size());
}

void vqPush(vector<queueNode> vq, queueNode qn)
{
    printf("vq size: %lu, vq cap: %lu\n", vq.size(), vq.capacity());
    vq.insert(vq.end(), qn);
    printf("vq size: %lu, vq cap: %lu\n", vq.size(), vq.capacity());
}

void vqPop(vector<queueNode> vq)
{
    vq.erase(vq.begin());
}

struct queueNode vqFront(vector<queueNode> vq)
{
    return *vq.begin();
}