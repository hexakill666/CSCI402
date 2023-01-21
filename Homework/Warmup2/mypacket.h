#ifndef _MYPACKET_H
#define _MYPACKET_H

typedef struct {
    int serviceType;
    int packetType;
    long long packetId;
    
    long long tokenNeed;
    long long interPacketTime;
    long long packetServiceTime;

    long long arriveTime;
    long long enterQ1Time;
    long long leaveQ1Time;
    long long enterQ2Time;
    long long leaveQ2Time;
    long long beginServiceTime;
    long long endServiceTime;

    long long realInterPacketArriveTime;
} MyPacket;

typedef struct {
    long long tokenNeed;
    long long interPcketTime;
    long long packetServiceTime;
} PacketData;

#endif