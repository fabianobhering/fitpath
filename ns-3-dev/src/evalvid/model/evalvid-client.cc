/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *
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
 * Author: Billy Pinheiro <haquiticos@gmail.com>
 *         Saulo da Mata <damata.saulo@gmail.com>
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "evalvid-client.h"
#include "ns3/flow-id-tag.h"
#include "ns3/seq-ts-header.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"

#include <stdlib.h>
#include <stdio.h>
#include <vector> 
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;
using namespace ns3;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EvalvidClient");
NS_OBJECT_ENSURE_REGISTERED (EvalvidClient);

TypeId
EvalvidClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EvalvidClient")
    .SetParent<Application> ()
    .AddConstructor<EvalvidClient> ()
    .AddAttribute ("RemoteAddress",
                   "The destination Ipv4Address of the outbound packets",
                   Ipv4AddressValue (),
                   MakeIpv4AddressAccessor (&EvalvidClient::m_peerAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&EvalvidClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ReceiverDumpFilename",
                   "Receiver Dump Filename",
                   StringValue(""),
                   MakeStringAccessor(&EvalvidClient::receiverDumpFileName),
                   MakeStringChecker())
    .AddAttribute ("PathsFileName",
                   "Paths File Name",
                   StringValue(""),
                   MakeStringAccessor(&EvalvidClient::m_pathsFileName),
                   MakeStringChecker())
    .AddAttribute ("PathsFileName2",
                   "Paths File Name2",
                   StringValue(""),
                   MakeStringAccessor(&EvalvidClient::m_pathsFileName2),
                   MakeStringChecker())
    .AddAttribute ("FlowId",
				   "Packet flow id",
				   UintegerValue (0),
				   MakeUintegerAccessor (&EvalvidClient::m_fid),
				   MakeUintegerChecker<UintegerValue> (0,10000000))  
    .AddAttribute ("TraceCode",
				   "Code of Trace",
				   UintegerValue (0),
				   MakeUintegerAccessor (&EvalvidClient::m_traceCode),
				   MakeUintegerChecker<UintegerValue> (0,10000000))            
    ;
  return tid;
}

const int32_t numNodesMax = 60;

struct PathInfo {
  uint16_t firstPathSizeVec;
  vector<uint16_t> firstPath;
  uint16_t secondPathSizeVec;
  vector<uint16_t> secondPath;
  uint16_t trCode;
};

std::vector<PathInfo> allPathListClient[numNodesMax];

struct PathHeader : public Header{   
  
  uint16_t firstSizeVec; 
  uint16_t firstPathHeader[numNodesMax];
  uint16_t secondSizeVec; 
  uint16_t secondPathHeader[numNodesMax];
  uint16_t traceCode; // SeqNum received from Server

  PathHeader (){
      firstSizeVec=0; 
      secondSizeVec=0;
      traceCode=0;
  }

  uint32_t GetSerializedSize() const override {
    return ((3 * sizeof(uint16_t)) + (firstSizeVec * sizeof(uint16_t)) + (secondSizeVec * sizeof(uint16_t))); 
  }

  void Serialize(Buffer::Iterator start) const override {
    start.WriteHtonU16(firstSizeVec);
    for (uint8_t j = 0; j < firstSizeVec; j++){
        start.WriteHtonU16(firstPathHeader[j]);
    }
    start.WriteHtonU16(secondSizeVec);
    for (uint8_t i = 0; i < secondSizeVec; i++){
     start.WriteHtonU16(secondPathHeader[i]);
    }
    start.WriteHtonU16(traceCode);
  }

  uint32_t Deserialize(Buffer::Iterator start) override { 
    firstSizeVec = start.ReadNtohU16();
    for (uint8_t j = 0; j < firstSizeVec; j++){
        firstPathHeader[j] = start.ReadNtohU16();
    }
    secondSizeVec = start.ReadNtohU16();
    for (uint8_t i = 0; i < secondSizeVec; i++){
        secondPathHeader[i] = start.ReadNtohU16();
    }
    traceCode = start.ReadNtohU16();
    return GetSerializedSize();
  }

  void Print(std::ostream &os) const override {
    os << "Next Hop Node: ";
    for (uint8_t i = 0; i < firstSizeVec; i++){
      os << firstPathHeader[i] << " ";
    os << std::endl;
    }
  }

  TypeId GetInstanceTypeId() const override {
    return GetTypeId();
  }
};

EvalvidClient::EvalvidClient ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_sendEvent = EventId ();
}

EvalvidClient::~EvalvidClient ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
EvalvidClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  m_peerAddress = ip;
  m_peerPort = port;
}

void
EvalvidClient::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Application::DoDispose ();
}

