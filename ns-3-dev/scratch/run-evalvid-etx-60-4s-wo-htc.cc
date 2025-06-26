/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// This script configures two nodes on an 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to the other node.  The physical layer is configured
// to receive at a fixed RSS (regardless of the distance and transmit
// power); therefore, changing position of the nodes has no effect.
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc --help"
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when rss drops below -97 dBm.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --rss=-97 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-98 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-99 --numPackets=20"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc --verbose=1"
//
// When you are done, you will notice two pcap trace files in your directory.
// If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-0-0.pcap -nn -tt
//
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/seq-ts-header.h"
#include "ns3/evalvid-client-server-helper.h"
#include "ns3/uinteger.h"
#include "ns3/flow-id-tag.h"
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cstdlib>

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleAdhoc");

const int32_t numNodesMax = 60;
double intervalHello = 2.0;
double intervalTC = 5.0; 
const uint32_t listSize = 100;

const std::string baseDirLinks = "../../inst/links/coastguard-60/wo-htc/4s-4M/";
const std::string baseDirInputFitpath = "../inst/links/coastguard-60/wo-htc/4s-4M/";
const std::string baseDirEvalvid = "../../inst/evalvid/coastguard-60/wo-htc/4s-4M/";
const std::string baseDirOutputFitpath = "../../inst/newILS/0new-route300_";


std::unordered_map<uint32_t, bool> nodeFailStatusMap;
std::unordered_map<uint32_t, bool> nodeFailStatusMapTC;

std::vector<uint32_t> nodeQueue(numNodesMax+1);
std::vector<uint32_t> traceQueue(numNodesMax+1);

// Store info about neighbor nodes
struct SenderInfo{
    uint32_t oneHopNeigh; // sender node Id
    vector<uint32_t> receivedPackets; 
    uint32_t lastSeqNum; 
    uint32_t linkQual; 
    vector<uint32_t> twoHopNeigh;
    vector<uint32_t> twoHopLinkQual;
    float etx; 
    double lastTimeRcvHello;
};

vector<SenderInfo> oneHopNList[numNodesMax];

struct SenderInfoTC{
    uint32_t nodeTC;
    uint32_t lastSeqNumTC;
    vector<uint32_t> neighborsTC;
    vector<uint32_t> etxTC;
    double lastTimeRcvTC;
};

vector<SenderInfoTC> topologyControl[numNodesMax];

struct NeighHeader : public Header{         

  uint16_t NodeId;
  uint16_t Seq;
  uint16_t sizeVec; //neighbor numbers
  uint16_t nextHopHeader[numNodesMax - 1]; 
  uint32_t cost[numNodesMax - 1];

  NeighHeader (){
    NodeId=0;
    Seq=0;
    sizeVec=0; // neighbor numbers
    for (uint8_t i=0; i < numNodesMax-1; i++){
        nextHopHeader[i]=0;
        cost[i]=0;
    }
  }

  uint32_t GetSerializedSize() const override {
    return ((3 * sizeof(uint16_t)) + (sizeVec * sizeof(uint16_t)) + (sizeVec * sizeof(uint32_t))); 
  }

  void Serialize(Buffer::Iterator start) const override {
    start.WriteHtonU16(NodeId);
    start.WriteHtonU16(Seq);
    start.WriteHtonU16(sizeVec);
    for (uint32_t j = 0; j < sizeVec; j++){
        start.WriteHtonU16(nextHopHeader[j]);
        start.WriteHtonU32(cost[j]);
    }
  }

  uint32_t Deserialize(Buffer::Iterator start) override {
    NodeId = start.ReadNtohU16();
    Seq = start.ReadNtohU16();
    sizeVec = start.ReadNtohU16();
    for (uint32_t j = 0; j < sizeVec; j++){
        nextHopHeader[j] = start.ReadNtohU16();
        cost[j] = start.ReadNtohU32();
    }
    return GetSerializedSize();
  }

  void Print(std::ostream &os) const override {
    os << "Next Hop Node: ";
    for (uint8_t i = 0; i < sizeVec; i++){
      os << nextHopHeader[i] << " ";
    os << std::endl;
    }
  }

  TypeId GetInstanceTypeId() const override {
    return GetTypeId();
  }
};

void Device1PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[1]++; nodeQueue[1]+=newValue;}
void Device2PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[2]++; nodeQueue[2]+=newValue;}
void Device3PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[3]++; nodeQueue[3]+=newValue;}
void Device4PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[4]++; nodeQueue[4]+=newValue;}
void Device5PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[5]++; nodeQueue[5]+=newValue;}
void Device6PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[6]++; nodeQueue[6]+=newValue;}
void Device7PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[7]++; nodeQueue[7]+=newValue;}
void Device8PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[8]++; nodeQueue[8]+=newValue;}
void Device9PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[9]++; nodeQueue[9]+=newValue;}
void Device10PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[10]++; nodeQueue[10]+=newValue;}
void Device11PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[11]++; nodeQueue[11]+=newValue;}
void Device12PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[12]++; nodeQueue[12]+=newValue;}
void Device13PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[13]++; nodeQueue[13]+=newValue;}
void Device14PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[14]++; nodeQueue[14]+=newValue;}
void Device15PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[15]++; nodeQueue[15]+=newValue;}
void Device16PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[16]++; nodeQueue[16]+=newValue;}
void Device17PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[17]++; nodeQueue[17]+=newValue;}
void Device18PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[18]++; nodeQueue[18]+=newValue;}
void Device19PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[19]++; nodeQueue[19]+=newValue;}
void Device20PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[20]++; nodeQueue[20]+=newValue;}
void Device21PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[21]++; nodeQueue[21]+=newValue;}
void Device22PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[22]++; nodeQueue[22]+=newValue;}
void Device23PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[23]++; nodeQueue[23]+=newValue;}
void Device24PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[24]++; nodeQueue[24]+=newValue;}
void Device25PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[25]++; nodeQueue[25]+=newValue;}
void Device26PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[26]++; nodeQueue[26]+=newValue;}
void Device27PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[27]++; nodeQueue[27]+=newValue;}
void Device28PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[28]++; nodeQueue[28]+=newValue;}
void Device29PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[29]++; nodeQueue[29]+=newValue;}
void Device30PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[30]++; nodeQueue[30]+=newValue;}
void Device31PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[31]++; nodeQueue[31]+=newValue;}
void Device32PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[32]++; nodeQueue[32]+=newValue;}
void Device33PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[33]++; nodeQueue[33]+=newValue;}
void Device34PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[34]++; nodeQueue[34]+=newValue;}
void Device35PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[35]++; nodeQueue[35]+=newValue;}
void Device36PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[36]++; nodeQueue[36]+=newValue;}
void Device37PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[37]++; nodeQueue[37]+=newValue;}
void Device38PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[38]++; nodeQueue[38]+=newValue;}
void Device39PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[39]++; nodeQueue[39]+=newValue;}
void Device40PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[40]++; nodeQueue[40]+=newValue;}
void Device41PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[41]++; nodeQueue[41]+=newValue;}
void Device42PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[42]++; nodeQueue[42]+=newValue;}
void Device43PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[43]++; nodeQueue[43]+=newValue;}
void Device44PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[44]++; nodeQueue[44]+=newValue;}
void Device45PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[45]++; nodeQueue[45]+=newValue;}
void Device46PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[46]++; nodeQueue[46]+=newValue;}
void Device47PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[47]++; nodeQueue[47]+=newValue;}
void Device48PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[48]++; nodeQueue[48]+=newValue;}
void Device49PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[49]++; nodeQueue[49]+=newValue;}
void Device50PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[50]++; nodeQueue[50]+=newValue;}
void Device51PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[51]++; nodeQueue[51]+=newValue;}
void Device52PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[52]++; nodeQueue[52]+=newValue;}
void Device53PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[53]++; nodeQueue[53]+=newValue;}
void Device54PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[54]++; nodeQueue[54]+=newValue;}
void Device55PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[55]++; nodeQueue[55]+=newValue;}
void Device56PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[56]++; nodeQueue[56]+=newValue;}
void Device57PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[57]++; nodeQueue[57]+=newValue;}
void Device58PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[58]++; nodeQueue[58]+=newValue;}
void Device59PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[59]++; nodeQueue[59]+=newValue;}
void Device60PacketsInQueueTrace (uint32_t oldValue, uint32_t newValue){traceQueue[60]++; nodeQueue[60]+=newValue;}

void TearDownLink (uint32_t interface, Ptr<Node> node, uint32_t nodeId){
    bool& ndFailStatus = nodeFailStatusMap[nodeId];
    //NS_LOG_UNCOND(">> tearDownLink: nodeFailStatus "<< ndFailStatus << " nodeId "<< nodeId);
    bool& ndFailStatusTC = nodeFailStatusMapTC[nodeId];
    //NS_LOG_UNCOND(">> tearDownLink: nodeFailStatusTC "<< ndFailStatusTC << " nodeId "<< nodeId);
    node->GetObject<Ipv4>()->SetDown(interface);
    ndFailStatus = true;
    //NS_LOG_UNCOND(">> TearDownLink - change fail status: nodeFailStatus "<< ndFailStatus << " nodeId "<< nodeId);
    ndFailStatusTC = true;
    //NS_LOG_UNCOND(">> TearDownLink - change fail status: nodeFailStatusTC "<< ndFailStatusTC << " nodeId "<< nodeId);
    
}

// void TearUpLink (uint32_t interface, Ptr<Node> node, uint32_t nodeId){//Ptr<Node> node
//     //bool& ndFailStatus = nodeFailStatusMap[nodeId];
//     //bool& ndFailStatusTC = nodeFailStatusMapTC[nodeId];
//     node->GetObject<Ipv4>()->SetUp(interface);
//     //NS_LOG_UNCOND(">> TearUpLink: nodeFailStatus "<< ndFailStatus << " nodeId "<< nodeId);
//     //NS_LOG_UNCOND(">> TearUpLink: nodeFailStatusTC "<< ndFailStatusTC << " nodeId "<< nodeId);
// }

void SetHelloComplete(bool* helloComplete) {
    *helloComplete = true;
}

void SetTcComplete(bool* tcComplete) {
    *tcComplete = true;
}

void ReceivePacketHello(Ptr<Socket> socket)
{
    //Node Id that receives the packet
    uint32_t receiverNodeId = socket->GetNode()->GetId(); 
    //cout<< "receiverNodeId " << receiverNodeId << "   ";

    Ptr<Packet> packet;
    Address senderAddress;
    // Read the header
    while ((packet = socket->RecvFrom(senderAddress)))
    {
        NeighHeader nHeader; 
        packet->PeekHeader(nHeader); 
        uint32_t senderNodeId = nHeader.NodeId;
        uint32_t seqNum = nHeader.Seq;

        if(Simulator::Now().GetSeconds() >= intervalHello * listSize)
        {
            //  std::cout << "sizeVec from header: " << nHeader.sizeVec <<"   ";
            //  std::cout << "Neighbors nodeId from header: ";
            //  for (uint32_t i = 0; i < nHeader.sizeVec; i++){ 
            //     std::cout << nHeader.nextHopHeader[i] << " "; 
            //  } 

            //  std::cout << "    linkQ from header: ";
            //  for (uint32_t i = 0; i < nHeader.sizeVec; i++){ 
            //     std::cout << nHeader.cost[i] << " "; 
            //  }
            //  std::cout << std::endl;             

            //Write the neighbors and link quality of 2 hops
            for (uint32_t j=0; j< oneHopNList[receiverNodeId].size(); j++){

                if (oneHopNList[receiverNodeId][j].oneHopNeigh == senderNodeId){
                    oneHopNList[receiverNodeId][j].twoHopNeigh.resize(nHeader.sizeVec, 0);
                    oneHopNList[receiverNodeId][j].twoHopLinkQual.resize(nHeader.sizeVec, 0);

                    for(uint32_t i=0; i<nHeader.sizeVec; i++){
                        oneHopNList[receiverNodeId][j].twoHopNeigh[i] = nHeader.nextHopHeader[i];
                        oneHopNList[receiverNodeId][j].twoHopLinkQual[i] = nHeader.cost[i];

                        if(receiverNodeId == oneHopNList[receiverNodeId][j].twoHopNeigh[i]){//nHeader.nextHopHeader[i]
                            oneHopNList[receiverNodeId][j].etx = float(10000 / float (oneHopNList[receiverNodeId][j].linkQual * oneHopNList[receiverNodeId][j].twoHopLinkQual[i]));
                            //NS_LOG_UNCOND("linkQual: " << oneHopNList[receiverNodeId][j].linkQual << "  twoHoplinkQual: " << oneHopNList[receiverNodeId][j].twoHopLinkQual[i] << "  ETX: " << oneHopNList[receiverNodeId][j].etx);
                        }
                    }
                 }   
            }        
        } 

        // Verify the first time that a packet is received from sender 
        bool senderExists = false;
        uint32_t senderIndex = 0;
        for (const auto &senderInfo : oneHopNList[receiverNodeId]){
            if (senderInfo.oneHopNeigh == senderNodeId){
                senderExists = true;
                //NS_LOG_UNCOND("<- Node " << receiverNodeId << " already has the neighbor " << senderNodeId << " udp " << packet->GetSize());
                break;
            }
            senderIndex++;
        }
        // Add a new neighbor
        if (!senderExists) { 
            SenderInfo newSender;
            newSender.oneHopNeigh = senderNodeId; 
            newSender.receivedPackets.resize(listSize, 0); 
            newSender.lastSeqNum = 0; 
            newSender.linkQual = 0;
            newSender.twoHopNeigh.resize(1, -1);
            newSender.twoHopLinkQual.resize(1, -1);
            newSender.etx = 0;
            newSender.lastTimeRcvHello = 0;
            oneHopNList[receiverNodeId].push_back(newSender);
            senderIndex = oneHopNList[receiverNodeId].size() - 1;
            //NS_LOG_UNCOND("<- Node " << receiverNodeId << " added new neighbor " << senderNodeId << " seqNum " << seqNum << " udp " << packet->GetSize());
        }

        uint32_t indexRecPackets = (seqNum-1)%listSize; 
        uint32_t diffPackets = seqNum - oneHopNList[receiverNodeId][senderIndex].lastSeqNum;
        
        // Count the number of hellos received in the window
        if (diffPackets == 1){                                                              
            oneHopNList[receiverNodeId][senderIndex].receivedPackets[indexRecPackets] = 1; 
        }
        
        if (diffPackets > 1) {
            for(uint32_t i=diffPackets; i>1 ; i--){   
                oneHopNList[receiverNodeId][senderIndex].receivedPackets[(seqNum-i)%listSize] = 0;  
            }  
            oneHopNList[receiverNodeId][senderIndex].receivedPackets[indexRecPackets] = 1; 
        }

        // Store the last sequence number and time received
        if (diffPackets > 0) { // diffPackets < 0 is considered as lost packet
        oneHopNList[receiverNodeId][senderIndex].lastSeqNum = seqNum; 
        oneHopNList[receiverNodeId][senderIndex].lastTimeRcvHello = Simulator::Now().GetSeconds();
        } 
        //NS_LOG_UNCOND ("last Seq Number received: " << oneHopNList[receiverNodeId][senderIndex].lastSeqNum);
        
        // Link Quality count
        uint8_t linkCount = 0; 
        //std::cout << "window (100): ";
        for (uint8_t i = 0; i < listSize; i++) {
            //std::cout << oneHopNList[receiverNodeId][senderIndex].receivedPackets.at(i);
            linkCount+= oneHopNList[receiverNodeId][senderIndex].receivedPackets.at(i);
        } 
        //cout << endl;  
        oneHopNList[receiverNodeId][senderIndex].linkQual = (linkCount*100)/listSize; //Take care with listSize changes
        //NS_LOG_UNCOND("link quality: " << oneHopNList[receiverNodeId][senderIndex].linkQual);
    }
}

static void GenerateTrafficHello(Ptr<Socket> socket, uint16_t pktSize, uint32_t pktCount, 
                                Time pktInterval, bool* helloComplete, float timeToRestoration){
                           
    static EventId HelloGeneratorEvent;                 
    uint32_t NodeId = socket->GetNode()->GetId(); // Transmitter Node Id
    uint16_t pktLossTolerance = 20; 
    Ptr<Packet> p = Create<Packet>(pktSize);

    // Delete neighbor node after pktLossTolerance consecutive lost Hello packets
    for (uint32_t i=0; i < oneHopNList[NodeId].size(); i++){
        if (Simulator::Now().GetSeconds() - oneHopNList[NodeId][i].lastTimeRcvHello >= (intervalHello * pktLossTolerance)){
            //NS_LOG_UNCOND ("oneHopNList[NodeId].size(): " << oneHopNList[NodeId].size() << " \tNode to exclude: " << oneHopNList[NodeId][i].oneHopNeigh);
            oneHopNList[NodeId].erase(oneHopNList[NodeId].begin() + i);
            //NS_LOG_UNCOND ("oneHopNList[NodeId].size() after node exclusion: " << oneHopNList[NodeId].size() );
        }
    }
    
    // uint32_t interfaceNumber = socket->GetNode()->GetObject<Ipv4>()->GetInterfaceForDevice(socket->GetNode()->GetDevice(0));
    //NS_LOG_UNCOND("interface " << interfaceNumber);

    // Restarts the transmission if node failure stops
    for (auto& nodeStatusPair : nodeFailStatusMap) {
    uint32_t node = nodeStatusPair.first;
    bool& failStatus = nodeStatusPair.second;
        if ((NodeId == node) && (failStatus == true) && (Simulator::Now().GetSeconds() >= timeToRestoration)){
            //NS_LOG_UNCOND ("Restarts the node.  FailStatus: "<< failStatus);
            socket->GetNode()->GetObject<Ipv4>()->SetUp(1);
            failStatus = false;
            pktCount = 1;
            //NS_LOG_UNCOND ("Restarts the node.  FailStatus change: "<< nodeFailStatusMap[node]); 
        }
    }

    // Add Header in the packet header  
    NeighHeader neighHeader;
    neighHeader.NodeId = NodeId;
    neighHeader.Seq = pktCount;

    // Reports neighbor nodes and two hop neighbors after full window time
    if(Simulator::Now().GetSeconds() >=  intervalHello * listSize){      
        neighHeader.sizeVec = oneHopNList[NodeId].size();
        for(uint32_t i = 0; i < oneHopNList[NodeId].size(); i++){ 
            neighHeader.nextHopHeader[i] = oneHopNList[NodeId][i].oneHopNeigh;
            neighHeader.cost[i] = oneHopNList[NodeId][i].linkQual;
        }
    }
    p->AddHeader(neighHeader);
    // NS_LOG_UNCOND(" udp " << p->GetSize());
    p->RemoveAtEnd((uint32_t) ((p->GetSize())-pktSize));
    socket->Send(p);

    // Simulate the video stream without the hello messages after calling fitpath.
    if (*helloComplete){
        Simulator::Cancel(HelloGeneratorEvent);
    }
    else {
        // Simulate the hello messages.
        HelloGeneratorEvent = Simulator::Schedule(pktInterval, &GenerateTrafficHello,
                            socket, pktSize, pktCount + 1, pktInterval, helloComplete, timeToRestoration);//, ndFailStatus
    }
    
    /* NS_LOG_UNCOND("----> Node " << NodeId << "  \t\tsent in " << std::fixed << Simulator::Now().ToDouble(Time::S) <<
    "s" << " \tseqNum " << pktCount << " \tudp " << p->GetSize() << endl); */
}

void ReceivePacketTC(Ptr<Socket> socket)
{
    //NS_LOG_UNCOND("received");
    //Node Id that receives the packet
    uint32_t receiverNodeId = socket->GetNode()->GetId(); // Obtém o ID do nó atual que recebe
    //cout<< "receiverNodeId TC: " << receiverNodeId << "  ";

    Ptr<Packet> packet;
    Address senderAddress;
    // Read the header
    while ((packet = socket->RecvFrom(senderAddress)))
    {
        NeighHeader tcHeader; 
        packet->PeekHeader(tcHeader); 
        uint32_t senderNodeId = tcHeader.NodeId;
        //cout << "senderNodeId TC: "<< senderNodeId << "  ";
        uint32_t seqNum = tcHeader.Seq;
        //cout << "seqNum TC: " << seqNum << "   ";
        //std::cout << "sizeVec from TC header: " << tcHeader.sizeVec <<"   ";
            //  std::cout << "Neighbors nodeId from TC header: ";
            //  for (uint32_t i = 0; i < tcHeader.sizeVec; i++){ 
            //     std::cout << tcHeader.nextHopHeader[i] << " "; 
            //  } 

            //  std::cout << "    ETX from TC header: ";
            //  for (uint32_t i = 0; i < tcHeader.sizeVec; i++){ 
            //     std::cout << tcHeader.cost[i] << " "; 
            //  }
            //  std::cout << std::endl;

        // Verify the first time that a packet is received from sender 
        bool senderExists = false;
        uint32_t senderIndex = 0;
        for (const auto &senderInfoTC : topologyControl[receiverNodeId]){
            if (senderInfoTC.nodeTC == tcHeader.NodeId){
                senderExists = true;
                //NS_LOG_UNCOND("<- Node " << receiverNodeId << " has already the neighbor " << senderNodeId << " udp " << packet->GetSize());
                break;
            }
            senderIndex++;
        } 

        // Add a new neighbor
        if (!senderExists) { 
            SenderInfoTC newTC;
            newTC.nodeTC = senderNodeId; 
            newTC.lastSeqNumTC = 0;
            newTC.neighborsTC.resize(tcHeader.sizeVec, 0); 
            newTC.etxTC.resize(tcHeader.sizeVec, 0);
            newTC.lastTimeRcvTC = 0;
            topologyControl[receiverNodeId].push_back(newTC);
            senderIndex = topologyControl[receiverNodeId].size() - 1;            
            //NS_LOG_UNCOND("<- Node " << receiverNodeId << " added new neighbor " << senderNodeId << " seqNum " << seqNum << " udp " << packet->GetSize());
        }
        int diffPackets = seqNum - topologyControl[receiverNodeId][senderIndex].lastSeqNumTC;
        if (diffPackets > 0){
            topologyControl[receiverNodeId][senderIndex].lastSeqNumTC = seqNum;
            topologyControl[receiverNodeId][senderIndex].neighborsTC.resize(tcHeader.sizeVec, 0);
            topologyControl[receiverNodeId][senderIndex].etxTC.resize(tcHeader.sizeVec, 0);
            topologyControl[receiverNodeId][senderIndex].lastTimeRcvTC = Simulator::Now().GetSeconds();
            for(uint32_t i=0; i < tcHeader.sizeVec; i++){
                topologyControl[receiverNodeId][senderIndex].neighborsTC[i] = tcHeader.nextHopHeader[i];
                topologyControl[receiverNodeId][senderIndex].etxTC[i] = tcHeader.cost[i];
            }
        
        Ptr<Socket> forwardSocket = Socket::CreateSocket(socket->GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        forwardSocket->SetAllowBroadcast(true);
        forwardSocket->Connect(InetSocketAddress(Ipv4Address("255.255.255.255"), 8001));
        forwardSocket->Send(packet);
        //NS_LOG_UNCOND("-------> Node " << receiverNodeId << " relays packet from node " << senderNodeId << " seqNum " << seqNum << " udp " << packet->GetSize() << endl);
        }
    }
}

static void GenerateTrafficTC(Ptr<Socket> socket, uint16_t pktSize, uint32_t pktCount, 
                                Time pktInterval, bool* tcComplete, float timeToRestoration){
    //cout<< "====================================================================  GENERATE TRAFFIC (TC) =========================================================================" << endl;                      
    
    static EventId TCGeneratorEvent;  
    uint16_t NodeId = socket->GetNode()->GetId(); // Transmitter Node Id
    uint16_t pktLossTolerance = 8;

    if (pktCount > 0){   

        Ptr<Packet> p = Create<Packet>(pktSize); // 4+4 : the size of Seq=4, NodeId=4

        // Delete node of topologyControl after pktLossTolerance consecutive lost TC packets
        for (uint8_t i=0; i < topologyControl[NodeId].size(); i++){
            if (Simulator::Now().GetSeconds() - topologyControl[NodeId][i].lastTimeRcvTC >= (intervalTC * pktLossTolerance)){
                //NS_LOG_UNCOND ("topologyControl[NodeId].size(): " << topologyControl[NodeId].size() << " \tNode to exclude: " << topologyControl[NodeId][i].nodeTC);
                topologyControl[NodeId].erase(topologyControl[NodeId].begin() + i);
                //NS_LOG_UNCOND ("topologyControl[NodeId].size() after node exclusion: " << topologyControl[NodeId].size());
            }
        }

        // Restarts the transmission if node failure stops
        for (auto& nodeStatusPairTC : nodeFailStatusMapTC) {
        uint32_t node = nodeStatusPairTC.first;
        bool& failStatusTC = nodeStatusPairTC.second;
            if ((NodeId == node) && (failStatusTC == true) && (Simulator::Now().GetSeconds() >= timeToRestoration)){
                //NS_LOG_UNCOND ("FailStatusTC "<< failStatusTC );
                socket->GetNode()->GetObject<Ipv4>()->SetUp(1);
                failStatusTC = false;
                pktCount = 1;
                //NS_LOG_UNCOND ("FailStatusTC change "<< nodeFailStatusMapTC[node]); 
            }
        }

        // Add Header in the packet header  
        NeighHeader topologyControlHeader;
        topologyControlHeader.NodeId = NodeId;
        topologyControlHeader.Seq = pktCount;
       
        //if(Simulator::Now().GetSeconds() >=  intervalHello * listSize){      
            topologyControlHeader.sizeVec = oneHopNList[NodeId].size();
            for(uint8_t i = 0; i < oneHopNList[NodeId].size(); i++){ 
                topologyControlHeader.nextHopHeader[i] = oneHopNList[NodeId][i].oneHopNeigh;
                topologyControlHeader.cost[i] = uint32_t((oneHopNList[NodeId][i].etx)*10000);
            }
        //}
        p->AddHeader(topologyControlHeader);
        p->RemoveAtEnd((uint32_t) ((p->GetSize())-pktSize));
        socket->Send(p);

        // Simulate the video stream without the TC messages after calling fitpath.
        if (*tcComplete){
            Simulator::Cancel(TCGeneratorEvent);
        } 
        else { 
            // Simulate the TC messages
            TCGeneratorEvent = Simulator::Schedule(pktInterval, &GenerateTrafficTC, 
                            socket, pktSize, pktCount + 1, pktInterval, tcComplete, timeToRestoration);//, ndFailStatusTC
        }

        /* NS_LOG_UNCOND("----> NodeTC " << NodeId << "  \tsent in " << std::fixed << Simulator::Now().ToDouble(Time::S) << 
        "s" << " \tseqNumTC " << pktCount << " \tudp " << p->GetSize() << endl); */
    }
}

void PrintTCMessage(int numNodes){ // Print senderListTC for each node
    std::cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= FINAL RESUME (TC) =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    for (int nodeId = 0; nodeId < numNodes; ++nodeId){
        cout << "* Rx Node ID " << nodeId << ":\n";
        //cout << "* vector size: " << topologyControl[nodeId].size() << endl;
        for (auto &senderInfoTC : topologyControl[nodeId]){
            cout << "  > Tx Node ID: " << senderInfoTC.nodeTC << "\t> Last Seq Number Rx: " << senderInfoTC.lastSeqNumTC << "\t> neighborsTC: ";
            for (int hops : senderInfoTC.neighborsTC){
                cout << hops << " ";
            } cout << endl;
            cout << "  > etx: ";
            for (int link : senderInfoTC.etxTC){
                cout << link << " ";
            } cout << endl << endl;
        } cout << endl;
    }
}

void PrintHelloMessage(int numNodes){ // Print senderList for each node
    std::cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= FINAL RESUME (HELLO) =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    for (int nodeId = 0; nodeId < numNodes; ++nodeId){
        cout << "* Rx Node ID " << nodeId << ":" << endl;
        //cout << "neighbor numbers: " << oneHopNList[nodeId].size() << endl;
        for (auto &senderInfo : oneHopNList[nodeId]){
            cout << "  > Tx Node ID: " << senderInfo.oneHopNeigh << "    ";
            // cout << "  > Window 100 last pkt: ";
            // for (int packet : senderInfo.receivedPackets){
            //     cout << packet;
            // }
            cout << "  > Last Seq Number Rx: " << senderInfo.lastSeqNum << "    ";
            cout << "  > link quality: " << senderInfo.linkQual << "    ";
            cout << "  > ETX: " << std::fixed << std::setprecision(4) << senderInfo.etx  << endl;
            cout << "  > twoHopNeigh: ";
            for (int hops : senderInfo.twoHopNeigh){
                cout << hops << " ";
            }
            cout << "     > twoHopLinkQual: ";
            for (int link : senderInfo.twoHopLinkQual){
                cout << link << " ";
            } cout<< endl << endl;
            
        } cout << endl;
    }
}

void PrintAllLinks(){
    // Print Fitpath input
    cout << "Links" << endl; 
    for (auto &tc : topologyControl[0]){
                if (tc.nodeTC == 0){
                    for(uint32_t i=0; i<oneHopNList[0].size(); i++){              
                        cout<< oneHopNList[0][i].oneHopNeigh << "\t0\t" << std::fixed << std::setprecision(4) << oneHopNList[0][i].etx << endl;                                   
                    }
                } 
                for (uint32_t i=0; i<tc.neighborsTC.size(); i++){
                    if (tc.nodeTC!=0){
                    cout << tc.neighborsTC[i] << "\t" << tc.nodeTC << "\t" << std::fixed << std::setprecision(4) << float(tc.etxTC[i])/float(10000) << endl;
                    }
                }
    }   
}

void PrintIntermediateTC(int numNodes){
    std::cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= INTERMEDIATE RESUME (TC) in " << std::fixed << Simulator::Now().ToDouble(Time::S) << " s =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    //for (int nodeId = 0; nodeId < numNodes; ++nodeId)
    //{
        cout << "* Rx Node ID: 0" << ":\n";
        //cout << "* vector size: " << topologyControl[nodeId].size() << endl;
        for (auto &senderInfoTC : topologyControl[0])
        {
            cout << "> Tx Node ID: " << senderInfoTC.nodeTC << " \t> Last Seq Number Rx: " << senderInfoTC.lastSeqNumTC << "\t> neighborsTC: ";
            for (int hops : senderInfoTC.neighborsTC){
                cout << hops << " ";
            } cout << endl;
            cout << "> etx: ";
            for (int link : senderInfoTC.etxTC){
                cout << link << " ";
            } cout << endl << endl;
        } cout<<endl;
    //}
}

void RecallEtx (int transmittingSources, int numNodes, int inst, int refId){
    
    // Generates the FITPATH input file
    static std::ofstream file0;
    char arq_name[80];
    //sprintf(arq_name, "../../inst/links/hall-30/W-HTC/4s-256/%ds-links300_%d-%d-ref%d-recall", transmittingSources, numNodes, inst, refId); //file containing etx
    sprintf(arq_name, "%s%ds-links300_%d-%d-ref%d-recall", baseDirLinks.c_str(), transmittingSources, numNodes, inst, refId); //file containing etx
    file0.open(arq_name, std::ios_base::trunc); 
    if (file0.is_open()) {
        file0 << "Links" << std::endl;
        for (auto& tc : topologyControl[0]) {
            if (tc.nodeTC == 0){
                for(uint32_t i=0; i<oneHopNList[0].size(); i++){
                    if (oneHopNList[0][i].etx >=1 && oneHopNList[0][i].etx < 20){
                        file0 << oneHopNList[0][i].oneHopNeigh << "\t" << tc.nodeTC << "\t" << std::fixed << std::setprecision(4) << oneHopNList[0][i].etx << endl;                                   
                    }
                }   
            } else {
                for (uint32_t i = 0; i < tc.neighborsTC.size(); i++) {
                    if (tc.etxTC[i] >= 10000 && tc.etxTC[i] < 200000){
                        file0 << tc.neighborsTC[i] << "\t" << tc.nodeTC << "\t" << std::fixed << std::setprecision(4) << float(tc.etxTC[i]) / float(10000) << std::endl;
                    }
                }
            }
        }
        std::ifstream file1 ("config-initialization-wo-htc");
        if (file1.is_open()){
            std::string line;
            while(std::getline(file1, line)){
                file0 << line << std::endl;
            }
            file1.close();
        } else{
            std::cerr << "Error opening file1 config-initialization-wo-htc" << std::endl;
        }
        file0.close();  
    } else {
    std::cerr << "Error opening file " << arq_name << std::endl;
    }
}

void PacketsInQueueTrace(){

    // Trace Queue for each node
    for(int i=0; i < numNodesMax+1; i++){
      nodeQueue[i] = 0;
      traceQueue[i] = 0;
   }

    Config::ConnectWithoutContext("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device1PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device2PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device3PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/4/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device4PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device5PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device6PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device7PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/8/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device8PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/9/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device9PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/10/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device10PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/11/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device11PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/12/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device12PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/13/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device13PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/14/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device14PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/15/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device15PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/16/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device16PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/17/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device17PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/18/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device18PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/19/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device19PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/20/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device20PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/21/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device21PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/22/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device22PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/23/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device23PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/24/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device24PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/25/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device25PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/26/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device26PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/27/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device27PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/28/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device28PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/29/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device29PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/30/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device30PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/31/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device31PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/32/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device32PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/33/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device33PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/34/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device34PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/35/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device35PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/36/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device36PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/37/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device37PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/38/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device38PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/39/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device39PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/40/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device40PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/41/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device41PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/42/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device42PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/43/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device43PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/44/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device44PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/45/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device45PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/46/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device46PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/47/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device47PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/48/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device48PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/49/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device49PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/50/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device50PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/51/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device51PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/52/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device52PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/53/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device53PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/54/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device54PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/55/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device55PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/56/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device56PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/57/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device57PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/58/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device58PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/59/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device59PacketsInQueueTrace));
    Config::ConnectWithoutContext("/NodeList/60/DeviceList/*/$ns3::WifiNetDevice/Mac/*/Queue/PacketsInQueue", MakeCallback(&Device60PacketsInQueueTrace));

}

void CallInputFitPath (int numNodes, int inst, int refId, NodeContainer adhocNodes, Ipv4InterfaceContainer adhocInterfaces, uint16_t pktSizeEval){ 
    
    LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
    LogComponentEnable ("EvalvidServer", LOG_LEVEL_INFO);

    // Get the Sources vector and TraceCodes vector
    std::ifstream file2("config-initialization-wo-htc"); 
    std::vector<uint32_t> sources;
    std::vector<uint32_t> flowTimes;
    sources.clear();
    flowTimes.clear();
    
    if (file2.is_open()){
        std::vector<uint32_t> flowTimesAll;
        std::unordered_set<uint32_t> uniqueSources;
        std::vector<uint32_t> indexSourceVec;
        std::string line;
        bool isSource = false;
        bool isFlowTime = false;
        int index = 0;
        flowTimesAll.clear();
        uniqueSources.clear();
        indexSourceVec.clear();

        while (std::getline(file2, line)) {
            if (line == "Source") {
            isSource = true;
            continue;
            }
            if (line == "Destination") {
            isSource = false;
            continue;
            }
            if (line == "FlowTime") {
            isSource = false;
            isFlowTime = true;
            continue;
            }

            if (isSource) {
                uint32_t source;
                if (std::istringstream(line) >> source) {
                    // Verify if the source has not been included in uniqueSources
                    if (uniqueSources.find(source) == uniqueSources.end()) { 
                        uniqueSources.insert(source);
                        sources.push_back(source);
                        indexSourceVec.push_back(index);
                        //cout<< "indexVec added "<< index << endl;
                    }
                    index++;
                //cout << "indexVec counter " << index << endl;
                }
            }

            if (isFlowTime) {
                uint32_t flowTime;
                if (std::istringstream(line) >> flowTime){
                    flowTimesAll.push_back(flowTime);
                }
            }
        }
        //NS_LOG_UNCOND ("indexVec size "<< indexSourceVec.size());
        //cout << "indexVec "<< " ";
            for (uint32_t i=0; i< indexSourceVec.size(); i++){
                //cout << indexSourceVec[i] << " ";
                flowTimes.push_back(flowTimesAll[indexSourceVec[i]]);
            } 
            //cout << endl;

        // cout << "FlowTimeAll: ";
        // for (uint32_t flow : flowTimesAll) {
        //     cout << flow << " ";
        // } cout << endl;
    } else {
        std::cerr << "Error opening file2 config-initialization-wo-htc" << std::endl;
    }

    std::ifstream file3("config-trace");
    std::vector<int> traceCode;
    traceCode.clear();

    if (file3.is_open()){
        std::string lineConfigTrace;
        std::vector<std::string> linesConfigTrace;
        linesConfigTrace.clear();

        while (std::getline(file3, lineConfigTrace)) {
            linesConfigTrace.push_back(lineConfigTrace);
        }
        file3.close();

        for (auto& flowTime : flowTimes) {
            for (auto& l : linesConfigTrace) {
                std::istringstream iss(l);
                uint32_t ft;
                int tc;
                iss >> ft >> tc;
                if (ft == flowTime) {
                    traceCode.push_back(tc);
                    break;
                }
            }
        }
    } else {
        std::cerr << "Error opening file3 config-trace" << std::endl;
    }

    // Print Source and TraceCode
    cout << "Source: ";
    for (uint32_t source : sources) {
        cout << source << " ";
    } cout << endl;

    /* cout << "FlowTime: ";
    for (uint32_t flowTime : flowTimes) {
        cout << flowTime << " ";
    } cout << endl; */

    cout << "TraceCode: ";
    for (int tc : traceCode) {
        std::cout << tc << " ";
    } cout << endl;

    // Generates the FITPATH input file
    int transmittingSources = sources.size();
    static std::ofstream file0;
    char arq_name[70];
    //sprintf(arq_name, "../../inst/links/hall-30/W-HTC/4s-256/%ds-links300_%d-%d-ref%d", transmittingSources, numNodes, inst, refId); //file containing etx
    sprintf(arq_name, "%s%ds-links300_%d-%d-ref%d", baseDirLinks.c_str(), transmittingSources, numNodes, inst, refId); //file containing etx
    file0.open(arq_name, std::ios_base::trunc); 
    if (file0.is_open()) {
        file0 << "Links" << std::endl;
        for (auto& tc : topologyControl[0]) {
            if (tc.nodeTC == 0){
                for(uint32_t i=0; i<oneHopNList[0].size(); i++){
                    if (oneHopNList[0][i].etx >=1 && oneHopNList[0][i].etx < 20){
                        file0 << oneHopNList[0][i].oneHopNeigh << "\t" << tc.nodeTC << "\t" << std::fixed << std::setprecision(4) << oneHopNList[0][i].etx << endl;                                   
                    }
                }   
            } else {
                for (uint32_t i = 0; i < tc.neighborsTC.size(); i++) {
                    if (tc.etxTC[i] >= 10000 && tc.etxTC[i] < 200000){
                        file0 << tc.neighborsTC[i] << "\t" << tc.nodeTC << "\t" << std::fixed << std::setprecision(4) << float(tc.etxTC[i]) / float(10000) << std::endl;
                    }
                }
            }
        }

        std::ifstream file1 ("config-initialization-wo-htc");
        if (file1.is_open()){
            std::string line;
            while(std::getline(file1, line)){
                file0 << line << std::endl;
            }
            file1.close();
        } else{
            std::cerr << "Error opening file1 config-initialization-wo-htc" << std::endl;
        }
        file0.close();  
    } else {
    std::cerr << "Error opening file " << arq_name << std::endl;
    }

    // Call FITPATH
    //std::string command = "cd ../../FITPATH2/ && ./runHeuristics.sh";
    //system(command.c_str());
    char command[200];
    // sprintf(command, "cd ../../FITPATH2/ && ./heuristicILS_mate ../inst/links/hall-30/W-HTC/4s-256/%ds-links300_%d-%d-ref%d %d %d %d", 
    //                                             transmittingSources, numNodes, inst, refId, numNodes, inst, refId); 

    sprintf(command, "cd ../../FITPATH2/ && ./heuristicILS_mate %s%ds-links300_%d-%d-ref%d %d %d %d", 
             baseDirInputFitpath.c_str(), transmittingSources, numNodes, inst, refId, numNodes, inst, refId); 
    system(command);  

   //PrintAllLinks();
   //PrintIntermediateTC(int numNodes);

    cout << "Source: ";
    for (uint32_t source : sources) {
        cout << source << " ";
    } cout << endl;

    cout << "TraceCode: ";
    for (int tc : traceCode) {
        std::cout << tc << " ";
    } cout << endl;

    //Simulator::Schedule (Seconds(7), &RecallEtx, transmittingSources, numNodes, inst, refId);

    NS_LOG_INFO ("Create Evalvid Applications.");

    ApplicationContainer apps;
    ApplicationContainer temp;
    int flow;
    std::stringstream fileName;
    std::stringstream pathsFileName;
  
    //for (uint32_t i=sources[0]; i<= sources[sources.size()-1]; i++){
    for (uint32_t j=0; j< sources.size(); j++){
        int i = sources[j];
        std::stringstream fid_s, fid_d;  
        //Destination
        int port = 2*i-1; 

        fid_d << inst << refId << 10+(2*i-1); //ex: 1013 (inst=1; ref=0; flow=13 (client -> server = 1; port=3))
        fid_d >> flow;
    
        fileName.str("");
        fileName << baseDirEvalvid << "rd-"<<inst<<"-"<<refId<<"-" << i; // Frames received
        
        pathsFileName.str("");
        pathsFileName << baseDirOutputFitpath << numNodes << "-" << inst << "-ref" << refId;
        
        EvalvidClientHelper sink1 (adhocInterfaces.GetAddress (i),port);
        sink1.SetAttribute ("ReceiverDumpFilename", StringValue(fileName.str()));
        sink1.SetAttribute ("PathsFileName", StringValue(pathsFileName.str()));
        sink1.SetAttribute ("FlowId",UintegerValue(flow));
        sink1.SetAttribute ("TraceCode", UintegerValue(traceCode[j]));
        apps = sink1.Install (adhocNodes.Get (0));

        apps.Start (Seconds (0.0)); 
        apps.Stop (Seconds (40.0));
    
        //Source
        fid_s << inst << refId << "0" << 2*i-1; // ex: 1003 (inst=1; ref=0; flow=03 (server -> client = 0; port = 3)) 18001
        fid_s >> flow;
  	
        fileName.str("");
        fileName << baseDirEvalvid << "sd-"<<inst<<"-"<<refId<<"-" << i; // frames sent

        EvalvidServerHelper source1 (port);
        //source1.SetAttribute ("SenderTraceFilename", StringValue("st_CBR"));
        source1.SetAttribute ("SenderDumpFilename", StringValue(fileName.str()));
        source1.SetAttribute ("PacketPayload",UintegerValue(pktSizeEval));
        source1.SetAttribute ("FlowId",UintegerValue(flow));
        source1.SetAttribute ("Ref", UintegerValue(refId));
        for (int n=1; n < numNodes; n++){
            apps = source1.Install (adhocNodes.Get(n)); // Installed on all sources for source routing
        }
	}   

    PacketsInQueueTrace(); 
    
}

int main(int argc, char *argv[])
{
    //LogComponentEnable("YansWifiChannel", LOG_LEVEL_INFO);
    LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
    LogComponentEnable ("EvalvidServer", LOG_LEVEL_INFO);
    
    string phyMode("ErpOfdmRate18Mbps");
    bool verbose = false;
    bool HelloComplete = false;
    bool TcComplete = false;
    uint16_t packetSize = 360; // bytes
    uint16_t packetSizeEvalvid = 1024;

    float FitpathCallTime = 400.0;
    float TotalTime = 440.0;

    float timeToFailure = 204.0; // Nodes failure time 
    float timeToRestoration = timeToFailure + 24.0; // Time to restore the nodes
    for (uint32_t i = 0; i < numNodesMax; i++) {
        nodeFailStatusMap[i] = false;
        nodeFailStatusMapTC[i] = false;
    }
    
    // int numNodes = 30;
    // int INST = 1;
    // int ref = 0;
    int numNodes = atoi(argv[1]);
    int INST = atoi(argv[2]); //instância
    int ref = atoi(argv[3]); //referência

    NS_LOG_UNCOND (ref << " " << INST << " " << packetSizeEvalvid << " " << timeToFailure);

    CommandLine cmd;
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("intervalHello", "interval (seconds) between packets", intervalHello);
    cmd.AddValue("verbose", "turn on all WifiNetDevice log components", verbose);
    cmd.Parse(argc, argv);

    // Convert to time object
    Time interPacketIntervalHello = Seconds(intervalHello);
    Time interPacketIntervalTC = Seconds(intervalTC);

    // Fix non-unicast data rate to be the same as that of unicast
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue(phyMode));

    NodeContainer c;
    c.Create(numNodes);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    if (verbose){
        wifi.EnableLogComponents(); // Turn on all Wifi logging
    }
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::Cost231PropagationLossModel");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a mac and disable rate control
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> initialAlloc = CreateObject<ListPositionAllocator>();

if(INST==1){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (177 , 115, 3));
initialAlloc->Add (Vector (293 , 235, 3));
initialAlloc->Add (Vector (286 , 192, 3));
initialAlloc->Add (Vector (249 , 121, 3));
initialAlloc->Add (Vector (62 , 127, 3));
initialAlloc->Add (Vector (182 , 230, 3));
initialAlloc->Add (Vector (62 , 223, 3));
initialAlloc->Add (Vector (222 , 258, 3));
initialAlloc->Add (Vector (269 , 267, 3));
initialAlloc->Add (Vector (211 , 142, 3));
initialAlloc->Add (Vector (129 , 273, 3));
initialAlloc->Add (Vector (284 , 37, 3));
initialAlloc->Add (Vector (98 , 224, 3));
initialAlloc->Add (Vector (91 , 80, 3));
initialAlloc->Add (Vector (56 , 273, 3));
initialAlloc->Add (Vector (62 , 170, 3));
initialAlloc->Add (Vector (196 , 181, 3));
initialAlloc->Add (Vector (236 , 5, 3));
initialAlloc->Add (Vector (13 , 257, 3));
initialAlloc->Add (Vector (24 , 195, 3));
initialAlloc->Add (Vector (134 , 164, 3));
initialAlloc->Add (Vector (243 , 50, 3));
initialAlloc->Add (Vector (188 , 84, 3));
initialAlloc->Add (Vector (254 , 199, 3));
initialAlloc->Add (Vector (132 , 60, 3));
initialAlloc->Add (Vector (139 , 112, 3));
initialAlloc->Add (Vector (167 , 201, 3));
initialAlloc->Add (Vector (197 , 2, 3));
initialAlloc->Add (Vector (117 , 192, 3));
initialAlloc->Add (Vector (52 , 56, 3));
initialAlloc->Add (Vector (297 , 71, 3));
initialAlloc->Add (Vector (32 , 29, 3));
initialAlloc->Add (Vector (96 , 23, 3));
initialAlloc->Add (Vector (243 , 291, 3));
initialAlloc->Add (Vector (137 , 28, 3));
initialAlloc->Add (Vector (158 , 295, 3));
initialAlloc->Add (Vector (206 , 40, 3));
initialAlloc->Add (Vector (282 , 158, 3));
initialAlloc->Add (Vector (199 , 281, 3));
initialAlloc->Add (Vector (98 , 113, 3));
initialAlloc->Add (Vector (98 , 290, 3));
initialAlloc->Add (Vector (24 , 290, 3));
initialAlloc->Add (Vector (284 , 121, 3));
initialAlloc->Add (Vector (36 , 108, 3));
initialAlloc->Add (Vector (143 , 235, 3));
initialAlloc->Add (Vector (238 , 166, 3));
initialAlloc->Add (Vector (93 , 258, 3));
initialAlloc->Add (Vector (296 , 4, 3));
initialAlloc->Add (Vector (225 , 91, 3));
initialAlloc->Add (Vector (223 , 224, 3));
initialAlloc->Add (Vector (165 , 147, 3));
initialAlloc->Add (Vector (170 , 56, 3));
initialAlloc->Add (Vector (33 , 143, 3));
initialAlloc->Add (Vector (66 , 11, 3));
initialAlloc->Add (Vector (8 , 223, 3));
}
if(INST==2){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (8 , 223, 3));
initialAlloc->Add (Vector (90 , 19, 3));
initialAlloc->Add (Vector (188 , 275, 3));
initialAlloc->Add (Vector (261 , 198, 3));
initialAlloc->Add (Vector (145 , 27, 3));
initialAlloc->Add (Vector (142 , 279, 3));
initialAlloc->Add (Vector (249 , 293, 3));
initialAlloc->Add (Vector (267 , 230, 3));
initialAlloc->Add (Vector (2 , 272, 3));
initialAlloc->Add (Vector (67 , 91, 3));
initialAlloc->Add (Vector (110 , 230, 3));
initialAlloc->Add (Vector (96 , 63, 3));
initialAlloc->Add (Vector (213 , 186, 3));
initialAlloc->Add (Vector (161 , 96, 3));
initialAlloc->Add (Vector (52 , 177, 3));
initialAlloc->Add (Vector (90 , 175, 3));
initialAlloc->Add (Vector (44 , 145, 3));
initialAlloc->Add (Vector (199 , 66, 3));
initialAlloc->Add (Vector (60 , 266, 3));
initialAlloc->Add (Vector (209 , 223, 3));
initialAlloc->Add (Vector (248 , 5, 3));
initialAlloc->Add (Vector (286 , 162, 3));
initialAlloc->Add (Vector (243 , 121, 3));
initialAlloc->Add (Vector (137 , 204, 3));
initialAlloc->Add (Vector (208 , 18, 3));
initialAlloc->Add (Vector (298 , 18, 3));
initialAlloc->Add (Vector (288 , 77, 3));
initialAlloc->Add (Vector (123 , 134, 3));
initialAlloc->Add (Vector (222 , 272, 3));
initialAlloc->Add (Vector (209 , 116, 3));
initialAlloc->Add (Vector (175 , 145, 3));
initialAlloc->Add (Vector (239 , 56, 3));
initialAlloc->Add (Vector (29 , 16, 3));
initialAlloc->Add (Vector (114 , 100, 3));
initialAlloc->Add (Vector (157 , 176, 3));
initialAlloc->Add (Vector (269 , 41, 3));
initialAlloc->Add (Vector (126 , 1, 3));
initialAlloc->Add (Vector (32 , 86, 3));
initialAlloc->Add (Vector (151 , 241, 3));
initialAlloc->Add (Vector (95 , 275, 3));
initialAlloc->Add (Vector (46 , 229, 3));
initialAlloc->Add (Vector (297 , 120, 3));
initialAlloc->Add (Vector (45 , 49, 3));
initialAlloc->Add (Vector (246 , 88, 3));
initialAlloc->Add (Vector (172 , 215, 3));
initialAlloc->Add (Vector (242 , 172, 3));
initialAlloc->Add (Vector (4 , 187, 3));
initialAlloc->Add (Vector (34 , 286, 3));
initialAlloc->Add (Vector (164 , 57, 3));
initialAlloc->Add (Vector (78 , 129, 3));
initialAlloc->Add (Vector (297 , 206, 3));
initialAlloc->Add (Vector (61 , 1, 3));
initialAlloc->Add (Vector (265 , 265, 3));
initialAlloc->Add (Vector (131 , 62, 3));
initialAlloc->Add (Vector (299 , 265, 3));
}
if(INST==3){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (299 , 265, 3));
initialAlloc->Add (Vector (246 , 85, 3));
initialAlloc->Add (Vector (168 , 240, 3));
initialAlloc->Add (Vector (225 , 40, 3));
initialAlloc->Add (Vector (72 , 76, 3));
initialAlloc->Add (Vector (58 , 124, 3));
initialAlloc->Add (Vector (93 , 139, 3));
initialAlloc->Add (Vector (202 , 141, 3));
initialAlloc->Add (Vector (264 , 175, 3));
initialAlloc->Add (Vector (1 , 195, 3));
initialAlloc->Add (Vector (115 , 42, 3));
initialAlloc->Add (Vector (238 , 258, 3));
initialAlloc->Add (Vector (268 , 298, 3));
initialAlloc->Add (Vector (136 , 74, 3));
initialAlloc->Add (Vector (115 , 278, 3));
initialAlloc->Add (Vector (94 , 248, 3));
initialAlloc->Add (Vector (36 , 185, 3));
initialAlloc->Add (Vector (86 , 4, 3));
initialAlloc->Add (Vector (67 , 177, 3));
initialAlloc->Add (Vector (133 , 182, 3));
initialAlloc->Add (Vector (50 , 2, 3));
initialAlloc->Add (Vector (190 , 111, 3));
initialAlloc->Add (Vector (42 , 271, 3));
initialAlloc->Add (Vector (215 , 216, 3));
initialAlloc->Add (Vector (206 , 177, 3));
initialAlloc->Add (Vector (126 , 240, 3));
initialAlloc->Add (Vector (91 , 213, 3));
initialAlloc->Add (Vector (297 , 26, 3));
initialAlloc->Add (Vector (206 , 268, 3));
initialAlloc->Add (Vector (156 , 105, 3));
initialAlloc->Add (Vector (6 , 258, 3));
initialAlloc->Add (Vector (70 , 298, 3));
initialAlloc->Add (Vector (201 , 15, 3));
initialAlloc->Add (Vector (48 , 54, 3));
initialAlloc->Add (Vector (285 , 213, 3));
initialAlloc->Add (Vector (184 , 78, 3));
initialAlloc->Add (Vector (243 , 127, 3));
initialAlloc->Add (Vector (144 , 213, 3));
initialAlloc->Add (Vector (157 , 299, 3));
initialAlloc->Add (Vector (263 , 44, 3));
initialAlloc->Add (Vector (138 , 150, 3));
initialAlloc->Add (Vector (44 , 222, 3));
initialAlloc->Add (Vector (248 , 220, 3));
initialAlloc->Add (Vector (261 , 2, 3));
initialAlloc->Add (Vector (134 , 12, 3));
initialAlloc->Add (Vector (281 , 90, 3));
initialAlloc->Add (Vector (101 , 173, 3));
initialAlloc->Add (Vector (174 , 194, 3));
initialAlloc->Add (Vector (101 , 99, 3));
initialAlloc->Add (Vector (34 , 84, 3));
initialAlloc->Add (Vector (174 , 37, 3));
initialAlloc->Add (Vector (291 , 136, 3));
initialAlloc->Add (Vector (296 , 169, 3));
initialAlloc->Add (Vector (24 , 27, 3));
initialAlloc->Add (Vector (10 , 291, 3));
}
if(INST==4){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (10 , 291, 3));
initialAlloc->Add (Vector (274 , 26, 3));
initialAlloc->Add (Vector (163 , 37, 3));
initialAlloc->Add (Vector (225 , 163, 3));
initialAlloc->Add (Vector (128 , 185, 3));
initialAlloc->Add (Vector (44 , 54, 3));
initialAlloc->Add (Vector (103 , 114, 3));
initialAlloc->Add (Vector (202 , 241, 3));
initialAlloc->Add (Vector (168 , 279, 3));
initialAlloc->Add (Vector (273 , 118, 3));
initialAlloc->Add (Vector (222 , 82, 3));
initialAlloc->Add (Vector (120 , 220, 3));
initialAlloc->Add (Vector (30 , 173, 3));
initialAlloc->Add (Vector (83 , 222, 3));
initialAlloc->Add (Vector (276 , 275, 3));
initialAlloc->Add (Vector (44 , 131, 3));
initialAlloc->Add (Vector (142 , 298, 3));
initialAlloc->Add (Vector (147 , 89, 3));
initialAlloc->Add (Vector (186 , 95, 3));
initialAlloc->Add (Vector (291 , 239, 3));
initialAlloc->Add (Vector (100 , 146, 3));
initialAlloc->Add (Vector (67 , 160, 3));
initialAlloc->Add (Vector (105 , 5, 3));
initialAlloc->Add (Vector (63 , 24, 3));
initialAlloc->Add (Vector (200 , 294, 3));
initialAlloc->Add (Vector (177 , 139, 3));
initialAlloc->Add (Vector (285 , 185, 3));
initialAlloc->Add (Vector (93 , 278, 3));
initialAlloc->Add (Vector (277 , 152, 3));
initialAlloc->Add (Vector (84 , 63, 3));
initialAlloc->Add (Vector (262 , 83, 3));
initialAlloc->Add (Vector (238 , 4, 3));
initialAlloc->Add (Vector (236 , 209, 3));
initialAlloc->Add (Vector (59 , 276, 3));
initialAlloc->Add (Vector (146 , 254, 3));
initialAlloc->Add (Vector (188 , 178, 3));
initialAlloc->Add (Vector (173 , 5, 3));
initialAlloc->Add (Vector (38 , 239, 3));
initialAlloc->Add (Vector (49 , 99, 3));
initialAlloc->Add (Vector (289 , 65, 3));
initialAlloc->Add (Vector (219 , 115, 3));
initialAlloc->Add (Vector (247 , 50, 3));
initialAlloc->Add (Vector (129 , 46, 3));
initialAlloc->Add (Vector (146 , 157, 3));
initialAlloc->Add (Vector (235 , 262, 3));
initialAlloc->Add (Vector (7 , 258, 3));
initialAlloc->Add (Vector (23 , 210, 3));
initialAlloc->Add (Vector (164 , 203, 3));
initialAlloc->Add (Vector (79 , 190, 3));
initialAlloc->Add (Vector (195 , 56, 3));
initialAlloc->Add (Vector (137 , 1, 3));
initialAlloc->Add (Vector (36 , 3, 3));
initialAlloc->Add (Vector (144 , 125, 3));
initialAlloc->Add (Vector (197 , 209, 3));
initialAlloc->Add (Vector (268 , 214, 3));
}
if(INST==5){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (268 , 214, 3));
initialAlloc->Add (Vector (175 , 65, 3));
initialAlloc->Add (Vector (210 , 72, 3));
initialAlloc->Add (Vector (176 , 232, 3));
initialAlloc->Add (Vector (20 , 249, 3));
initialAlloc->Add (Vector (273 , 181, 3));
initialAlloc->Add (Vector (126 , 248, 3));
initialAlloc->Add (Vector (60 , 198, 3));
initialAlloc->Add (Vector (220 , 126, 3));
initialAlloc->Add (Vector (279 , 273, 3));
initialAlloc->Add (Vector (231 , 260, 3));
initialAlloc->Add (Vector (283 , 73, 3));
initialAlloc->Add (Vector (246 , 27, 3));
initialAlloc->Add (Vector (99 , 61, 3));
initialAlloc->Add (Vector (140 , 171, 3));
initialAlloc->Add (Vector (53 , 137, 3));
initialAlloc->Add (Vector (93 , 15, 3));
initialAlloc->Add (Vector (88 , 273, 3));
initialAlloc->Add (Vector (190 , 144, 3));
initialAlloc->Add (Vector (287 , 3, 3));
initialAlloc->Add (Vector (61 , 74, 3));
initialAlloc->Add (Vector (178 , 298, 3));
initialAlloc->Add (Vector (194 , 186, 3));
initialAlloc->Add (Vector (52 , 234, 3));
initialAlloc->Add (Vector (91 , 189, 3));
initialAlloc->Add (Vector (232 , 207, 3));
initialAlloc->Add (Vector (94 , 221, 3));
initialAlloc->Add (Vector (214 , 26, 3));
initialAlloc->Add (Vector (134 , 21, 3));
initialAlloc->Add (Vector (297 , 243, 3));
initialAlloc->Add (Vector (183 , 15, 3));
initialAlloc->Add (Vector (298 , 121, 3));
initialAlloc->Add (Vector (246 , 107, 3));
initialAlloc->Add (Vector (117 , 290, 3));
initialAlloc->Add (Vector (55 , 18, 3));
initialAlloc->Add (Vector (134 , 217, 3));
initialAlloc->Add (Vector (119 , 127, 3));
initialAlloc->Add (Vector (91 , 108, 3));
initialAlloc->Add (Vector (152 , 135, 3));
initialAlloc->Add (Vector (95 , 157, 3));
initialAlloc->Add (Vector (139 , 99, 3));
initialAlloc->Add (Vector (248 , 289, 3));
initialAlloc->Add (Vector (137 , 65, 3));
initialAlloc->Add (Vector (170 , 264, 3));
initialAlloc->Add (Vector (255 , 151, 3));
initialAlloc->Add (Vector (6 , 288, 3));
initialAlloc->Add (Vector (40 , 47, 3));
initialAlloc->Add (Vector (68 , 299, 3));
initialAlloc->Add (Vector (37 , 168, 3));
initialAlloc->Add (Vector (246 , 68, 3));
initialAlloc->Add (Vector (51 , 271, 3));
initialAlloc->Add (Vector (12 , 11, 3));
initialAlloc->Add (Vector (280 , 37, 3));
initialAlloc->Add (Vector (230 , 171, 3));
initialAlloc->Add (Vector (35 , 97, 3));
}
if(INST==6){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (35 , 97, 3));
initialAlloc->Add (Vector (141 , 85, 3));
initialAlloc->Add (Vector (112 , 65, 3));
initialAlloc->Add (Vector (8 , 185, 3));
initialAlloc->Add (Vector (286 , 243, 3));
initialAlloc->Add (Vector (202 , 178, 3));
initialAlloc->Add (Vector (35 , 223, 3));
initialAlloc->Add (Vector (105 , 293, 3));
initialAlloc->Add (Vector (123 , 244, 3));
initialAlloc->Add (Vector (51 , 170, 3));
initialAlloc->Add (Vector (120 , 28, 3));
initialAlloc->Add (Vector (257 , 75, 3));
initialAlloc->Add (Vector (212 , 219, 3));
initialAlloc->Add (Vector (156 , 254, 3));
initialAlloc->Add (Vector (216 , 59, 3));
initialAlloc->Add (Vector (34 , 42, 3));
initialAlloc->Add (Vector (297 , 176, 3));
initialAlloc->Add (Vector (159 , 298, 3));
initialAlloc->Add (Vector (69 , 282, 3));
initialAlloc->Add (Vector (73 , 108, 3));
initialAlloc->Add (Vector (258 , 6, 3));
initialAlloc->Add (Vector (99 , 174, 3));
initialAlloc->Add (Vector (199 , 101, 3));
initialAlloc->Add (Vector (29 , 267, 3));
initialAlloc->Add (Vector (247 , 150, 3));
initialAlloc->Add (Vector (2 , 293, 3));
initialAlloc->Add (Vector (206 , 297, 3));
initialAlloc->Add (Vector (224 , 265, 3));
initialAlloc->Add (Vector (299 , 8, 3));
initialAlloc->Add (Vector (2 , 136, 3));
initialAlloc->Add (Vector (295 , 84, 3));
initialAlloc->Add (Vector (112 , 121, 3));
initialAlloc->Add (Vector (88 , 205, 3));
initialAlloc->Add (Vector (256 , 276, 3));
initialAlloc->Add (Vector (211 , 13, 3));
initialAlloc->Add (Vector (169 , 169, 3));
initialAlloc->Add (Vector (151 , 207, 3));
initialAlloc->Add (Vector (63 , 70, 3));
initialAlloc->Add (Vector (135 , 159, 3));
initialAlloc->Add (Vector (43 , 129, 3));
initialAlloc->Add (Vector (251 , 206, 3));
initialAlloc->Add (Vector (189 , 41, 3));
initialAlloc->Add (Vector (223 , 124, 3));
initialAlloc->Add (Vector (185 , 137, 3));
initialAlloc->Add (Vector (81 , 251, 3));
initialAlloc->Add (Vector (86 , 44, 3));
initialAlloc->Add (Vector (287 , 122, 3));
initialAlloc->Add (Vector (189 , 249, 3));
initialAlloc->Add (Vector (260 , 42, 3));
initialAlloc->Add (Vector (145 , 124, 3));
initialAlloc->Add (Vector (0 , 98, 3));
initialAlloc->Add (Vector (1 , 249, 3));
initialAlloc->Add (Vector (142 , 53, 3));
initialAlloc->Add (Vector (243 , 238, 3));
initialAlloc->Add (Vector (72 , 145, 3));
}
if(INST==7){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (72 , 145, 3));
initialAlloc->Add (Vector (177 , 99, 3));
initialAlloc->Add (Vector (125 , 243, 3));
initialAlloc->Add (Vector (86 , 97, 3));
initialAlloc->Add (Vector (0 , 253, 3));
initialAlloc->Add (Vector (215 , 121, 3));
initialAlloc->Add (Vector (31 , 126, 3));
initialAlloc->Add (Vector (265 , 120, 3));
initialAlloc->Add (Vector (178 , 146, 3));
initialAlloc->Add (Vector (170 , 36, 3));
initialAlloc->Add (Vector (49 , 184, 3));
initialAlloc->Add (Vector (157 , 230, 3));
initialAlloc->Add (Vector (213 , 34, 3));
initialAlloc->Add (Vector (30 , 212, 3));
initialAlloc->Add (Vector (6 , 155, 3));
initialAlloc->Add (Vector (253 , 208, 3));
initialAlloc->Add (Vector (97 , 220, 3));
initialAlloc->Add (Vector (98 , 47, 3));
initialAlloc->Add (Vector (186 , 1, 3));
initialAlloc->Add (Vector (205 , 235, 3));
initialAlloc->Add (Vector (237 , 266, 3));
initialAlloc->Add (Vector (129 , 278, 3));
initialAlloc->Add (Vector (186 , 265, 3));
initialAlloc->Add (Vector (83 , 191, 3));
initialAlloc->Add (Vector (106 , 166, 3));
initialAlloc->Add (Vector (290 , 51, 3));
initialAlloc->Add (Vector (127 , 194, 3));
initialAlloc->Add (Vector (141 , 75, 3));
initialAlloc->Add (Vector (63 , 293, 3));
initialAlloc->Add (Vector (47 , 92, 3));
initialAlloc->Add (Vector (258 , 154, 3));
initialAlloc->Add (Vector (209 , 160, 3));
initialAlloc->Add (Vector (170 , 189, 3));
initialAlloc->Add (Vector (119 , 125, 3));
initialAlloc->Add (Vector (5 , 34, 3));
initialAlloc->Add (Vector (293 , 201, 3));
initialAlloc->Add (Vector (91 , 272, 3));
initialAlloc->Add (Vector (234 , 76, 3));
initialAlloc->Add (Vector (284 , 17, 3));
initialAlloc->Add (Vector (299 , 146, 3));
initialAlloc->Add (Vector (252 , 16, 3));
initialAlloc->Add (Vector (272 , 243, 3));
initialAlloc->Add (Vector (5 , 82, 3));
initialAlloc->Add (Vector (38 , 258, 3));
initialAlloc->Add (Vector (40 , 58, 3));
initialAlloc->Add (Vector (289 , 91, 3));
initialAlloc->Add (Vector (66 , 230, 3));
initialAlloc->Add (Vector (202 , 65, 3));
initialAlloc->Add (Vector (273 , 276, 3));
initialAlloc->Add (Vector (206 , 292, 3));
initialAlloc->Add (Vector (156 , 298, 3));
initialAlloc->Add (Vector (68 , 30, 3));
initialAlloc->Add (Vector (143 , 159, 3));
initialAlloc->Add (Vector (13 , 283, 3));
initialAlloc->Add (Vector (122 , 24, 3));
}
if(INST==8){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (122 , 24, 3));
initialAlloc->Add (Vector (196 , 144, 3));
initialAlloc->Add (Vector (42 , 149, 3));
initialAlloc->Add (Vector (111 , 193, 3));
initialAlloc->Add (Vector (82 , 221, 3));
initialAlloc->Add (Vector (177 , 213, 3));
initialAlloc->Add (Vector (247 , 81, 3));
initialAlloc->Add (Vector (196 , 47, 3));
initialAlloc->Add (Vector (255 , 194, 3));
initialAlloc->Add (Vector (0 , 298, 3));
initialAlloc->Add (Vector (297 , 262, 3));
initialAlloc->Add (Vector (134 , 137, 3));
initialAlloc->Add (Vector (55 , 183, 3));
initialAlloc->Add (Vector (153 , 252, 3));
initialAlloc->Add (Vector (203 , 97, 3));
initialAlloc->Add (Vector (209 , 208, 3));
initialAlloc->Add (Vector (199 , 262, 3));
initialAlloc->Add (Vector (169 , 281, 3));
initialAlloc->Add (Vector (277 , 226, 3));
initialAlloc->Add (Vector (110 , 252, 3));
initialAlloc->Add (Vector (8 , 126, 3));
initialAlloc->Add (Vector (163 , 51, 3));
initialAlloc->Add (Vector (236 , 31, 3));
initialAlloc->Add (Vector (146 , 96, 3));
initialAlloc->Add (Vector (77 , 104, 3));
initialAlloc->Add (Vector (290 , 126, 3));
initialAlloc->Add (Vector (178 , 172, 3));
initialAlloc->Add (Vector (280 , 7, 3));
initialAlloc->Add (Vector (100 , 154, 3));
initialAlloc->Add (Vector (29 , 54, 3));
initialAlloc->Add (Vector (85 , 39, 3));
initialAlloc->Add (Vector (18 , 234, 3));
initialAlloc->Add (Vector (231 , 263, 3));
initialAlloc->Add (Vector (67 , 278, 3));
initialAlloc->Add (Vector (111 , 95, 3));
initialAlloc->Add (Vector (286 , 182, 3));
initialAlloc->Add (Vector (187 , 2, 3));
initialAlloc->Add (Vector (9 , 178, 3));
initialAlloc->Add (Vector (286 , 50, 3));
initialAlloc->Add (Vector (229 , 147, 3));
initialAlloc->Add (Vector (123 , 285, 3));
initialAlloc->Add (Vector (296 , 82, 3));
initialAlloc->Add (Vector (24 , 271, 3));
initialAlloc->Add (Vector (22 , 97, 3));
initialAlloc->Add (Vector (169 , 124, 3));
initialAlloc->Add (Vector (59 , 245, 3));
initialAlloc->Add (Vector (243 , 297, 3));
initialAlloc->Add (Vector (142 , 211, 3));
initialAlloc->Add (Vector (62 , 62, 3));
initialAlloc->Add (Vector (257 , 131, 3));
initialAlloc->Add (Vector (118 , 57, 3));
initialAlloc->Add (Vector (246 , 234, 3));
initialAlloc->Add (Vector (202 , 298, 3));
initialAlloc->Add (Vector (143 , 178, 3));
initialAlloc->Add (Vector (0 , 35, 3));
}
if(INST==9){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 35, 3));
initialAlloc->Add (Vector (115 , 74, 3));
initialAlloc->Add (Vector (265 , 7, 3));
initialAlloc->Add (Vector (230 , 53, 3));
initialAlloc->Add (Vector (20 , 178, 3));
initialAlloc->Add (Vector (242 , 152, 3));
initialAlloc->Add (Vector (58 , 289, 3));
initialAlloc->Add (Vector (37 , 215, 3));
initialAlloc->Add (Vector (209 , 240, 3));
initialAlloc->Add (Vector (45 , 80, 3));
initialAlloc->Add (Vector (99 , 194, 3));
initialAlloc->Add (Vector (264 , 263, 3));
initialAlloc->Add (Vector (183 , 57, 3));
initialAlloc->Add (Vector (110 , 295, 3));
initialAlloc->Add (Vector (96 , 104, 3));
initialAlloc->Add (Vector (147 , 53, 3));
initialAlloc->Add (Vector (255 , 94, 3));
initialAlloc->Add (Vector (165 , 282, 3));
initialAlloc->Add (Vector (145 , 257, 3));
initialAlloc->Add (Vector (66 , 121, 3));
initialAlloc->Add (Vector (298 , 177, 3));
initialAlloc->Add (Vector (162 , 145, 3));
initialAlloc->Add (Vector (298 , 211, 3));
initialAlloc->Add (Vector (57 , 167, 3));
initialAlloc->Add (Vector (176 , 212, 3));
initialAlloc->Add (Vector (91 , 258, 3));
initialAlloc->Add (Vector (207 , 289, 3));
initialAlloc->Add (Vector (265 , 59, 3));
initialAlloc->Add (Vector (292 , 138, 3));
initialAlloc->Add (Vector (14 , 242, 3));
initialAlloc->Add (Vector (1 , 293, 3));
initialAlloc->Add (Vector (164 , 102, 3));
initialAlloc->Add (Vector (2 , 141, 3));
initialAlloc->Add (Vector (144 , 222, 3));
initialAlloc->Add (Vector (211 , 180, 3));
initialAlloc->Add (Vector (256 , 230, 3));
initialAlloc->Add (Vector (85 , 48, 3));
initialAlloc->Add (Vector (141 , 176, 3));
initialAlloc->Add (Vector (27 , 117, 3));
initialAlloc->Add (Vector (178 , 177, 3));
initialAlloc->Add (Vector (297 , 70, 3));
initialAlloc->Add (Vector (131 , 134, 3));
initialAlloc->Add (Vector (242 , 289, 3));
initialAlloc->Add (Vector (12 , 77, 3));
initialAlloc->Add (Vector (45 , 253, 3));
initialAlloc->Add (Vector (180 , 25, 3));
initialAlloc->Add (Vector (95 , 138, 3));
initialAlloc->Add (Vector (35 , 32, 3));
initialAlloc->Add (Vector (293 , 27, 3));
initialAlloc->Add (Vector (212 , 24, 3));
initialAlloc->Add (Vector (259 , 197, 3));
initialAlloc->Add (Vector (205 , 126, 3));
initialAlloc->Add (Vector (204 , 91, 3));
initialAlloc->Add (Vector (76 , 219, 3));
initialAlloc->Add (Vector (111 , 227, 3));
}
if(INST==10){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 0, 3));//initialAlloc->Add (Vector (50 , 50, 3)); 
initialAlloc->Add (Vector (50 , 0, 3));//initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (111 , 227, 3));
initialAlloc->Add (Vector (295 , 208, 3));
initialAlloc->Add (Vector (278 , 125, 3));
initialAlloc->Add (Vector (18 , 77, 3));
initialAlloc->Add (Vector (275 , 271, 3));
initialAlloc->Add (Vector (47 , 107, 3));
initialAlloc->Add (Vector (23 , 173, 3));
initialAlloc->Add (Vector (228 , 273, 3));
initialAlloc->Add (Vector (71 , 168, 3));
initialAlloc->Add (Vector (168 , 244, 3));
initialAlloc->Add (Vector (88 , 66, 3));
initialAlloc->Add (Vector (260 , 45, 3));
initialAlloc->Add (Vector (69 , 212, 3));
initialAlloc->Add (Vector (117 , 181, 3));
initialAlloc->Add (Vector (256 , 238, 3));
initialAlloc->Add (Vector (120 , 27, 3));
initialAlloc->Add (Vector (158 , 288, 3));
initialAlloc->Add (Vector (23 , 265, 3));
initialAlloc->Add (Vector (221 , 37, 3));
initialAlloc->Add (Vector (260 , 182, 3));
initialAlloc->Add (Vector (17 , 127, 3));
initialAlloc->Add (Vector (137 , 72, 3));
initialAlloc->Add (Vector (254 , 92, 3));
initialAlloc->Add (Vector (245 , 126, 3));
initialAlloc->Add (Vector (171 , 103, 3));
initialAlloc->Add (Vector (121 , 297, 3));
initialAlloc->Add (Vector (195 , 76, 3));
initialAlloc->Add (Vector (273 , 11, 3));
initialAlloc->Add (Vector (111 , 108, 3));
initialAlloc->Add (Vector (71 , 290, 3));
initialAlloc->Add (Vector (291 , 171, 3));
initialAlloc->Add (Vector (219 , 200, 3));
initialAlloc->Add (Vector (191 , 216, 3));
initialAlloc->Add (Vector (151 , 150, 3));
initialAlloc->Add (Vector (48 , 237, 3));
initialAlloc->Add (Vector (165 , 44, 3));
initialAlloc->Add (Vector (194 , 9, 3));
initialAlloc->Add (Vector (0 , 37, 3));
initialAlloc->Add (Vector (187 , 145, 3));
initialAlloc->Add (Vector (152 , 203, 3));
initialAlloc->Add (Vector (79 , 137, 3));
initialAlloc->Add (Vector (299 , 65, 3));
initialAlloc->Add (Vector (199 , 292, 3));
initialAlloc->Add (Vector (3 , 238, 3));
initialAlloc->Add (Vector (16 , 297, 3));
initialAlloc->Add (Vector (59 , 33, 3));
initialAlloc->Add (Vector (116 , 147, 3));
initialAlloc->Add (Vector (106 , 261, 3));
initialAlloc->Add (Vector (220 , 103, 3));
initialAlloc->Add (Vector (186 , 181, 3));
initialAlloc->Add (Vector (204 , 246, 3));
initialAlloc->Add (Vector (223 , 160, 3));
initialAlloc->Add (Vector (10 , 205, 3));
initialAlloc->Add (Vector (252 , 298, 3));
initialAlloc->Add (Vector (299 , 246, 3));
}
if(INST==11){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (299 , 246, 3));
initialAlloc->Add (Vector (23 , 194, 3));
initialAlloc->Add (Vector (3 , 295, 3));
initialAlloc->Add (Vector (181 , 181, 3));
initialAlloc->Add (Vector (89 , 223, 3));
initialAlloc->Add (Vector (200 , 216, 3));
initialAlloc->Add (Vector (218 , 134, 3));
initialAlloc->Add (Vector (181 , 284, 3));
initialAlloc->Add (Vector (155 , 35, 3));
initialAlloc->Add (Vector (112 , 98, 3));
initialAlloc->Add (Vector (151 , 73, 3));
initialAlloc->Add (Vector (26 , 226, 3));
initialAlloc->Add (Vector (19 , 23, 3));
initialAlloc->Add (Vector (18 , 105, 3));
initialAlloc->Add (Vector (256 , 160, 3));
initialAlloc->Add (Vector (28 , 157, 3));
initialAlloc->Add (Vector (76 , 298, 3));
initialAlloc->Add (Vector (168 , 225, 3));
initialAlloc->Add (Vector (273 , 83, 3));
initialAlloc->Add (Vector (218 , 294, 3));
initialAlloc->Add (Vector (92 , 150, 3));
initialAlloc->Add (Vector (54 , 264, 3));
initialAlloc->Add (Vector (124 , 186, 3));
initialAlloc->Add (Vector (136 , 266, 3));
initialAlloc->Add (Vector (73 , 94, 3));
initialAlloc->Add (Vector (35 , 73, 3));
initialAlloc->Add (Vector (223 , 68, 3));
initialAlloc->Add (Vector (265 , 241, 3));
initialAlloc->Add (Vector (232 , 219, 3));
initialAlloc->Add (Vector (237 , 262, 3));
initialAlloc->Add (Vector (298 , 182, 3));
initialAlloc->Add (Vector (77 , 23, 3));
initialAlloc->Add (Vector (124 , 235, 3));
initialAlloc->Add (Vector (223 , 183, 3));
initialAlloc->Add (Vector (297 , 135, 3));
initialAlloc->Add (Vector (255 , 118, 3));
initialAlloc->Add (Vector (279 , 272, 3));
initialAlloc->Add (Vector (189 , 36, 3));
initialAlloc->Add (Vector (144 , 146, 3));
initialAlloc->Add (Vector (253 , 3, 3));
initialAlloc->Add (Vector (83 , 63, 3));
initialAlloc->Add (Vector (115 , 47, 3));
initialAlloc->Add (Vector (187 , 113, 3));
initialAlloc->Add (Vector (36 , 296, 3));
initialAlloc->Add (Vector (232 , 28, 3));
initialAlloc->Add (Vector (149 , 106, 3));
initialAlloc->Add (Vector (296 , 50, 3));
initialAlloc->Add (Vector (54 , 184, 3));
initialAlloc->Add (Vector (295 , 7, 3));
initialAlloc->Add (Vector (61 , 138, 3));
initialAlloc->Add (Vector (177 , 145, 3));
initialAlloc->Add (Vector (101 , 275, 3));
initialAlloc->Add (Vector (47 , 39, 3));
initialAlloc->Add (Vector (271 , 204, 3));
initialAlloc->Add (Vector (7 , 53, 3));
}
if(INST==12){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (7 , 53, 3));
initialAlloc->Add (Vector (260 , 14, 3));
initialAlloc->Add (Vector (194 , 208, 3));
initialAlloc->Add (Vector (223 , 255, 3));
initialAlloc->Add (Vector (29 , 273, 3));
initialAlloc->Add (Vector (97 , 264, 3));
initialAlloc->Add (Vector (178 , 69, 3));
initialAlloc->Add (Vector (107 , 67, 3));
initialAlloc->Add (Vector (287 , 160, 3));
initialAlloc->Add (Vector (262 , 248, 3));
initialAlloc->Add (Vector (138 , 112, 3));
initialAlloc->Add (Vector (30 , 137, 3));
initialAlloc->Add (Vector (250 , 127, 3));
initialAlloc->Add (Vector (296 , 30, 3));
initialAlloc->Add (Vector (229 , 153, 3));
initialAlloc->Add (Vector (104 , 197, 3));
initialAlloc->Add (Vector (264 , 197, 3));
initialAlloc->Add (Vector (176 , 278, 3));
initialAlloc->Add (Vector (54 , 198, 3));
initialAlloc->Add (Vector (139 , 271, 3));
initialAlloc->Add (Vector (270 , 68, 3));
initialAlloc->Add (Vector (176 , 126, 3));
initialAlloc->Add (Vector (22 , 206, 3));
initialAlloc->Add (Vector (74 , 125, 3));
initialAlloc->Add (Vector (141 , 80, 3));
initialAlloc->Add (Vector (41 , 53, 3));
initialAlloc->Add (Vector (109 , 297, 3));
initialAlloc->Add (Vector (240 , 295, 3));
initialAlloc->Add (Vector (75 , 46, 3));
initialAlloc->Add (Vector (141 , 201, 3));
initialAlloc->Add (Vector (82 , 227, 3));
initialAlloc->Add (Vector (42 , 237, 3));
initialAlloc->Add (Vector (233 , 92, 3));
initialAlloc->Add (Vector (173 , 168, 3));
initialAlloc->Add (Vector (214 , 44, 3));
initialAlloc->Add (Vector (294 , 222, 3));
initialAlloc->Add (Vector (291 , 262, 3));
initialAlloc->Add (Vector (2 , 98, 3));
initialAlloc->Add (Vector (26 , 172, 3));
initialAlloc->Add (Vector (131 , 148, 3));
initialAlloc->Add (Vector (211 , 118, 3));
initialAlloc->Add (Vector (119 , 240, 3));
initialAlloc->Add (Vector (73 , 81, 3));
initialAlloc->Add (Vector (36 , 101, 3));
initialAlloc->Add (Vector (120 , 29, 3));
initialAlloc->Add (Vector (298 , 90, 3));
initialAlloc->Add (Vector (70 , 288, 3));
initialAlloc->Add (Vector (175 , 238, 3));
initialAlloc->Add (Vector (226 , 218, 3));
initialAlloc->Add (Vector (74 , 169, 3));
initialAlloc->Add (Vector (105 , 111, 3));
initialAlloc->Add (Vector (287 , 127, 3));
initialAlloc->Add (Vector (167 , 34, 3));
initialAlloc->Add (Vector (218 , 186, 3));
initialAlloc->Add (Vector (19 , 11, 3));
}
if(INST==13){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (19 , 11, 3));
initialAlloc->Add (Vector (90 , 281, 3));
initialAlloc->Add (Vector (279 , 91, 3));
initialAlloc->Add (Vector (64 , 124, 3));
initialAlloc->Add (Vector (32 , 275, 3));
initialAlloc->Add (Vector (226 , 79, 3));
initialAlloc->Add (Vector (262 , 294, 3));
initialAlloc->Add (Vector (237 , 148, 3));
initialAlloc->Add (Vector (8 , 114, 3));
initialAlloc->Add (Vector (109 , 146, 3));
initialAlloc->Add (Vector (93 , 64, 3));
initialAlloc->Add (Vector (143 , 169, 3));
initialAlloc->Add (Vector (112 , 221, 3));
initialAlloc->Add (Vector (180 , 74, 3));
initialAlloc->Add (Vector (229 , 204, 3));
initialAlloc->Add (Vector (32 , 238, 3));
initialAlloc->Add (Vector (192 , 104, 3));
initialAlloc->Add (Vector (273 , 198, 3));
initialAlloc->Add (Vector (73 , 176, 3));
initialAlloc->Add (Vector (41 , 150, 3));
initialAlloc->Add (Vector (110 , 93, 3));
initialAlloc->Add (Vector (24 , 183, 3));
initialAlloc->Add (Vector (228 , 117, 3));
initialAlloc->Add (Vector (247 , 235, 3));
initialAlloc->Add (Vector (236 , 266, 3));
initialAlloc->Add (Vector (188 , 37, 3));
initialAlloc->Add (Vector (183 , 194, 3));
initialAlloc->Add (Vector (131 , 39, 3));
initialAlloc->Add (Vector (9 , 68, 3));
initialAlloc->Add (Vector (158 , 262, 3));
initialAlloc->Add (Vector (197 , 258, 3));
initialAlloc->Add (Vector (161 , 115, 3));
initialAlloc->Add (Vector (295 , 131, 3));
initialAlloc->Add (Vector (72 , 216, 3));
initialAlloc->Add (Vector (39 , 94, 3));
initialAlloc->Add (Vector (148 , 204, 3));
initialAlloc->Add (Vector (47 , 57, 3));
initialAlloc->Add (Vector (200 , 294, 3));
initialAlloc->Add (Vector (262 , 51, 3));
initialAlloc->Add (Vector (274 , 17, 3));
initialAlloc->Add (Vector (291 , 171, 3));
initialAlloc->Add (Vector (124 , 292, 3));
initialAlloc->Add (Vector (284 , 234, 3));
initialAlloc->Add (Vector (0 , 287, 3));
initialAlloc->Add (Vector (72 , 36, 3));
initialAlloc->Add (Vector (227 , 21, 3));
initialAlloc->Add (Vector (260 , 119, 3));
initialAlloc->Add (Vector (176 , 227, 3));
initialAlloc->Add (Vector (298 , 267, 3));
initialAlloc->Add (Vector (195 , 136, 3));
initialAlloc->Add (Vector (294 , 47, 3));
initialAlloc->Add (Vector (71 , 89, 3));
initialAlloc->Add (Vector (157 , 294, 3));
initialAlloc->Add (Vector (122 , 254, 3));
initialAlloc->Add (Vector (67 , 253, 3));
}
if(INST==14){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (67 , 253, 3));
initialAlloc->Add (Vector (283 , 152, 3));
initialAlloc->Add (Vector (51 , 222, 3));
initialAlloc->Add (Vector (73 , 188, 3));
initialAlloc->Add (Vector (219 , 106, 3));
initialAlloc->Add (Vector (78 , 64, 3));
initialAlloc->Add (Vector (240 , 150, 3));
initialAlloc->Add (Vector (296 , 185, 3));
initialAlloc->Add (Vector (205 , 41, 3));
initialAlloc->Add (Vector (159 , 198, 3));
initialAlloc->Add (Vector (160 , 262, 3));
initialAlloc->Add (Vector (263 , 123, 3));
initialAlloc->Add (Vector (252 , 298, 3));
initialAlloc->Add (Vector (243 , 79, 3));
initialAlloc->Add (Vector (101 , 215, 3));
initialAlloc->Add (Vector (186 , 179, 3));
initialAlloc->Add (Vector (30 , 27, 3));
initialAlloc->Add (Vector (285 , 268, 3));
initialAlloc->Add (Vector (119 , 117, 3));
initialAlloc->Add (Vector (190 , 278, 3));
initialAlloc->Add (Vector (173 , 31, 3));
initialAlloc->Add (Vector (172 , 117, 3));
initialAlloc->Add (Vector (110 , 273, 3));
initialAlloc->Add (Vector (246 , 4, 3));
initialAlloc->Add (Vector (19 , 172, 3));
initialAlloc->Add (Vector (108 , 167, 3));
initialAlloc->Add (Vector (19 , 97, 3));
initialAlloc->Add (Vector (241 , 198, 3));
initialAlloc->Add (Vector (63 , 152, 3));
initialAlloc->Add (Vector (188 , 145, 3));
initialAlloc->Add (Vector (22 , 236, 3));
initialAlloc->Add (Vector (192 , 83, 3));
initialAlloc->Add (Vector (243 , 264, 3));
initialAlloc->Add (Vector (293 , 220, 3));
initialAlloc->Add (Vector (275 , 37, 3));
initialAlloc->Add (Vector (277 , 93, 3));
initialAlloc->Add (Vector (139 , 156, 3));
initialAlloc->Add (Vector (78 , 118, 3));
initialAlloc->Add (Vector (149 , 83, 3));
initialAlloc->Add (Vector (120 , 69, 3));
initialAlloc->Add (Vector (44 , 276, 3));
initialAlloc->Add (Vector (4 , 275, 3));
initialAlloc->Add (Vector (86 , 32, 3));
initialAlloc->Add (Vector (1 , 57, 3));
initialAlloc->Add (Vector (126 , 33, 3));
initialAlloc->Add (Vector (11 , 138, 3));
initialAlloc->Add (Vector (299 , 8, 3));
initialAlloc->Add (Vector (205 , 238, 3));
initialAlloc->Add (Vector (40 , 67, 3));
initialAlloc->Add (Vector (78 , 287, 3));
initialAlloc->Add (Vector (3 , 6, 3));
initialAlloc->Add (Vector (220 , 292, 3));
initialAlloc->Add (Vector (143 , 296, 3));
initialAlloc->Add (Vector (150 , 231, 3));
initialAlloc->Add (Vector (262 , 233, 3));
}
if(INST==15){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (262 , 233, 3));
initialAlloc->Add (Vector (293 , 150, 3));
initialAlloc->Add (Vector (274 , 192, 3));
initialAlloc->Add (Vector (103 , 292, 3));
initialAlloc->Add (Vector (44 , 164, 3));
initialAlloc->Add (Vector (190 , 225, 3));
initialAlloc->Add (Vector (224 , 253, 3));
initialAlloc->Add (Vector (25 , 80, 3));
initialAlloc->Add (Vector (93 , 261, 3));
initialAlloc->Add (Vector (160 , 61, 3));
initialAlloc->Add (Vector (268 , 81, 3));
initialAlloc->Add (Vector (160 , 168, 3));
initialAlloc->Add (Vector (65 , 205, 3));
initialAlloc->Add (Vector (70 , 114, 3));
initialAlloc->Add (Vector (280 , 35, 3));
initialAlloc->Add (Vector (11 , 23, 3));
initialAlloc->Add (Vector (115 , 114, 3));
initialAlloc->Add (Vector (30 , 258, 3));
initialAlloc->Add (Vector (17 , 113, 3));
initialAlloc->Add (Vector (245 , 3, 3));
initialAlloc->Add (Vector (225 , 164, 3));
initialAlloc->Add (Vector (63 , 293, 3));
initialAlloc->Add (Vector (3 , 212, 3));
initialAlloc->Add (Vector (127 , 250, 3));
initialAlloc->Add (Vector (141 , 212, 3));
initialAlloc->Add (Vector (112 , 152, 3));
initialAlloc->Add (Vector (188 , 193, 3));
initialAlloc->Add (Vector (229 , 42, 3));
initialAlloc->Add (Vector (130 , 85, 3));
initialAlloc->Add (Vector (220 , 293, 3));
initialAlloc->Add (Vector (258 , 277, 3));
initialAlloc->Add (Vector (179 , 35, 3));
initialAlloc->Add (Vector (137 , 291, 3));
initialAlloc->Add (Vector (212 , 85, 3));
initialAlloc->Add (Vector (75 , 75, 3));
initialAlloc->Add (Vector (18 , 296, 3));
initialAlloc->Add (Vector (189 , 111, 3));
initialAlloc->Add (Vector (260 , 129, 3));
initialAlloc->Add (Vector (177 , 294, 3));
initialAlloc->Add (Vector (292 , 246, 3));
initialAlloc->Add (Vector (74 , 31, 3));
initialAlloc->Add (Vector (111 , 200, 3));
initialAlloc->Add (Vector (39 , 48, 3));
initialAlloc->Add (Vector (126 , 24, 3));
initialAlloc->Add (Vector (222 , 216, 3));
initialAlloc->Add (Vector (192 , 146, 3));
initialAlloc->Add (Vector (93 , 227, 3));
initialAlloc->Add (Vector (156 , 132, 3));
initialAlloc->Add (Vector (78 , 173, 3));
initialAlloc->Add (Vector (192 , 263, 3));
initialAlloc->Add (Vector (36 , 219, 3));
initialAlloc->Add (Vector (292 , 108, 3));
initialAlloc->Add (Vector (226 , 123, 3));
initialAlloc->Add (Vector (12 , 152, 3));
initialAlloc->Add (Vector (63 , 244, 3));
}
if(INST==16){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (63 , 244, 3));
initialAlloc->Add (Vector (144 , 213, 3));
initialAlloc->Add (Vector (117 , 172, 3));
initialAlloc->Add (Vector (99 , 23, 3));
initialAlloc->Add (Vector (127 , 263, 3));
initialAlloc->Add (Vector (183 , 288, 3));
initialAlloc->Add (Vector (69 , 125, 3));
initialAlloc->Add (Vector (119 , 111, 3));
initialAlloc->Add (Vector (288 , 62, 3));
initialAlloc->Add (Vector (189 , 4, 3));
initialAlloc->Add (Vector (148 , 52, 3));
initialAlloc->Add (Vector (251 , 240, 3));
initialAlloc->Add (Vector (268 , 148, 3));
initialAlloc->Add (Vector (205 , 137, 3));
initialAlloc->Add (Vector (161 , 148, 3));
initialAlloc->Add (Vector (188 , 88, 3));
initialAlloc->Add (Vector (266 , 22, 3));
initialAlloc->Add (Vector (192 , 47, 3));
initialAlloc->Add (Vector (39 , 147, 3));
initialAlloc->Add (Vector (285 , 116, 3));
initialAlloc->Add (Vector (1 , 146, 3));
initialAlloc->Add (Vector (212 , 173, 3));
initialAlloc->Add (Vector (77 , 65, 3));
initialAlloc->Add (Vector (223 , 256, 3));
initialAlloc->Add (Vector (197 , 215, 3));
initialAlloc->Add (Vector (57 , 32, 3));
initialAlloc->Add (Vector (178 , 119, 3));
initialAlloc->Add (Vector (220 , 73, 3));
initialAlloc->Add (Vector (255 , 94, 3));
initialAlloc->Add (Vector (75 , 173, 3));
initialAlloc->Add (Vector (53 , 278, 3));
initialAlloc->Add (Vector (255 , 294, 3));
initialAlloc->Add (Vector (286 , 181, 3));
initialAlloc->Add (Vector (104 , 293, 3));
initialAlloc->Add (Vector (286 , 264, 3));
initialAlloc->Add (Vector (297 , 8, 3));
initialAlloc->Add (Vector (24 , 250, 3));
initialAlloc->Add (Vector (7 , 296, 3));
initialAlloc->Add (Vector (151 , 287, 3));
initialAlloc->Add (Vector (220 , 28, 3));
initialAlloc->Add (Vector (0 , 203, 3));
initialAlloc->Add (Vector (20 , 173, 3));
initialAlloc->Add (Vector (145 , 92, 3));
initialAlloc->Add (Vector (100 , 210, 3));
initialAlloc->Add (Vector (32 , 89, 3));
initialAlloc->Add (Vector (161 , 22, 3));
initialAlloc->Add (Vector (166 , 186, 3));
initialAlloc->Add (Vector (176 , 255, 3));
initialAlloc->Add (Vector (287 , 223, 3));
initialAlloc->Add (Vector (239 , 194, 3));
initialAlloc->Add (Vector (109 , 68, 3));
initialAlloc->Add (Vector (67 , 212, 3));
initialAlloc->Add (Vector (104 , 140, 3));
initialAlloc->Add (Vector (251 , 58, 3));
initialAlloc->Add (Vector (36 , 202, 3));
}
if(INST==17){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (36 , 202, 3));
initialAlloc->Add (Vector (265 , 157, 3));
initialAlloc->Add (Vector (239 , 47, 3));
initialAlloc->Add (Vector (79 , 59, 3));
initialAlloc->Add (Vector (145 , 111, 3));
initialAlloc->Add (Vector (1 , 165, 3));
initialAlloc->Add (Vector (179 , 276, 3));
initialAlloc->Add (Vector (108 , 177, 3));
initialAlloc->Add (Vector (282 , 123, 3));
initialAlloc->Add (Vector (206 , 161, 3));
initialAlloc->Add (Vector (153 , 248, 3));
initialAlloc->Add (Vector (283 , 233, 3));
initialAlloc->Add (Vector (208 , 250, 3));
initialAlloc->Add (Vector (96 , 217, 3));
initialAlloc->Add (Vector (136 , 36, 3));
initialAlloc->Add (Vector (16 , 267, 3));
initialAlloc->Add (Vector (107 , 100, 3));
initialAlloc->Add (Vector (50 , 267, 3));
initialAlloc->Add (Vector (277 , 84, 3));
initialAlloc->Add (Vector (245 , 295, 3));
initialAlloc->Add (Vector (97 , 258, 3));
initialAlloc->Add (Vector (47 , 125, 3));
initialAlloc->Add (Vector (157 , 80, 3));
initialAlloc->Add (Vector (127 , 140, 3));
initialAlloc->Add (Vector (261 , 1, 3));
initialAlloc->Add (Vector (219 , 110, 3));
initialAlloc->Add (Vector (293 , 264, 3));
initialAlloc->Add (Vector (243 , 190, 3));
initialAlloc->Add (Vector (147 , 290, 3));
initialAlloc->Add (Vector (67 , 298, 3));
initialAlloc->Add (Vector (79 , 152, 3));
initialAlloc->Add (Vector (64 , 92, 3));
initialAlloc->Add (Vector (155 , 170, 3));
initialAlloc->Add (Vector (152 , 206, 3));
initialAlloc->Add (Vector (248 , 247, 3));
initialAlloc->Add (Vector (3 , 201, 3));
initialAlloc->Add (Vector (295 , 48, 3));
initialAlloc->Add (Vector (198 , 202, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (180 , 104, 3));
initialAlloc->Add (Vector (36 , 76, 3));
initialAlloc->Add (Vector (219 , 20, 3));
initialAlloc->Add (Vector (206 , 294, 3));
initialAlloc->Add (Vector (35 , 237, 3));
initialAlloc->Add (Vector (188 , 71, 3));
initialAlloc->Add (Vector (68 , 201, 3));
initialAlloc->Add (Vector (113 , 299, 3));
initialAlloc->Add (Vector (234 , 81, 3));
initialAlloc->Add (Vector (295 , 13, 3));
initialAlloc->Add (Vector (189 , 33, 3));
initialAlloc->Add (Vector (79 , 16, 3));
initialAlloc->Add (Vector (52 , 38, 3));
initialAlloc->Add (Vector (249 , 125, 3));
initialAlloc->Add (Vector (20 , 299, 3));
initialAlloc->Add (Vector (291 , 199, 3));
}
if(INST==18){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (291 , 199, 3));
initialAlloc->Add (Vector (55 , 247, 3));
initialAlloc->Add (Vector (76 , 198, 3));
initialAlloc->Add (Vector (87 , 52, 3));
initialAlloc->Add (Vector (122 , 98, 3));
initialAlloc->Add (Vector (22 , 263, 3));
initialAlloc->Add (Vector (70 , 150, 3));
initialAlloc->Add (Vector (101 , 283, 3));
initialAlloc->Add (Vector (131 , 259, 3));
initialAlloc->Add (Vector (48 , 91, 3));
initialAlloc->Add (Vector (139 , 5, 3));
initialAlloc->Add (Vector (211 , 112, 3));
initialAlloc->Add (Vector (249 , 189, 3));
initialAlloc->Add (Vector (86 , 109, 3));
initialAlloc->Add (Vector (168 , 155, 3));
initialAlloc->Add (Vector (213 , 181, 3));
initialAlloc->Add (Vector (111 , 193, 3));
initialAlloc->Add (Vector (178 , 74, 3));
initialAlloc->Add (Vector (29 , 169, 3));
initialAlloc->Add (Vector (274 , 242, 3));
initialAlloc->Add (Vector (187 , 218, 3));
initialAlloc->Add (Vector (139 , 65, 3));
initialAlloc->Add (Vector (284 , 50, 3));
initialAlloc->Add (Vector (227 , 270, 3));
initialAlloc->Add (Vector (3 , 190, 3));
initialAlloc->Add (Vector (114 , 227, 3));
initialAlloc->Add (Vector (207 , 33, 3));
initialAlloc->Add (Vector (153 , 229, 3));
initialAlloc->Add (Vector (248 , 95, 3));
initialAlloc->Add (Vector (180 , 281, 3));
initialAlloc->Add (Vector (43 , 125, 3));
initialAlloc->Add (Vector (222 , 75, 3));
initialAlloc->Add (Vector (142 , 128, 3));
initialAlloc->Add (Vector (298 , 119, 3));
initialAlloc->Add (Vector (234 , 146, 3));
initialAlloc->Add (Vector (273 , 277, 3));
initialAlloc->Add (Vector (282 , 12, 3));
initialAlloc->Add (Vector (273 , 147, 3));
initialAlloc->Add (Vector (10 , 135, 3));
initialAlloc->Add (Vector (116 , 147, 3));
initialAlloc->Add (Vector (60 , 283, 3));
initialAlloc->Add (Vector (243 , 19, 3));
initialAlloc->Add (Vector (8 , 292, 3));
initialAlloc->Add (Vector (96 , 20, 3));
initialAlloc->Add (Vector (162 , 39, 3));
initialAlloc->Add (Vector (9 , 222, 3));
initialAlloc->Add (Vector (47 , 216, 3));
initialAlloc->Add (Vector (43 , 38, 3));
initialAlloc->Add (Vector (208 , 299, 3));
initialAlloc->Add (Vector (174 , 123, 3));
initialAlloc->Add (Vector (145 , 197, 3));
initialAlloc->Add (Vector (235 , 237, 3));
initialAlloc->Add (Vector (287 , 86, 3));
initialAlloc->Add (Vector (197 , 2, 3));
initialAlloc->Add (Vector (134 , 298, 3));
}
if(INST==19){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (134 , 298, 3));
initialAlloc->Add (Vector (149 , 176, 3));
initialAlloc->Add (Vector (187 , 15, 3));
initialAlloc->Add (Vector (221 , 45, 3));
initialAlloc->Add (Vector (152 , 64, 3));
initialAlloc->Add (Vector (265 , 126, 3));
initialAlloc->Add (Vector (109 , 225, 3));
initialAlloc->Add (Vector (226 , 199, 3));
initialAlloc->Add (Vector (103 , 88, 3));
initialAlloc->Add (Vector (47 , 56, 3));
initialAlloc->Add (Vector (155 , 222, 3));
initialAlloc->Add (Vector (238 , 91, 3));
initialAlloc->Add (Vector (38 , 182, 3));
initialAlloc->Add (Vector (5 , 186, 3));
initialAlloc->Add (Vector (114 , 191, 3));
initialAlloc->Add (Vector (206 , 278, 3));
initialAlloc->Add (Vector (47 , 111, 3));
initialAlloc->Add (Vector (254 , 253, 3));
initialAlloc->Add (Vector (37 , 232, 3));
initialAlloc->Add (Vector (204 , 140, 3));
initialAlloc->Add (Vector (71 , 263, 3));
initialAlloc->Add (Vector (71 , 170, 3));
initialAlloc->Add (Vector (156 , 258, 3));
initialAlloc->Add (Vector (238 , 1, 3));
initialAlloc->Add (Vector (287 , 81, 3));
initialAlloc->Add (Vector (231 , 158, 3));
initialAlloc->Add (Vector (283 , 2, 3));
initialAlloc->Add (Vector (159 , 117, 3));
initialAlloc->Add (Vector (190 , 85, 3));
initialAlloc->Add (Vector (291 , 191, 3));
initialAlloc->Add (Vector (129 , 133, 3));
initialAlloc->Add (Vector (124 , 262, 3));
initialAlloc->Add (Vector (146 , 18, 3));
initialAlloc->Add (Vector (80 , 39, 3));
initialAlloc->Add (Vector (98 , 8, 3));
initialAlloc->Add (Vector (6 , 261, 3));
initialAlloc->Add (Vector (254 , 43, 3));
initialAlloc->Add (Vector (93 , 290, 3));
initialAlloc->Add (Vector (168 , 292, 3));
initialAlloc->Add (Vector (286 , 47, 3));
initialAlloc->Add (Vector (247 , 294, 3));
initialAlloc->Add (Vector (5 , 136, 3));
initialAlloc->Add (Vector (193 , 181, 3));
initialAlloc->Add (Vector (24 , 291, 3));
initialAlloc->Add (Vector (96 , 142, 3));
initialAlloc->Add (Vector (298 , 138, 3));
initialAlloc->Add (Vector (187 , 243, 3));
initialAlloc->Add (Vector (72 , 215, 3));
initialAlloc->Add (Vector (271 , 221, 3));
initialAlloc->Add (Vector (271 , 163, 3));
initialAlloc->Add (Vector (46 , 150, 3));
initialAlloc->Add (Vector (28 , 82, 3));
initialAlloc->Add (Vector (4 , 223, 3));
initialAlloc->Add (Vector (288 , 270, 3));
initialAlloc->Add (Vector (219 , 232, 3));
}
if(INST==20){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 50, 3));
initialAlloc->Add (Vector (50 , 0, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (219 , 232, 3));
initialAlloc->Add (Vector (197 , 159, 3));
initialAlloc->Add (Vector (296 , 181, 3));
initialAlloc->Add (Vector (262 , 41, 3));
initialAlloc->Add (Vector (40 , 181, 3));
initialAlloc->Add (Vector (50 , 124, 3));
initialAlloc->Add (Vector (265 , 107, 3));
initialAlloc->Add (Vector (179 , 59, 3));
initialAlloc->Add (Vector (41 , 251, 3));
initialAlloc->Add (Vector (160 , 91, 3));
initialAlloc->Add (Vector (125 , 63, 3));
initialAlloc->Add (Vector (104 , 228, 3));
initialAlloc->Add (Vector (73 , 154, 3));
initialAlloc->Add (Vector (129 , 181, 3));
initialAlloc->Add (Vector (55 , 66, 3));
initialAlloc->Add (Vector (258 , 215, 3));
initialAlloc->Add (Vector (158 , 13, 3));
initialAlloc->Add (Vector (89 , 122, 3));
initialAlloc->Add (Vector (295 , 28, 3));
initialAlloc->Add (Vector (185 , 201, 3));
initialAlloc->Add (Vector (108 , 1, 3));
initialAlloc->Add (Vector (90 , 197, 3));
initialAlloc->Add (Vector (295 , 141, 3));
initialAlloc->Add (Vector (26 , 218, 3));
initialAlloc->Add (Vector (130 , 147, 3));
initialAlloc->Add (Vector (223 , 50, 3));
initialAlloc->Add (Vector (89 , 33, 3));
initialAlloc->Add (Vector (224 , 138, 3));
initialAlloc->Add (Vector (237 , 280, 3));
initialAlloc->Add (Vector (199 , 109, 3));
initialAlloc->Add (Vector (103 , 92, 3));
initialAlloc->Add (Vector (159 , 275, 3));
initialAlloc->Add (Vector (10 , 168, 3));
initialAlloc->Add (Vector (173 , 235, 3));
initialAlloc->Add (Vector (164 , 168, 3));
initialAlloc->Add (Vector (74 , 245, 3));
initialAlloc->Add (Vector (230 , 174, 3));
initialAlloc->Add (Vector (116 , 263, 3));
initialAlloc->Add (Vector (254 , 152, 3));
initialAlloc->Add (Vector (279 , 76, 3));
initialAlloc->Add (Vector (267 , 2, 3));
initialAlloc->Add (Vector (6 , 274, 3));
initialAlloc->Add (Vector (228 , 92, 3));
initialAlloc->Add (Vector (28 , 26, 3));
initialAlloc->Add (Vector (259 , 250, 3));
initialAlloc->Add (Vector (296 , 214, 3));
initialAlloc->Add (Vector (39 , 291, 3));
initialAlloc->Add (Vector (145 , 218, 3));
initialAlloc->Add (Vector (233 , 11, 3));
initialAlloc->Add (Vector (200 , 292, 3));
initialAlloc->Add (Vector (131 , 297, 3));
initialAlloc->Add (Vector (165 , 132, 3));
initialAlloc->Add (Vector (297 , 257, 3));
initialAlloc->Add (Vector (194 , 6, 3));
initialAlloc->Add (Vector (130 , 111, 3));
}
if(INST==21){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (50 , 50, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (130 , 111, 3));
initialAlloc->Add (Vector (56 , 119, 3));
initialAlloc->Add (Vector (290 , 22, 3));
initialAlloc->Add (Vector (60 , 291, 3));
initialAlloc->Add (Vector (63 , 166, 3));
initialAlloc->Add (Vector (111 , 155, 3));
initialAlloc->Add (Vector (212 , 69, 3));
initialAlloc->Add (Vector (161 , 41, 3));
initialAlloc->Add (Vector (152 , 299, 3));
initialAlloc->Add (Vector (118 , 294, 3));
initialAlloc->Add (Vector (233 , 272, 3));
initialAlloc->Add (Vector (290 , 74, 3));
initialAlloc->Add (Vector (13 , 215, 3));
initialAlloc->Add (Vector (289 , 110, 3));
initialAlloc->Add (Vector (276 , 210, 3));
initialAlloc->Add (Vector (208 , 241, 3));
initialAlloc->Add (Vector (230 , 135, 3));
initialAlloc->Add (Vector (242 , 42, 3));
initialAlloc->Add (Vector (116 , 227, 3));
initialAlloc->Add (Vector (89 , 67, 3));
initialAlloc->Add (Vector (182 , 278, 3));
initialAlloc->Add (Vector (184 , 142, 3));
initialAlloc->Add (Vector (211 , 201, 3));
initialAlloc->Add (Vector (12 , 5, 3));
initialAlloc->Add (Vector (162 , 114, 3));
initialAlloc->Add (Vector (140 , 169, 3));
initialAlloc->Add (Vector (2 , 284, 3));
initialAlloc->Add (Vector (278 , 168, 3));
initialAlloc->Add (Vector (136 , 4, 3));
initialAlloc->Add (Vector (259 , 241, 3));
initialAlloc->Add (Vector (138 , 265, 3));
initialAlloc->Add (Vector (131 , 61, 3));
initialAlloc->Add (Vector (8 , 183, 3));
initialAlloc->Add (Vector (187 , 21, 3));
initialAlloc->Add (Vector (16 , 48, 3));
initialAlloc->Add (Vector (164 , 201, 3));
initialAlloc->Add (Vector (50 , 10, 3));
initialAlloc->Add (Vector (86 , 265, 3));
initialAlloc->Add (Vector (57 , 235, 3));
initialAlloc->Add (Vector (215 , 106, 3));
initialAlloc->Add (Vector (111 , 36, 3));
initialAlloc->Add (Vector (254 , 104, 3));
initialAlloc->Add (Vector (288 , 262, 3));
initialAlloc->Add (Vector (224 , 3, 3));
initialAlloc->Add (Vector (115 , 190, 3));
initialAlloc->Add (Vector (24 , 255, 3));
initialAlloc->Add (Vector (81 , 210, 3));
initialAlloc->Add (Vector (53 , 82, 3));
initialAlloc->Add (Vector (262 , 297, 3));
initialAlloc->Add (Vector (162 , 237, 3));
initialAlloc->Add (Vector (92 , 108, 3));
initialAlloc->Add (Vector (211 , 161, 3));
initialAlloc->Add (Vector (80 , 27, 3));
initialAlloc->Add (Vector (161 , 79, 3));
initialAlloc->Add (Vector (209 , 297, 3));
}
if(INST==22){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (100 , 0, 3)); //initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (50 , 50, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (209 , 297, 3)); // comentar nas simulações de 30 nós
initialAlloc->Add (Vector (270 , 278, 3));
initialAlloc->Add (Vector (48 , 151, 3));
initialAlloc->Add (Vector (287 , 137, 3));
initialAlloc->Add (Vector (167 , 45, 3));
initialAlloc->Add (Vector (84 , 135, 3));
initialAlloc->Add (Vector (124 , 155, 3));
initialAlloc->Add (Vector (285 , 202, 3));
initialAlloc->Add (Vector (69 , 269, 3));
initialAlloc->Add (Vector (63 , 95, 3));
initialAlloc->Add (Vector (141 , 79, 3));
initialAlloc->Add (Vector (173 , 173, 3));
initialAlloc->Add (Vector (57 , 202, 3));
initialAlloc->Add (Vector (137 , 277, 3));
initialAlloc->Add (Vector (112 , 98, 3));
initialAlloc->Add (Vector (281 , 236, 3));
initialAlloc->Add (Vector (5 , 266, 3));
initialAlloc->Add (Vector (190 , 75, 3));
initialAlloc->Add (Vector (235 , 209, 3));
initialAlloc->Add (Vector (182 , 259, 3));
initialAlloc->Add (Vector (173 , 107, 3));
initialAlloc->Add (Vector (290 , 38, 3));
initialAlloc->Add (Vector (240 , 102, 3));
initialAlloc->Add (Vector (29 , 298, 3));
initialAlloc->Add (Vector (200 , 211, 3));
initialAlloc->Add (Vector (271 , 70, 3));
initialAlloc->Add (Vector (228 , 242, 3));
initialAlloc->Add (Vector (1 , 54, 3));
initialAlloc->Add (Vector (2 , 191, 3));
initialAlloc->Add (Vector (218 , 31, 3));
initialAlloc->Add (Vector (242 , 171, 3));
initialAlloc->Add (Vector (112 , 195, 3));
initialAlloc->Add (Vector (138 , 7, 3));
initialAlloc->Add (Vector (37 , 262, 3));
initialAlloc->Add (Vector (30 , 25, 3));
initialAlloc->Add (Vector (96 , 36, 3));
initialAlloc->Add (Vector (257 , 38, 3));
initialAlloc->Add (Vector (279 , 1, 3));
initialAlloc->Add (Vector (26 , 215, 3));
initialAlloc->Add (Vector (108 , 292, 3));
initialAlloc->Add (Vector (151 , 216, 3));
initialAlloc->Add (Vector (30 , 115, 3));
initialAlloc->Add (Vector (83 , 227, 3));
initialAlloc->Add (Vector (65 , 8, 3));
initialAlloc->Add (Vector (207 , 156, 3));
initialAlloc->Add (Vector (134 , 45, 3));
initialAlloc->Add (Vector (147 , 131, 3));
initialAlloc->Add (Vector (176 , 2, 3));
initialAlloc->Add (Vector (293 , 103, 3));
initialAlloc->Add (Vector (222 , 65, 3));
initialAlloc->Add (Vector (205 , 105, 3));
initialAlloc->Add (Vector (119 , 247, 3));
initialAlloc->Add (Vector (85 , 170, 3));
initialAlloc->Add (Vector (176 , 293, 3));
initialAlloc->Add (Vector (0 , 0, 3));
}
if(INST==23){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (100 , 0, 3)); //initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (50 , 50, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (0 , 0, 3));
initialAlloc->Add (Vector (2 , 234, 3)); //comentar nas simulações de 30 nós
initialAlloc->Add (Vector (134 , 274, 3));
initialAlloc->Add (Vector (284 , 202, 3));
initialAlloc->Add (Vector (84 , 219, 3));
initialAlloc->Add (Vector (255 , 139, 3));
initialAlloc->Add (Vector (166 , 17, 3));
initialAlloc->Add (Vector (44 , 183, 3));
initialAlloc->Add (Vector (119 , 111, 3));
initialAlloc->Add (Vector (236 , 283, 3));
initialAlloc->Add (Vector (173 , 201, 3));
initialAlloc->Add (Vector (233 , 233, 3));
initialAlloc->Add (Vector (205 , 201, 3));
initialAlloc->Add (Vector (123 , 189, 3));
initialAlloc->Add (Vector (209 , 0, 3));
initialAlloc->Add (Vector (52 , 126, 3));
initialAlloc->Add (Vector (196 , 126, 3));
initialAlloc->Add (Vector (237 , 90, 3));
initialAlloc->Add (Vector (76 , 274, 3));
initialAlloc->Add (Vector (141 , 143, 3));
initialAlloc->Add (Vector (44 , 246, 3));
initialAlloc->Add (Vector (175 , 244, 3));
initialAlloc->Add (Vector (76 , 96, 3));
initialAlloc->Add (Vector (85 , 65, 3));
initialAlloc->Add (Vector (288 , 14, 3));
initialAlloc->Add (Vector (209 , 63, 3));
initialAlloc->Add (Vector (161 , 171, 3));
initialAlloc->Add (Vector (295 , 238, 3));
initialAlloc->Add (Vector (239 , 17, 3));
initialAlloc->Add (Vector (268 , 64, 3));
initialAlloc->Add (Vector (32 , 6, 3));
initialAlloc->Add (Vector (14 , 65, 3));
initialAlloc->Add (Vector (295 , 82, 3));
initialAlloc->Add (Vector (176 , 282, 3));
initialAlloc->Add (Vector (94 , 144, 3));
initialAlloc->Add (Vector (269 , 259, 3));
initialAlloc->Add (Vector (32 , 291, 3));
initialAlloc->Add (Vector (242 , 169, 3));
initialAlloc->Add (Vector (1 , 192, 3));
initialAlloc->Add (Vector (102 , 247, 3));
initialAlloc->Add (Vector (131 , 25, 3));
initialAlloc->Add (Vector (269 , 292, 3));
initialAlloc->Add (Vector (298 , 172, 3));
initialAlloc->Add (Vector (172 , 100, 3));
initialAlloc->Add (Vector (145 , 223, 3));
initialAlloc->Add (Vector (194 , 170, 3));
initialAlloc->Add (Vector (156 , 64, 3));
initialAlloc->Add (Vector (94 , 34, 3));
initialAlloc->Add (Vector (64 , 20, 3));
initialAlloc->Add (Vector (32 , 214, 3));
initialAlloc->Add (Vector (33 , 94, 3));
initialAlloc->Add (Vector (7 , 270, 3));
initialAlloc->Add (Vector (213 , 259, 3));
initialAlloc->Add (Vector (119 , 67, 3));
initialAlloc->Add (Vector (282 , 116, 3));
}
if(INST==24){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (100 , 0, 3));
initialAlloc->Add (Vector (50 , 50, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (282 , 116, 3));
initialAlloc->Add (Vector (273 , 259, 3));
initialAlloc->Add (Vector (116 , 129, 3)); //comentado na simulação de 30 nós
initialAlloc->Add (Vector (73 , 101, 3));
initialAlloc->Add (Vector (122 , 263, 3));
initialAlloc->Add (Vector (56 , 140, 3));
initialAlloc->Add (Vector (200 , 196, 3));
initialAlloc->Add (Vector (52 , 2, 3));
initialAlloc->Add (Vector (53 , 198, 3));
initialAlloc->Add (Vector (195 , 258, 3));
initialAlloc->Add (Vector (8 , 184, 3));
initialAlloc->Add (Vector (50 , 263, 3));
initialAlloc->Add (Vector (224 , 68, 3));
initialAlloc->Add (Vector (18 , 217, 3));
initialAlloc->Add (Vector (228 , 13, 3));
initialAlloc->Add (Vector (138 , 44, 3));
initialAlloc->Add (Vector (231 , 277, 3));
initialAlloc->Add (Vector (175 , 224, 3));
initialAlloc->Add (Vector (295 , 183, 3));
initialAlloc->Add (Vector (2 , 257, 3));
initialAlloc->Add (Vector (96 , 282, 3));
initialAlloc->Add (Vector (209 , 112, 3));
initialAlloc->Add (Vector (178 , 136, 3));
initialAlloc->Add (Vector (88 , 173, 3));
initialAlloc->Add (Vector (270 , 218, 3));
initialAlloc->Add (Vector (187 , 87, 3));
initialAlloc->Add (Vector (147 , 182, 3));
initialAlloc->Add (Vector (263 , 20, 3));
initialAlloc->Add (Vector (2 , 34, 3));
initialAlloc->Add (Vector (142 , 7, 3));
initialAlloc->Add (Vector (228 , 212, 3));
initialAlloc->Add (Vector (175 , 1, 3));
initialAlloc->Add (Vector (229 , 138, 3));
initialAlloc->Add (Vector (268 , 79, 3));
initialAlloc->Add (Vector (32 , 84, 3));
initialAlloc->Add (Vector (197 , 46, 3));
initialAlloc->Add (Vector (140 , 102, 3));
initialAlloc->Add (Vector (150 , 245, 3));
initialAlloc->Add (Vector (112 , 212, 3));
initialAlloc->Add (Vector (171 , 299, 3));
initialAlloc->Add (Vector (297 , 34, 3));
initialAlloc->Add (Vector (32 , 290, 3));
initialAlloc->Add (Vector (90 , 66, 3));
initialAlloc->Add (Vector (250 , 169, 3));
initialAlloc->Add (Vector (26 , 121, 3));
initialAlloc->Add (Vector (239 , 245, 3));
initialAlloc->Add (Vector (65 , 294, 3));
initialAlloc->Add (Vector (95 , 34, 3));
initialAlloc->Add (Vector (297 , 0, 3));
initialAlloc->Add (Vector (81 , 240, 3));
initialAlloc->Add (Vector (128 , 296, 3));
initialAlloc->Add (Vector (160 , 68, 3));
initialAlloc->Add (Vector (199 , 163, 3));
initialAlloc->Add (Vector (246 , 106, 3));
initialAlloc->Add (Vector (298 , 238, 3));
}
if(INST==25){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 100, 3));
initialAlloc->Add (Vector (100 , 0, 3));//initialAlloc->Add (Vector (200 , 0, 3));
initialAlloc->Add (Vector (50 , 50, 3));
initialAlloc->Add (Vector (0 , 150, 3));//initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (298 , 238, 3));
initialAlloc->Add (Vector (161 , 288, 3));
initialAlloc->Add (Vector (160 , 68, 3));
initialAlloc->Add (Vector (260 , 93, 3));
initialAlloc->Add (Vector (215 , 252, 3));
initialAlloc->Add (Vector (245 , 157, 3));
initialAlloc->Add (Vector (64 , 182, 3));
initialAlloc->Add (Vector (23 , 182, 3));
initialAlloc->Add (Vector (276 , 269, 3));
initialAlloc->Add (Vector (64 , 255, 3));
initialAlloc->Add (Vector (178 , 98, 3));
initialAlloc->Add (Vector (222 , 112, 3));
initialAlloc->Add (Vector (203 , 157, 3));
initialAlloc->Add (Vector (125 , 53, 3));
initialAlloc->Add (Vector (181 , 38, 3));
initialAlloc->Add (Vector (93 , 41, 3));
initialAlloc->Add (Vector (91 , 232, 3));
initialAlloc->Add (Vector (22 , 216, 3));
initialAlloc->Add (Vector (32 , 136, 3));
initialAlloc->Add (Vector (137 , 158, 3));
initialAlloc->Add (Vector (35 , 299, 3));
initialAlloc->Add (Vector (286 , 144, 3));
initialAlloc->Add (Vector (225 , 77, 3));
initialAlloc->Add (Vector (128 , 244, 3));
initialAlloc->Add (Vector (238 , 47, 3));
initialAlloc->Add (Vector (290 , 176, 3));
initialAlloc->Add (Vector (104 , 79, 3));
initialAlloc->Add (Vector (261 , 232, 3));
initialAlloc->Add (Vector (118 , 191, 3));
initialAlloc->Add (Vector (292 , 96, 3));
initialAlloc->Add (Vector (287 , 44, 3));
initialAlloc->Add (Vector (146 , 112, 3));
initialAlloc->Add (Vector (11 , 58, 3));
initialAlloc->Add (Vector (167 , 173, 3));
initialAlloc->Add (Vector (88 , 145, 3));
initialAlloc->Add (Vector (64 , 21, 3));
initialAlloc->Add (Vector (4 , 288, 3));
initialAlloc->Add (Vector (210 , 194, 3));
initialAlloc->Add (Vector (166 , 1, 3));
initialAlloc->Add (Vector (150 , 205, 3));
initialAlloc->Add (Vector (25 , 19, 3));
initialAlloc->Add (Vector (58 , 219, 3));
initialAlloc->Add (Vector (91 , 283, 3));
initialAlloc->Add (Vector (65 , 121, 3));
initialAlloc->Add (Vector (173 , 255, 3));
initialAlloc->Add (Vector (256 , 17, 3));
initialAlloc->Add (Vector (227 , 295, 3));
initialAlloc->Add (Vector (288 , 1, 3));
initialAlloc->Add (Vector (186 , 220, 3));
initialAlloc->Add (Vector (216 , 10, 3));
initialAlloc->Add (Vector (59 , 85, 3));
initialAlloc->Add (Vector (124 , 277, 3));
initialAlloc->Add (Vector (250 , 196, 3));
initialAlloc->Add (Vector (100 , 111, 3));
initialAlloc->Add (Vector (26 , 263, 3));
}
if(INST==26){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (26 , 263, 3));
initialAlloc->Add (Vector (212 , 121, 3));
initialAlloc->Add (Vector (183 , 233, 3));
initialAlloc->Add (Vector (102 , 199, 3));
initialAlloc->Add (Vector (263 , 198, 3));
initialAlloc->Add (Vector (184 , 2, 3));
initialAlloc->Add (Vector (197 , 178, 3));
initialAlloc->Add (Vector (235 , 265, 3));
initialAlloc->Add (Vector (282 , 229, 3));
initialAlloc->Add (Vector (64 , 233, 3));
initialAlloc->Add (Vector (224 , 32, 3));
initialAlloc->Add (Vector (182 , 287, 3));
initialAlloc->Add (Vector (115 , 91, 3));
initialAlloc->Add (Vector (297 , 79, 3));
initialAlloc->Add (Vector (12 , 67, 3));
initialAlloc->Add (Vector (131 , 28, 3));
initialAlloc->Add (Vector (265 , 16, 3));
initialAlloc->Add (Vector (30 , 230, 3));
initialAlloc->Add (Vector (169 , 118, 3));
initialAlloc->Add (Vector (69 , 105, 3));
initialAlloc->Add (Vector (88 , 10, 3));
initialAlloc->Add (Vector (283 , 270, 3));
initialAlloc->Add (Vector (297 , 150, 3));
initialAlloc->Add (Vector (62 , 46, 3));
initialAlloc->Add (Vector (93 , 58, 3));
initialAlloc->Add (Vector (147 , 147, 3));
initialAlloc->Add (Vector (17 , 32, 3));
initialAlloc->Add (Vector (104 , 276, 3));
initialAlloc->Add (Vector (247 , 71, 3));
initialAlloc->Add (Vector (180 , 41, 3));
initialAlloc->Add (Vector (155 , 262, 3));
initialAlloc->Add (Vector (99 , 237, 3));
initialAlloc->Add (Vector (83 , 155, 3));
initialAlloc->Add (Vector (34 , 183, 3));
initialAlloc->Add (Vector (273 , 111, 3));
initialAlloc->Add (Vector (53 , 0, 3));
initialAlloc->Add (Vector (134 , 290, 3));
initialAlloc->Add (Vector (174 , 79, 3));
initialAlloc->Add (Vector (50 , 132, 3));
initialAlloc->Add (Vector (62 , 278, 3));
initialAlloc->Add (Vector (228 , 208, 3));
initialAlloc->Add (Vector (297 , 38, 3));
initialAlloc->Add (Vector (42 , 80, 3));
initialAlloc->Add (Vector (160 , 199, 3));
initialAlloc->Add (Vector (246 , 158, 3));
initialAlloc->Add (Vector (5 , 118, 3));
initialAlloc->Add (Vector (10 , 0, 3));
initialAlloc->Add (Vector (146 , 230, 3));
initialAlloc->Add (Vector (215 , 87, 3));
initialAlloc->Add (Vector (66 , 199, 3));
initialAlloc->Add (Vector (294 , 2, 3));
initialAlloc->Add (Vector (121 , 169, 3));
initialAlloc->Add (Vector (31 , 298, 3));
initialAlloc->Add (Vector (214 , 294, 3));
initialAlloc->Add (Vector (0 , 283, 3));
}
if(INST==27){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (0 , 283, 3));
initialAlloc->Add (Vector (117 , 170, 3));
initialAlloc->Add (Vector (272 , 252, 3));
initialAlloc->Add (Vector (124 , 125, 3));
initialAlloc->Add (Vector (13 , 78, 3));
initialAlloc->Add (Vector (182 , 27, 3));
initialAlloc->Add (Vector (65 , 110, 3));
initialAlloc->Add (Vector (284 , 157, 3));
initialAlloc->Add (Vector (101 , 268, 3));
initialAlloc->Add (Vector (157 , 248, 3));
initialAlloc->Add (Vector (268 , 129, 3));
initialAlloc->Add (Vector (200 , 144, 3));
initialAlloc->Add (Vector (222 , 189, 3));
initialAlloc->Add (Vector (293 , 39, 3));
initialAlloc->Add (Vector (20 , 16, 3));
initialAlloc->Add (Vector (288 , 5, 3));
initialAlloc->Add (Vector (129 , 49, 3));
initialAlloc->Add (Vector (67 , 276, 3));
initialAlloc->Add (Vector (85 , 20, 3));
initialAlloc->Add (Vector (121 , 92, 3));
initialAlloc->Add (Vector (286 , 95, 3));
initialAlloc->Add (Vector (215 , 80, 3));
initialAlloc->Add (Vector (234 , 236, 3));
initialAlloc->Add (Vector (96 , 222, 3));
initialAlloc->Add (Vector (41 , 155, 3));
initialAlloc->Add (Vector (79 , 77, 3));
initialAlloc->Add (Vector (20 , 113, 3));
initialAlloc->Add (Vector (157 , 82, 3));
initialAlloc->Add (Vector (34 , 266, 3));
initialAlloc->Add (Vector (158 , 208, 3));
initialAlloc->Add (Vector (262 , 191, 3));
initialAlloc->Add (Vector (55 , 200, 3));
initialAlloc->Add (Vector (248 , 294, 3));
initialAlloc->Add (Vector (186 , 60, 3));
initialAlloc->Add (Vector (189 , 109, 3));
initialAlloc->Add (Vector (190 , 286, 3));
initialAlloc->Add (Vector (53 , 15, 3));
initialAlloc->Add (Vector (163 , 158, 3));
initialAlloc->Add (Vector (206 , 255, 3));
initialAlloc->Add (Vector (89 , 133, 3));
initialAlloc->Add (Vector (28 , 224, 3));
initialAlloc->Add (Vector (126 , 239, 3));
initialAlloc->Add (Vector (222 , 21, 3));
initialAlloc->Add (Vector (131 , 280, 3));
initialAlloc->Add (Vector (258 , 23, 3));
initialAlloc->Add (Vector (295 , 207, 3));
initialAlloc->Add (Vector (201 , 219, 3));
initialAlloc->Add (Vector (66 , 236, 3));
initialAlloc->Add (Vector (251 , 67, 3));
initialAlloc->Add (Vector (112 , 2, 3));
initialAlloc->Add (Vector (42 , 64, 3));
initialAlloc->Add (Vector (244 , 99, 3));
initialAlloc->Add (Vector (158 , 120, 3));
initialAlloc->Add (Vector (243 , 160, 3));
initialAlloc->Add (Vector (74 , 168, 3));
}
if(INST==28){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (74 , 168, 3));
initialAlloc->Add (Vector (131 , 149, 3));
initialAlloc->Add (Vector (232 , 228, 3));
initialAlloc->Add (Vector (144 , 77, 3));
initialAlloc->Add (Vector (97 , 278, 3));
initialAlloc->Add (Vector (272 , 205, 3));
initialAlloc->Add (Vector (161 , 278, 3));
initialAlloc->Add (Vector (117 , 55, 3));
initialAlloc->Add (Vector (120 , 220, 3));
initialAlloc->Add (Vector (171 , 138, 3));
initialAlloc->Add (Vector (57 , 65, 3));
initialAlloc->Add (Vector (22 , 38, 3));
initialAlloc->Add (Vector (66 , 211, 3));
initialAlloc->Add (Vector (191 , 298, 3));
initialAlloc->Add (Vector (3 , 292, 3));
initialAlloc->Add (Vector (226 , 145, 3));
initialAlloc->Add (Vector (229 , 24, 3));
initialAlloc->Add (Vector (179 , 98, 3));
initialAlloc->Add (Vector (70 , 15, 3));
initialAlloc->Add (Vector (189 , 233, 3));
initialAlloc->Add (Vector (51 , 299, 3));
initialAlloc->Add (Vector (37 , 117, 3));
initialAlloc->Add (Vector (56 , 264, 3));
initialAlloc->Add (Vector (277 , 111, 3));
initialAlloc->Add (Vector (115 , 6, 3));
initialAlloc->Add (Vector (258 , 166, 3));
initialAlloc->Add (Vector (283 , 267, 3));
initialAlloc->Add (Vector (167 , 177, 3));
initialAlloc->Add (Vector (133 , 257, 3));
initialAlloc->Add (Vector (113 , 113, 3));
initialAlloc->Add (Vector (74 , 133, 3));
initialAlloc->Add (Vector (234 , 284, 3));
initialAlloc->Add (Vector (296 , 61, 3));
initialAlloc->Add (Vector (268 , 38, 3));
initialAlloc->Add (Vector (218 , 106, 3));
initialAlloc->Add (Vector (130 , 294, 3));
initialAlloc->Add (Vector (285 , 8, 3));
initialAlloc->Add (Vector (294 , 231, 3));
initialAlloc->Add (Vector (33 , 240, 3));
initialAlloc->Add (Vector (117 , 180, 3));
initialAlloc->Add (Vector (166 , 38, 3));
initialAlloc->Add (Vector (213 , 53, 3));
initialAlloc->Add (Vector (86 , 87, 3));
initialAlloc->Add (Vector (40 , 182, 3));
initialAlloc->Add (Vector (203 , 268, 3));
initialAlloc->Add (Vector (297 , 164, 3));
initialAlloc->Add (Vector (209 , 176, 3));
initialAlloc->Add (Vector (256 , 86, 3));
initialAlloc->Add (Vector (16 , 0, 3));
initialAlloc->Add (Vector (263 , 242, 3));
initialAlloc->Add (Vector (203 , 0, 3));
initialAlloc->Add (Vector (85 , 47, 3));
initialAlloc->Add (Vector (152 , 213, 3));
initialAlloc->Add (Vector (87 , 242, 3));
initialAlloc->Add (Vector (10 , 75, 3));
}
if(INST==29){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (10 , 75, 3));
initialAlloc->Add (Vector (55 , 87, 3));
initialAlloc->Add (Vector (204 , 245, 3));
initialAlloc->Add (Vector (259 , 267, 3));
initialAlloc->Add (Vector (159 , 41, 3));
initialAlloc->Add (Vector (180 , 291, 3));
initialAlloc->Add (Vector (52 , 248, 3));
initialAlloc->Add (Vector (146 , 129, 3));
initialAlloc->Add (Vector (285 , 99, 3));
initialAlloc->Add (Vector (292 , 49, 3));
initialAlloc->Add (Vector (239 , 208, 3));
initialAlloc->Add (Vector (223 , 42, 3));
initialAlloc->Add (Vector (48 , 160, 3));
initialAlloc->Add (Vector (286 , 155, 3));
initialAlloc->Add (Vector (101 , 258, 3));
initialAlloc->Add (Vector (209 , 148, 3));
initialAlloc->Add (Vector (242 , 163, 3));
initialAlloc->Add (Vector (111 , 294, 3));
initialAlloc->Add (Vector (163 , 257, 3));
initialAlloc->Add (Vector (176 , 201, 3));
initialAlloc->Add (Vector (108 , 175, 3));
initialAlloc->Add (Vector (188 , 101, 3));
initialAlloc->Add (Vector (61 , 200, 3));
initialAlloc->Add (Vector (125 , 235, 3));
initialAlloc->Add (Vector (115 , 7, 3));
initialAlloc->Add (Vector (277 , 13, 3));
initialAlloc->Add (Vector (13 , 291, 3));
initialAlloc->Add (Vector (25 , 32, 3));
initialAlloc->Add (Vector (149 , 88, 3));
initialAlloc->Add (Vector (77 , 295, 3));
initialAlloc->Add (Vector (125 , 58, 3));
initialAlloc->Add (Vector (101 , 122, 3));
initialAlloc->Add (Vector (295 , 267, 3));
initialAlloc->Add (Vector (282 , 221, 3));
initialAlloc->Add (Vector (217 , 281, 3));
initialAlloc->Add (Vector (94 , 87, 3));
initialAlloc->Add (Vector (27 , 105, 3));
initialAlloc->Add (Vector (251 , 104, 3));
initialAlloc->Add (Vector (74 , 58, 3));
initialAlloc->Add (Vector (147 , 164, 3));
initialAlloc->Add (Vector (263 , 65, 3));
initialAlloc->Add (Vector (90 , 222, 3));
initialAlloc->Add (Vector (213 , 180, 3));
initialAlloc->Add (Vector (192 , 8, 3));
initialAlloc->Add (Vector (179 , 68, 3));
initialAlloc->Add (Vector (64 , 26, 3));
initialAlloc->Add (Vector (249 , 299, 3));
initialAlloc->Add (Vector (15 , 251, 3));
initialAlloc->Add (Vector (5 , 6, 3));
initialAlloc->Add (Vector (83 , 151, 3));
initialAlloc->Add (Vector (228 , 76, 3));
initialAlloc->Add (Vector (143 , 202, 3));
initialAlloc->Add (Vector (231 , 6, 3));
initialAlloc->Add (Vector (27 , 219, 3));
initialAlloc->Add (Vector (58 , 125, 3));
}
if(INST==30){
initialAlloc->Add (Vector (300 , 300, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (150 , 0, 3));
initialAlloc->Add (Vector (0 , 200, 3));
initialAlloc->Add (Vector (0 , 150, 3));
initialAlloc->Add (Vector (58 , 125, 3));
initialAlloc->Add (Vector (133 , 71, 3));
initialAlloc->Add (Vector (230 , 121, 3));
initialAlloc->Add (Vector (52 , 54, 3));
initialAlloc->Add (Vector (182 , 299, 3));
initialAlloc->Add (Vector (198 , 139, 3));
initialAlloc->Add (Vector (266 , 285, 3));
initialAlloc->Add (Vector (113 , 207, 3));
initialAlloc->Add (Vector (169 , 247, 3));
initialAlloc->Add (Vector (175 , 29, 3));
initialAlloc->Add (Vector (36 , 26, 3));
initialAlloc->Add (Vector (199 , 171, 3));
initialAlloc->Add (Vector (243 , 7, 3));
initialAlloc->Add (Vector (198 , 6, 3));
initialAlloc->Add (Vector (44 , 213, 3));
initialAlloc->Add (Vector (175 , 213, 3));
initialAlloc->Add (Vector (268 , 119, 3));
initialAlloc->Add (Vector (240 , 182, 3));
initialAlloc->Add (Vector (277 , 49, 3));
initialAlloc->Add (Vector (295 , 192, 3));
initialAlloc->Add (Vector (172 , 95, 3));
initialAlloc->Add (Vector (60 , 284, 3));
initialAlloc->Add (Vector (230 , 47, 3));
initialAlloc->Add (Vector (125 , 251, 3));
initialAlloc->Add (Vector (216 , 79, 3));
initialAlloc->Add (Vector (4 , 62, 3));
initialAlloc->Add (Vector (248 , 253, 3));
initialAlloc->Add (Vector (4 , 234, 3));
initialAlloc->Add (Vector (65 , 250, 3));
initialAlloc->Add (Vector (74 , 12, 3));
initialAlloc->Add (Vector (265 , 79, 3));
initialAlloc->Add (Vector (5 , 18, 3));
initialAlloc->Add (Vector (63 , 165, 3));
initialAlloc->Add (Vector (144 , 291, 3));
initialAlloc->Add (Vector (131 , 162, 3));
initialAlloc->Add (Vector (95 , 72, 3));
initialAlloc->Add (Vector (221 , 235, 3));
initialAlloc->Add (Vector (267 , 218, 3));
initialAlloc->Add (Vector (165 , 176, 3));
initialAlloc->Add (Vector (114 , 35, 3));
initialAlloc->Add (Vector (26 , 127, 3));
initialAlloc->Add (Vector (111 , 119, 3));
initialAlloc->Add (Vector (281 , 151, 3));
initialAlloc->Add (Vector (233 , 299, 3));
initialAlloc->Add (Vector (297 , 86, 3));
initialAlloc->Add (Vector (293 , 5, 3));
initialAlloc->Add (Vector (16 , 94, 3));
initialAlloc->Add (Vector (92 , 296, 3));
initialAlloc->Add (Vector (6 , 294, 3));
initialAlloc->Add (Vector (286 , 258, 3));
initialAlloc->Add (Vector (77 , 204, 3));
initialAlloc->Add (Vector (95 , 263, 3));
initialAlloc->Add (Vector (179 , 63, 3));
initialAlloc->Add (Vector (29 , 169, 3));
initialAlloc->Add (Vector (32 , 264, 3));
}

    // Assign the nodes mobility models
    mobility.SetPositionAllocator(initialAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Install the protocol stack
    InternetStackHelper internet;
    internet.Install(c);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    NS_LOG_INFO("Assign IP Addresses.");
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    // Hello Application
    for (int n = 0; n < numNodes; n++){
        Ptr<Socket> recvSink = Socket::CreateSocket(c.Get(n), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8000);
        recvSink->Bind(local);
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacketHello));
    }

    for (int n = 0; n < numNodes; n++){
        Ptr<Socket> source = Socket::CreateSocket(c.Get(n), tid);
        InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"), 8000);
        source->SetAllowBroadcast(true);
        source->Connect(remote);

        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
        Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                       Seconds(var->GetValue(0.0, intervalHello)), &GenerateTrafficHello,
                                       source, packetSize, 1, interPacketIntervalHello, &HelloComplete, timeToRestoration); 
    }

    // TC Application
    for (int n = 0; n < numNodes; n++){
        Ptr<Socket> recvSinkTC = Socket::CreateSocket(c.Get(n), tid);
        InetSocketAddress localTC = InetSocketAddress(Ipv4Address::GetAny(), 8001);
        recvSinkTC->Bind(localTC);
        recvSinkTC->SetRecvCallback(MakeCallback(&ReceivePacketTC));
    }

    for (int n = 0; n < numNodes; n++){
        Ptr<Socket> sourceTC = Socket::CreateSocket(c.Get(n), tid);
        InetSocketAddress remoteTC = InetSocketAddress(Ipv4Address("255.255.255.255"), 8001);
        sourceTC->SetAllowBroadcast(true);
        sourceTC->Connect(remoteTC);

        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
        Simulator::ScheduleWithContext(sourceTC->GetNode()->GetId(),
                                       Seconds(var->GetValue(intervalHello*listSize + intervalHello, ( intervalHello*listSize + intervalTC))), 
                                        &GenerateTrafficTC, sourceTC, packetSize, 1, interPacketIntervalTC, &TcComplete, timeToRestoration);   
    }

    // Simulate the video stream without the hello and TC messages: 
    Simulator::Schedule(Seconds(FitpathCallTime - intervalHello), &SetHelloComplete, &HelloComplete); // Stop generating hello messages before calling fitpath
    Simulator::Schedule(Seconds(FitpathCallTime - intervalTC), &SetTcComplete, &TcComplete); // Stop generating TC messages before calling fitpath

    Simulator::Schedule (Seconds(FitpathCallTime), &CallInputFitPath, numNodes, INST, ref, c, interfaces, packetSizeEvalvid);
    
    // Simulate nodes failure:
    // int nodeFail1 = 8;
    // int nodeFail2 = 7;
    // Simulator::Schedule(Seconds(timeToFailure), &TearDownLink, 1, c.Get(nodeFail1), nodeFail1);
    // Simulator::Schedule(Seconds(timeToFailure), &TearDownLink, 1, c.Get(nodeFail2), nodeFail2);


    Simulator::Stop(Seconds(TotalTime));
    Simulator::Run();

    //PrintTCMessage(numNodes);
    //PrintHelloMessage(numNodes);
    
    std::stringstream fileName;
    fileName.str("");
    fileName << baseDirEvalvid << "macQueue/macQueue-"<< INST <<"-"<< ref;
    static std::ofstream file;  
    file.open(fileName.str(),std::ios_base::trunc); //app

    for(int i=0; i<numNodes; i++){
    file  << i+1 << "	" <<  nodeQueue[i] << "	"  << traceQueue[i]  << std::endl;
    }
    
    Simulator::Destroy();
    return 0;
}