void
EvalvidClient::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS();
  //NS_LOG_INFO ("Start Client Application Evalvid in " << std::fixed << Simulator::Now().ToDouble(Time::S));

  m_peerSource = (m_peerPort + 1)/2;
  allPathListClient[m_peerSource].clear();

  std::string pathsFileName;
  //pathsFileName = "../../inst/newILS/copia-18-provisao/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-28/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-38/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-12/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-13/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-13a/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-55/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-55a/0new-route300_60-1-ref0-1";
  //pathsFileName = "../../inst/newILS/copia-26/0new-route300_60-1-ref0-1";
  pathsFileName = "../../inst/newILS/inst16-2s-512/0new-route300_60-16-ref0-1";

  PathInfo newPath;
  //std::ifstream outputFitpathFile(m_pathsFileName.c_str()); // Paths generated by FITPATH
  //std::ifstream outputFitpathFile(m_pathsFileName2.c_str()); // Paths generated by FITPATH

  std::ifstream outputFitpathFile(pathsFileName.c_str()); // Paths generated by FITPATH  

    if (outputFitpathFile.is_open()) {
        std::string line;
        while (std::getline(outputFitpathFile, line)) {
          std::istringstream iss(line);
          uint32_t node;
          std::vector<uint16_t> route;

          // Read line and store in vector route
          while (iss >> node) {
            route.push_back(node);
          }
  
          //Verify if the last node is equal the source
          if (!route.empty() && route.back() == m_peerSource) {
            if (newPath.firstPath.empty()){
              newPath.firstPath = route;
            } else{
              newPath.secondPath = route;
            }
          } 
        }
      allPathListClient[m_peerSource].push_back(newPath);
      outputFitpathFile.close();
    } else {
       std::cout << ">> EvalvidClient (StartApplication): Unable to open the file 0new-route300." << std::endl;
    }
  
  if (m_socket == 0){
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_peerPort));
      // m_socket->Bind ();
      // m_socket->Connect (InetSocketAddress (m_peerAddress, m_peerPort)); //next hop IP address;
    }
 
  receiverDumpFile.open(receiverDumpFileName.c_str(), ios::out);
  if (receiverDumpFile.fail()){
      NS_FATAL_ERROR(">> EvalvidClient: Error while opening output file: " << receiverDumpFileName.c_str());
      return;
  }
  
  m_socket->SetRecvCallback (MakeCallback (&EvalvidClient::HandleRead, this));

  //Delay requesting to get server on line.
 Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  m_sendEvent = Simulator::Schedule (Seconds(var->GetValue(0.0, 0.2)), &EvalvidClient::Send, this);//Seconds(0.1)
  //m_sendEvent = Simulator::Schedule (Seconds(0.0), &EvalvidClient::Send, this);//Seconds(0.1)

}

void
EvalvidClient::RecallSecondSol1 (void)//adicionei em 26/10/2023
{

allPathListClient[m_peerSource].clear();
  PathInfo newPath;

  std::string pathsFileName;
  //pathsFileName = "../../inst/newILS/copia-18-provisao/0new-route300_60-1-ref0-2"; 
  //pathsFileName = "../../inst/newILS/copia-28/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-38/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-12/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-13/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-13a/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-55/0new-route300_60-1-ref0-2";
  //pathsFileName = "../../inst/newILS/copia-55a/0new-route300_60-1-ref0-2";
  pathsFileName = "../../inst/newILS/inst16-2s-512/0new-route300_60-16-ref0-2";
  
  //std::ifstream outputFitpathFile(m_pathsFileName2.c_str()); // Paths generated by FITPATH 
  std::ifstream outputFitpathFile(pathsFileName.c_str()); // caminho com o nó 48

    if (outputFitpathFile.is_open()) {
        std::string line;
        while (std::getline(outputFitpathFile, line)) {
          std::istringstream iss(line);
          uint32_t node;
          std::vector<uint16_t> route;

          // Read line and store in vector route
          while (iss >> node) {
            route.push_back(node);
          }
  
          //Verify if the last node is equal the source
          if (!route.empty() && route.back() == m_peerSource) {
            if (newPath.firstPath.empty()){
              newPath.firstPath = route;
            } else{
              newPath.secondPath = route;
            }
          } 
        }
      allPathListClient[m_peerSource].push_back(newPath);
      outputFitpathFile.close();
    } else {
       std::cout << ">> EvalvidClient (RecallSecondSol1): Unable to open the file 0new-route300." << std::endl;
    }

    cout << "Source-Second Solution " << m_peerSource << " - Path 1: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].firstPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].firstPath[i] << " ";
  } std::cout << std::endl;

  cout << "Source-Second Solution " << m_peerSource << " - Path 2: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].secondPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].secondPath[i] << " ";
  } std::cout << std::endl;

  //Simulator::Schedule (Seconds(0.0), &EvalvidClient::Send, this);
  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  m_sendEvent = Simulator::Schedule (Seconds(var->GetValue(0.0, 0.1)), &EvalvidClient::Send, this);//Seconds(0.1)

}

void
EvalvidClient::RecallSecondSol (void)//adicionei em 26/10/2023
{

allPathListClient[m_peerSource].clear();
  PathInfo newPath;
  std::ifstream outputFitpathFile(m_pathsFileName2.c_str()); // Solução otimizada do FITPATH 

    if (outputFitpathFile.is_open()) {
        std::string line;
        while (std::getline(outputFitpathFile, line)) {
          std::istringstream iss(line);
          uint32_t node;
          std::vector<uint16_t> route;

          // Read line and store in vector route
          while (iss >> node) {
            route.push_back(node);
          }
  
          //Verify if the last node is equal the source
          if (!route.empty() && route.back() == m_peerSource) {
            if (newPath.firstPath.empty()){
              newPath.firstPath = route;
            } else{
              newPath.secondPath = route;
            }
          } 
        }
      allPathListClient[m_peerSource].push_back(newPath);
      outputFitpathFile.close();
    } else {
       std::cout << ">> EvalvidClient (RecallSecondSol): Unable to open the file 0new-route300." << std::endl;
    }

    cout << "Source-Second Solution " << m_peerSource << " - Path 1: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].firstPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].firstPath[i] << " ";
  } std::cout << std::endl;

  cout << "Source-Second Solution " << m_peerSource << " - Path 2: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].secondPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].secondPath[i] << " ";
  } std::cout << std::endl;

  //Simulator::Schedule (Seconds(0.0), &EvalvidClient::Send, this);
  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  m_sendEvent = Simulator::Schedule (Seconds(var->GetValue(0.0, 0.1)), &EvalvidClient::Send, this);//Seconds(0.1)

}

void
EvalvidClient::Recall (void)//adicionei em 26/10/2023
{

allPathListClient[m_peerSource].clear();
  PathInfo newPath;
  std::ifstream outputFitpathFile(m_pathsFileName.c_str()); // Solução inicial do FITPATH 

    if (outputFitpathFile.is_open()) {
        std::string line;
        while (std::getline(outputFitpathFile, line)) {
          std::istringstream iss(line);
          uint32_t node;
          std::vector<uint16_t> route;

          // Read line and store in vector route
          while (iss >> node) {
            route.push_back(node);
          }
  
          //Verify if the last node is equal the source
          if (!route.empty() && route.back() == m_peerSource) {
            if (newPath.firstPath.empty()){
              newPath.firstPath = route;
            } else{
              newPath.secondPath = route;
            }
          } 
        }
      allPathListClient[m_peerSource].push_back(newPath);
      outputFitpathFile.close();
    } else {
       std::cout << ">> EvalvidClient (Recall): Unable to open the file 0new-route300." << std::endl;
    }

    cout << "Source-recall " << m_peerSource << " - Path 1: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].firstPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].firstPath[i] << " ";
  } std::cout << std::endl;

  cout << "Source-recall " << m_peerSource << " - Path 2: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].secondPath.size(); i++){
    cout << allPathListClient[m_peerSource][0].secondPath[i] << " ";
  } std::cout << std::endl;

  //Simulator::Schedule (Seconds(0.0), &EvalvidClient::Send, this);
  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  m_sendEvent = Simulator::Schedule (Seconds(var->GetValue(0.0, 0.1)), &EvalvidClient::Send, this);//Seconds(0.1)
}

void
EvalvidClient::Send (void)//void
{
  NS_LOG_FUNCTION_NOARGS ();
  //NS_LOG_UNCOND("m_peerSource no send "<< m_peerSource);
  //Create the header object
  PathHeader pathHeader1;
  pathHeader1.traceCode = m_traceCode;
  pathHeader1.firstSizeVec = allPathListClient[m_peerSource][0].firstPath.size();
  pathHeader1.secondSizeVec = allPathListClient[m_peerSource][0].secondPath.size();
NS_LOG_UNCOND ( "======================================================================================================================================================");
  cout << "Source " << m_peerSource << " - Path 1: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].firstPath.size(); i++){
    pathHeader1.firstPathHeader[i] = allPathListClient[m_peerSource][0].firstPath[i];
    cout << pathHeader1.firstPathHeader[i] << " ";
  } std::cout << std::endl;

  cout << "Source " << m_peerSource << " - Path 2: ";
  for (uint8_t i=0; i < allPathListClient[m_peerSource][0].secondPath.size(); i++){
    pathHeader1.secondPathHeader[i] = allPathListClient[m_peerSource][0].secondPath[i];
    cout << pathHeader1.secondPathHeader[i] << " ";
  } std::cout << std::endl;
 
  uint32_t nextAddr = ((pathHeader1.firstPathHeader[1]) + 1);
  std::string nextAddress = "10.1.1." + std::to_string(nextAddr);
  ns3::Ipv4Address nextIPAddress(nextAddress.c_str());
  //m_nextIPAddress = nextIPAddress;

  //if (m_socket1 == 0){
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory"); // "ns3::TcpSocketFactory"
    m_socket1 = Socket::CreateSocket (GetNode (), tid);
    //m_socket1->Connect (InetSocketAddress (m_nextIPAddress, m_peerPort)); //next hop IP address;
    m_socket1->Connect (InetSocketAddress (nextIPAddress, m_peerPort)); //next hop IP address;
    //NS_LOG_UNCOND("Sink node Id "<< GetNode()->GetId() << " next IP " << m_nextIPAddress << "  " << " port " << m_peerPort);
    NS_LOG_UNCOND("Sink node Id "<< GetNode()->GetId() << " next IP " << nextAddress << "  " << " port " << m_peerPort);

   // }

  Ptr<Packet> p = Create<Packet> ();

  SeqTsHeader seqTs;
  seqTs.SetSeq (0);
  p->AddHeader (seqTs);
  p->AddHeader (pathHeader1); 
  
  UintegerValue fid;
  this->GetAttribute("FlowId",fid);
  FlowIdTag myflowIdTag(fid.Get());
  p->AddByteTag(myflowIdTag);

  m_socket1->Send (p);

NS_LOG_INFO (">> EvalvidClient: Sending request for video streaming to EvalvidServer at "<< m_peerAddress << ":" << m_peerPort << " by next hop " 
<< nextAddress << " at " << std::fixed << Simulator::Now().ToDouble(Time::S) << "s flowId: "<<fid.Get() << " traceCode: " << pathHeader1.traceCode);

 // Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
 // m_sendEvent2 = Simulator::Schedule (Seconds(130.1), &EvalvidClient::Recall, this);//adicinei em 26/10/2023
  //m_sendEvent2 = Simulator::Schedule (Seconds(var->GetValue(115.1, 116.1)), &EvalvidClient::Recall, this);//adicinei em 26/10/2023

}

void
EvalvidClient::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();
  receiverDumpFile.close();
  Simulator::Cancel (m_sendEvent);
}
 
void
EvalvidClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  uint16_t packetId;
  PathHeader rcvHeader;
 
  
  
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          if (packet->GetSize () > 0)
            {
              
              //packet->RemoveHeader (rcvHeader); //comentado em 08/10/2023
              packet->PeekHeader(rcvHeader);
              uint32_t packetSize = packet->GetSize(); //adicionado em 08/10/2023
              packetSize-=rcvHeader.GetSerializedSize(); //adicionado em 08/10/2023

              packetId = rcvHeader.traceCode; // Server sends the SeqNum to the Client by traceCode header
              
              // SeqTsHeader seqTs;
              // packet->RemoveHeader (seqTs);
              // uint32_t packetId = seqTs.GetSeq (); 
              
              FlowIdTag myflowIdTag;
			        packet->FindFirstMatchingByteTag(myflowIdTag); 	

              //NS_LOG_DEBUG(">> EvalvidClient: Packet received at " << Simulator::Now().GetSeconds()  //comentado em 08/10/2023
              //           << "s\tid: " << packetId << " \tflow: " <<  myflowIdTag.GetFlowId() << " \tudp " << packet->GetSize() << std::endl);
              
              NS_LOG_DEBUG(">> EvalvidClient: Packet received at " << Simulator::Now().GetSeconds() //adicionado em 08/10/2023
                           << "s\tid: " << packetId << " \tflow: " <<  myflowIdTag.GetFlowId() << " \tudp " << packetSize << std::endl);

              //Print rd completo
              /*if( myflowIdTag.GetFlowId() == 7 ||  myflowIdTag.GetFlowId() == 8
               ||  myflowIdTag.GetFlowId() == 18 ||  myflowIdTag.GetFlowId() == 28 ||  myflowIdTag.GetFlowId() == 38){	 
				  NS_LOG_UNCOND(std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(ns3::Time::S)
								   << std::setfill(' ') << std::setw(16) <<  "id " << packetId
								   << std::setfill(' ') <<  std::setw(16) <<  "udp " << packet->GetSize()); 
				   
              }
               */
              /* receiverDumpFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(ns3::Time::S)
                               << std::setfill(' ') << std::setw(16) <<  "id " << packetId
                               << std::setfill(' ') <<  std::setw(16) <<  "udp " << packet->GetSize() 
                               << std::endl; */ //comentado em 08/10/2023

              receiverDumpFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(ns3::Time::S)
                               << std::setfill(' ') << std::setw(16) <<  "id " << packetId
                               << std::setfill(' ') <<  std::setw(16) <<  "udp " << packetSize 
                               << std::endl;
              
           }

        }
    }

    
}

} // Namespace ns3
