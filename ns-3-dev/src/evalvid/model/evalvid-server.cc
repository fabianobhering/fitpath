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
 *
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
#include "ns3/string.h"
#include "ns3/flow-id-tag.h"

#include "evalvid-server.h"

#include <stdlib.h>
#include <stdio.h>
#include <vector> 
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>


using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EvalvidServer");
NS_OBJECT_ENSURE_REGISTERED (EvalvidServer);


TypeId
EvalvidServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EvalvidServer")
    .SetParent<Application> ()
    .AddConstructor<EvalvidServer> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&EvalvidServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("SenderDumpFilename",
                   "Sender Dump Filename",
                   StringValue(""),
                   MakeStringAccessor(&EvalvidServer::m_senderTraceFileName),
                   MakeStringChecker())
    .AddAttribute ("SenderTraceFilename",
                   "Sender trace Filename",
                   StringValue(""),
                   MakeStringAccessor(&EvalvidServer::m_videoTraceFileName),
                   MakeStringChecker())
    .AddAttribute ("PacketPayload",
                   "Packet Payload, i.e. MTU - (SEQ_HEADER + UDP_HEADER + IP_HEADER). "
                   "This is the same value used to hint video with MP4Box. Default: 1460.",
                   UintegerValue (1460),
                   MakeUintegerAccessor (&EvalvidServer::m_packetPayload),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("FlowId",
				   "Packet flow id",
				   UintegerValue (0),
				   MakeUintegerAccessor (&EvalvidServer::m_fid),
				   MakeUintegerChecker<UintegerValue> (0,10000000))  
    .AddAttribute ("Ref",
				   "Reference",
				   UintegerValue (0),
				   MakeUintegerAccessor (&EvalvidServer::REF),
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

std::vector<PathInfo> allPathList[numNodesMax];

struct PathHeader : public Header{  
       
  uint16_t firstSizeVec; 
  uint16_t firstPathHeader[numNodesMax];
  uint16_t secondSizeVec; 
  uint16_t secondPathHeader[numNodesMax];
  uint16_t traceCode; // Send the SeqNum to the Client

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

EvalvidServer::EvalvidServer ()
 {
  NS_LOG_FUNCTION (this);
  m_source = 0;
  m_socket = 0;
  m_port = 0;
  REF = 0;
  m_numOfFrames = 0;
  m_packetPayload = 0;
  m_packetId = 0;
  m_sendEvent = EventId ();
  m_countReq = 0;
  m_senderTraceFileNameRelay.str(""); 
}

EvalvidServer::~EvalvidServer ()
{
  NS_LOG_FUNCTION (this);
}

void
EvalvidServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
EvalvidServer::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS();
  //NS_LOG_INFO ("Start Server Application Evalvid in " << std::fixed << Simulator::Now().ToDouble(Time::S)<< " m_port "<< m_port);

  m_source = (m_port + 1)/2;

  Ptr<Socket> socket = 0;
  Ptr<Socket> socket6 = 0;

  if (socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port); //
      socket->Bind (local);

      socket->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead, this));
      //NS_LOG_UNCOND("Server node Id "<< GetNode()->GetId()<< " , port server  "<< m_port << " in "<< std::fixed << Simulator::Now().ToDouble(Time::S));
    }


  if (socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      socket6->Bind (local);

      socket6->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead, this));
    }

  //Load video trace file
  //Setup(); //Commented by Debora on 07/20/2023

  m_forwardSocket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));

  m_senderTraceFileNameRelay << m_senderTraceFileName << "-" << socket->GetNode()->GetId();

   m_senderTraceFileRelay.open((m_senderTraceFileNameRelay.str()).c_str(), ios::out);
  if (m_senderTraceFileRelay.fail())
    {
      NS_FATAL_ERROR(">> EvalvidServer: Error while opening sender trace file relay: " << (m_senderTraceFileNameRelay.str()).c_str());
      return;
      //NS_LOG_UNCOND (">> EvalvidServer: m_senderTraceFileRelay opened");
    } 
    
}

void
EvalvidServer::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS();
  //Simulator::Cancel (m_sendEvent);
}

void
EvalvidServer::Setup()
{
  NS_LOG_FUNCTION_NOARGS();

  m_videoInfoStruct_t *videoInfoStruct;
  uint32_t frameId;
  string frameType;
  uint32_t frameSize;
  uint16_t numOfUdpPackets;
  double sendTime;
  double lastSendTime = 0.0;

  m_countReq++;

  // Set the video trace file as required by client
  ifstream traceConfigFile ("config-trace");

    if (traceConfigFile.is_open()){
      std::string line;

      while (std::getline(traceConfigFile, line)) {
        std::istringstream iss(line);
        int flowTime, traceCode;
        std::string videoTraceFileName;

          while (iss >> flowTime >> traceCode >> videoTraceFileName){
            if (traceCode == allPathList[m_source][0].trCode){
              m_videoTraceFileName = videoTraceFileName;
              NS_LOG_UNCOND( ">> EvalvidServer: Source "<< m_source << " has set video trace " << m_videoTraceFileName 
              << " as requested by trace code "<< allPathList[m_source][0].trCode);
            }
          }       
        } 
        traceConfigFile.close();
            
    } else {
      std::cerr << "Unable to open the traceConfigFile config-trace" << std::endl;
    }
NS_LOG_UNCOND ( "-------------------------------------------------------------------------------------------------------------------------------------------------------");

  //Open file from mp4trace tool of EvalVid. 
  ifstream videoTraceFile(m_videoTraceFileName.c_str(), ios::in);

  if (videoTraceFile.fail())
    {
      NS_FATAL_ERROR(">> EvalvidServer: Error while opening video trace file: " 
                     << m_videoTraceFileName.c_str() << "\n" << "Error info: " << std::strerror(errno) << "\n");
      return;
    }

  //Store video trace information on the struct
  while (videoTraceFile >> frameId >> frameType >> frameSize >> numOfUdpPackets >> sendTime)
    {
      videoInfoStruct = new m_videoInfoStruct_t;
      videoInfoStruct->frameType = frameType;
      videoInfoStruct->frameSize = frameSize;
      videoInfoStruct->numOfUdpPackets = numOfUdpPackets;
      videoInfoStruct->packetInterval = Seconds(sendTime - lastSendTime);
	    m_videoInfoMap.insert (pair<uint32_t, m_videoInfoStruct_t*>(frameId, videoInfoStruct));
      NS_LOG_LOGIC(">> EvalvidServer: " << frameId << "\t" << frameType << "\t" <<
                                frameSize << "\t" << numOfUdpPackets << "\t" << sendTime);
      lastSendTime = sendTime;
    }

  m_numOfFrames = frameId;
  m_videoInfoMapIt = m_videoInfoMap.begin();

  //Open file to store information of packets transmitted by EvalvidServer.
  m_senderTraceFile.open(m_senderTraceFileName.c_str(), ios::out);
  if (m_senderTraceFile.fail())
    {
      NS_FATAL_ERROR(">> EvalvidServer: Error while opening sender trace file: " << m_senderTraceFileName.c_str());
      return;
      //NS_LOG_UNCOND (">> EvalvidServer: m_senderTraceFile opened");
    }

  // Call the function Send
  //if (m_videoInfoMapIt != m_videoInfoMap.end()) // Added by Debora on 07/20/2023
  //{
    //m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this); 
    //m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
  //} 
  
  //else {
  //  NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
  //}

  m_forwardSocket1 = Socket::CreateSocket(m_socket->GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));

  uint32_t nextAddr1 = ((allPathList[m_source][0].firstPath[1]) +1);
  std::string nextAddress1 = "10.1.1." + std::to_string(nextAddr1);
  ns3::Ipv4Address nextIPAddress1(nextAddress1.c_str());
  m_nextIPAddress1 = nextIPAddress1;


  uint32_t nextAddr2 = ((allPathList[m_source][0].secondPath[1]) +1);
  std::string nextAddress2 = "10.1.1." + std::to_string(nextAddr2);
  ns3::Ipv4Address nextIPAddress2(nextAddress2.c_str());
  m_nextIPAddress2 = nextIPAddress2;

   if (m_videoInfoMapIt != m_videoInfoMap.end()) //Commented by Debora on 07/20/2023
        {
          //NS_LOG_UNCOND(">> EvalvidServer: Starting video streaming..." << m_fid);
          if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
            {
              m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
              //m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);
            }
          else
            {
             // m_sendEvent = Simulator::Schedule (m_videoInfoMapIt->second->packetInterval, &EvalvidServer::Send, this);
              //m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this); 
             m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);                                 
            }
        }
      else
        {
          NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
        } 

  

}



void
EvalvidServer::Send ()
{
  NS_LOG_FUNCTION( this << Simulator::Now().GetSeconds());
  //ILS
  FlowIdTag myflowIdTag(m_fid);
  
  // Create a socket to forward the frames
  //Ptr<Socket> m_forwardSocket1 = Socket::CreateSocket(m_socket->GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));

  // Create a header
  PathHeader pathHeaderFirst;
                
  if (m_videoInfoMapIt != m_videoInfoMap.end())
    { 
      //Sending the frame in multiples segments
      for(int i=0; i<m_videoInfoMapIt->second->numOfUdpPackets - 1; i++)
        {
          Ptr<Packet> p = Create<Packet> (m_packetPayload);
          m_packetId++;
        
          //GOP
          //std::cout <<"GOP " << REF << "\n";
          //if (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"){ // Modified by Debora on 07/24/2023
          //if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H")) || 
          if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I")) || (REF == 0 && (m_videoInfoMapIt->second->frameType == "H")) || 
              (REF == 1 && m_videoInfoMapIt->first % 2 == 1) || (allPathList[m_source][0].secondPathSizeVec == 0)){
          

            myflowIdTag.SetFlowId(m_fid);
            p->AddByteTag(myflowIdTag);

            /* 
            uint32_t nextAddr1 = ((allPathList[m_source][0].firstPath[1]) +1);
            std::string nextAddress1 = "10.1.1." + std::to_string(nextAddr1);
            ns3::Ipv4Address nextIPAddress1(nextAddress1.c_str());
            m_nextIPAddress1 = nextIPAddress1;
             */

              if (InetSocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Packet sent at " << Simulator::Now().GetSeconds() << "s \t\tid: " << m_packetId << " \tflow: " << m_fid
                      << " \tudp " << p->GetSize() << " \tfrom Node: " << m_source << " \tto Node: " << allPathList[m_source][0].firstPath[1]   
                      << std::endl); 
              }
              else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                      << "\tudp " << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid
                      << std::endl);
              }
          }
          //if(m_videoInfoMapIt->second->frameType == "P"){ // Modified by Debora on 07/24/2023
          if ((REF == 0 && m_videoInfoMapIt->second->frameType == "P" && allPathList[m_source][0].secondPathSizeVec != 0) || 
              (REF == 1 && m_videoInfoMapIt->first % 2 == 0 && allPathList[m_source][0].secondPathSizeVec != 0)){
          

            myflowIdTag.SetFlowId(m_fid+1);
            p->AddByteTag(myflowIdTag);
            /* 
            uint32_t nextAddr2 = ((allPathList[m_source][0].secondPath[1]) +1);
            std::string nextAddress2 = "10.1.1." + std::to_string(nextAddr2);
            ns3::Ipv4Address nextIPAddress2(nextAddress2.c_str());
            m_nextIPAddress2 = nextIPAddress2;
             */
            if (InetSocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Packet sent at " << Simulator::Now().GetSeconds() << "s \t\tid: " << m_packetId << " \tflow: " << m_fid+1
                      << " \tudp " << p->GetSize() << " \tfrom Node: " << m_source << " \tto Node: " << allPathList[m_source][0].secondPath[1]   
                      << std::endl);
              }
              else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                      << "\tudp " << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid+1
                      << std::endl);
              }
          }
          m_senderTraceFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
										<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
										<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize()
										<< std::endl;   
          
          pathHeaderFirst.traceCode = m_packetId;
          //if (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"){ // Modified by Debora on 07/24/2023
          //if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H")) ||
          if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I")) || (REF == 0 && (m_videoInfoMapIt->second->frameType == "H")) || 
              (REF == 1 && m_videoInfoMapIt->first % 2 == 1) || (allPathList[m_source][0].secondPathSizeVec == 0)){
            pathHeaderFirst.firstSizeVec = allPathList[m_source][0].firstPathSizeVec;
            //cout<< " 1º Path Header for frames I and H: ";
            for (uint8_t i=0; i < allPathList[m_source][0].firstPathSizeVec; i++){
            pathHeaderFirst.firstPathHeader[i] = allPathList[m_source][0].firstPath[i];
            //cout << pathHeaderFirst.firstPathHeader[i] << " ";
            } //cout << endl;
            pathHeaderFirst.secondSizeVec = 0;
            m_nextIPAddress3 = m_nextIPAddress1;
          }
          //if(m_videoInfoMapIt->second->frameType == "P"){ // Modified by Debora on 07/24/2023
          if ((REF == 0 && m_videoInfoMapIt->second->frameType == "P" && allPathList[m_source][0].secondPathSizeVec != 0) || 
              (REF == 1 && m_videoInfoMapIt->first % 2 == 0 && allPathList[m_source][0].secondPathSizeVec != 0)){
            pathHeaderFirst.firstSizeVec = allPathList[m_source][0].secondPathSizeVec;
            //cout<< " 2º Path Header for frames P: ";
            for (uint8_t i=0; i < allPathList[m_source][0].secondPathSizeVec; i++){
            pathHeaderFirst.firstPathHeader[i] = allPathList[m_source][0].secondPath[i];
            //cout << pathHeaderFirst.firstPathHeader[i] << " ";
            } //cout << endl;
            pathHeaderFirst.secondSizeVec = 0;
            m_nextIPAddress3 = m_nextIPAddress2;
          }

          m_forwardSocket1->Connect(InetSocketAddress(m_nextIPAddress3, m_port));
          p->AddHeader(pathHeaderFirst);
          m_forwardSocket1->Send(p);
          //m_forwardSocket1->SendTo(p,0, m_nextIPAddress3);

        }

      //Sending the rest of the frame
      m_packetId++;
      
      NS_LOG_DEBUG(">> Frame size " << m_videoInfoMapIt->second->frameSize << " payload: " << m_packetPayload
						   << " packetsizet " << m_videoInfoMapIt->second->frameSize % m_packetPayload << std::endl);


      uint32_t packtsize = 1;
      if(m_videoInfoMapIt->second->frameSize % m_packetPayload)
          packtsize=m_videoInfoMapIt->second->frameSize % m_packetPayload;
        

      Ptr<Packet> p = Create<Packet> (packtsize);
      if(m_videoInfoMapIt->second->frameSize % m_packetPayload){	

		    Ptr<Packet> p = Create<Packet> (m_videoInfoMapIt->second->frameSize % m_packetPayload);
      }else Ptr<Packet> p = Create<Packet> (1);


        //GOP
         //std::cout <<"GOP " << REF << "\n";
         //if (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"){ // Modified by Debora on 07/24/2023
        //if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H")) ||
        if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I")) || (REF == 0 && (m_videoInfoMapIt->second->frameType == "H")) || 
            (REF == 1 && m_videoInfoMapIt->first % 2 == 1) || (allPathList[m_source][0].secondPathSizeVec == 0)){
        
         
          myflowIdTag.SetFlowId(m_fid);
          p->AddByteTag(myflowIdTag);
/* 
          uint32_t nextAddr1 = ((allPathList[m_source][0].firstPath[1]) +1 );
          std::string nextAddress1 = "10.1.1." + std::to_string(nextAddr1);
          ns3::Ipv4Address nextIPAddress1(nextAddress1.c_str());
          m_nextIPAddress1 = nextIPAddress1;

 */          
          if (InetSocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Packet sent at " << Simulator::Now().GetSeconds() << "s \t\tid: " << m_packetId << " \tflow: " << m_fid
                      << " \tudp " << p->GetSize() << " \tfrom Node: " << m_source << " \tto Node: " << allPathList[m_source][0].firstPath[1]   
                      << std::endl);//InetSocketAddress::ConvertFrom (m_nextIPAddress1).GetIpv4 ()
              }
              else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                      << "\tudp " << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid
                      << std::endl);
              }
        }
        //if(m_videoInfoMapIt->second->frameType == "P"){ // Modified by Debora on 07/24/2023
        if ((REF == 0 && m_videoInfoMapIt->second->frameType == "P" && allPathList[m_source][0].secondPathSizeVec != 0) || 
            (REF == 1 && m_videoInfoMapIt->first % 2 == 0 && allPathList[m_source][0].secondPathSizeVec != 0)){

          myflowIdTag.SetFlowId(m_fid+1);
          p->AddByteTag(myflowIdTag);
/* 
          uint32_t nextAddr2 = ((allPathList[m_source][0].secondPath[1]) +1);
          std::string nextAddress2 = "10.1.1." + std::to_string(nextAddr2);
          ns3::Ipv4Address nextIPAddress2(nextAddress2.c_str());
          m_nextIPAddress2 = nextIPAddress2;
 */
              if (InetSocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Packet sent at " << Simulator::Now().GetSeconds() << "s \t\tid: " << m_packetId << " \tflow: " << m_fid+1
                      << " \tudp " << p->GetSize() << " \tfrom Node: " << m_source << " \tto Node: " << allPathList[m_source][0].secondPath[1]   
                      << std::endl);
              }
               else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
              {
                NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                      << "\tudp " << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid+1
                      << std::endl);
              } 
        }

         m_senderTraceFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
							<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
							<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize()
							<< std::endl;

        pathHeaderFirst.traceCode = m_packetId; // Add the SeqNum instead of trace code to send the packet to client 
        //if (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"){ // Modified by Debora on 07/24/2023
        //if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H")) ||
        if ((REF == 0 && (m_videoInfoMapIt->second->frameType == "I")) || (REF == 0 && (m_videoInfoMapIt->second->frameType == "H")) || 
            (REF == 1 && m_videoInfoMapIt->first % 2 == 1) || (allPathList[m_source][0].secondPathSizeVec == 0)){
            pathHeaderFirst.firstSizeVec = allPathList[m_source][0].firstPathSizeVec;
            //cout<< " 1º Path Header for frames I and H: ";
            for (uint8_t i=0; i < allPathList[m_source][0].firstPathSizeVec; i++){
            pathHeaderFirst.firstPathHeader[i] = allPathList[m_source][0].firstPath[i];
            //cout << pathHeaderFirst.firstPathHeader[i] << " ";
            } //cout << endl;
            pathHeaderFirst.secondSizeVec = 0; 
            m_nextIPAddress3 = m_nextIPAddress1;
        }
        //if (m_videoInfoMapIt->second->frameType == "P"){ // Modified by Debora on 07/24/2023
        if ((REF == 0 && m_videoInfoMapIt->second->frameType == "P" && allPathList[m_source][0].secondPathSizeVec != 0) || 
            (REF == 1 && m_videoInfoMapIt->first % 2 == 0 && allPathList[m_source][0].secondPathSizeVec != 0)){
            pathHeaderFirst.firstSizeVec = allPathList[m_source][0].secondPathSizeVec;
            //cout<< " 2º Path Header for frames P: ";
            for (uint8_t i=0; i < allPathList[m_source][0].secondPathSizeVec; i++){
            pathHeaderFirst.firstPathHeader[i] = allPathList[m_source][0].secondPath[i];
            //cout << pathHeaderFirst.firstPathHeader[i] << " ";
            } //cout << endl;
            pathHeaderFirst.secondSizeVec = 0;
            m_nextIPAddress3 = m_nextIPAddress2;
        }

        m_forwardSocket1->Connect(InetSocketAddress(m_nextIPAddress3, m_port));
        p->AddHeader(pathHeaderFirst);
        m_forwardSocket1->Send(p);
        //m_forwardSocket1->SendTo(p,0, m_nextIPAddress3);
		    m_videoInfoMapIt++;



		  if (m_videoInfoMapIt == m_videoInfoMap.end()){
			  NS_LOG_INFO(">> EvalvidServer: Video streaming successfully completed!" << endl);
			} 
      else {
			    if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0){
				    m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
				  } 
          else {
          //MM TRAFFIC
          m_sendEvent = Simulator::Schedule (m_videoInfoMapIt->second->packetInterval,
													 &EvalvidServer::Send, this);
          /*        
          //CBR TRAFFIC
          if (m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"){
             m_sendEvent = Simulator::Schedule (Seconds(0,009102222), //900k
                           &EvalvidServer::Send, this);
          }
          if(m_videoInfoMapIt->second->frameType == "P"){
           m_sendEvent = Simulator::Schedule (Seconds(0,010922667), //750K
                           &EvalvidServer::Send, this);
          }
          */  
				  }
			}

  } 
  else {
      NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
  }
}

void
EvalvidServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION_NOARGS();
  
  Ptr<Packet> packet;
  Address from;
  m_socket = socket;
  uint16_t nextHop;
  PathHeader pathHeader;

  while ((packet = socket->RecvFrom (from)))
    {
      m_peerAddress = from; 
      uint16_t receiverNodeId = socket->GetNode()->GetId(); 
      
      packet->PeekHeader(pathHeader);
      //cout << endl;
      // Check if the relay node is not the video source required and search the index position of relay node
      if (receiverNodeId != pathHeader.firstPathHeader[pathHeader.firstSizeVec - 1]){  
        //cout << "Path to Server: ";
        for(uint8_t i=0; i<pathHeader.firstSizeVec; i++){
          //cout << pathHeader.firstPathHeader[i] << " ";
          if(pathHeader.firstPathHeader[i]==receiverNodeId){ // Find the index position of relay node
            nextHop = pathHeader.firstPathHeader[i+1]; // Set the next hop incrementing the index position of relay node
          }
        } //cout << endl;

        uint32_t nextAddr = (nextHop + 1);
        std::string nextAddress = "10.1.1." + std::to_string(nextAddr);
        ns3::Ipv4Address nextIPAddress(nextAddress.c_str());
        m_nextIPAddress = nextIPAddress;

        FlowIdTag myflowIdTag;
        packet->FindFirstMatchingByteTag(myflowIdTag); 

        // Routing for client 
         
       /* if (pathHeader.firstPathHeader[pathHeader.firstSizeVec -1] == 0){
         NS_LOG_DEBUG (">> EvalvidServer: Packet relayed at " << Simulator::Now().GetSeconds() << "s \tid: " << pathHeader.traceCode 
         << " \tflow: " << myflowIdTag.GetFlowId() << " \tport: " << m_port << " \tfrom Node: "<< receiverNodeId << " \tto Node: " << nextHop << endl);

        uint32_t packetSize = packet->GetSize(); //adicionado em 08/10/2023
        packetSize-=pathHeader.GetSerializedSize();

         m_senderTraceFileRelay << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
							<< std::setfill(' ') << std::setw(16) <<  "id " << pathHeader.traceCode
							<< std::setfill(' ') <<  std::setw(16) <<  "udp " << packetSize
							<< std::endl;
        } */
        
       
        //Routing for Source
        if (pathHeader.firstPathHeader[pathHeader.firstSizeVec -1] != 0) {
        NS_LOG_UNCOND(">> Receiver Node: "<< receiverNodeId << "\t     >> Next Node: " << nextHop << "\t     >> IP Addr: "<< m_nextIPAddress
         <<":" << m_port << "\t    >> flowId " << myflowIdTag.GetFlowId() << "\t    >> traceCode: " << pathHeader.traceCode << " >> Time: " << Simulator::Now().GetSeconds());
        }

        //Ptr<Socket> m_forwardSocket = Socket::CreateSocket(socket->GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        m_forwardSocket->Connect(InetSocketAddress(m_nextIPAddress, m_port));
        m_forwardSocket->Send(packet);   
        //m_forwardSocket->SendTo(packet,0, m_nextIPAddress);    
       
        break;
      }
      // If the receiver node is the video source required, stores the reverse routes
      if (receiverNodeId == pathHeader.firstPathHeader[pathHeader.firstSizeVec-1]){
        cout << endl;
        NS_LOG_UNCOND(">>>>>>>>>>>> REQUEST RECEIVED AT SOURCE "<< receiverNodeId << " at " << Simulator::Now().GetSeconds() << "s <<<<<<<<<<<<");
        cout << endl;

        allPathList[receiverNodeId].clear();

        PathInfo newPath;
        newPath.firstPathSizeVec = pathHeader.firstSizeVec;
        newPath.secondPathSizeVec = pathHeader.secondSizeVec;
        newPath.firstPath.resize(pathHeader.firstSizeVec, 0);
        newPath.secondPath.resize(pathHeader.secondSizeVec, 0);
        newPath.trCode = pathHeader.traceCode;
        allPathList[receiverNodeId].push_back(newPath);
        cout << "1º Path RTN: ";      
        for (uint8_t i=0; i < pathHeader.firstSizeVec; i++){
          allPathList[receiverNodeId][0].firstPath[i] = pathHeader.firstPathHeader[pathHeader.firstSizeVec-1-i];
          cout << allPathList[receiverNodeId][0].firstPath[i]<< " ";
        }
        cout << endl;
        cout << "2º Path RTN: ";
        for (uint8_t i=0; i < pathHeader.secondSizeVec; i++){
         allPathList[receiverNodeId][0].secondPath[i] = pathHeader.secondPathHeader[pathHeader.secondSizeVec-1-i];
          cout << allPathList[receiverNodeId][0].secondPath[i]<< " ";
        } cout<< endl;

      }
      
      //NS_LOG_UNCOND("EvalvidServer at node " << receiverNodeId << " is relaying to node " << nextHop << " the request from " 
     // << InetSocketAddress::ConvertFrom (from).GetIpv4 ()<< m_fid);

      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO(">> EvalvidServer: Node at " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                        << " is requesting a video streaming. FlowId: " << m_fid << " traceCode: " << pathHeader.traceCode);
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
             NS_LOG_INFO (">> EvalvidServer: Client at " << Inet6SocketAddress::ConvertFrom (from).GetIpv6 ()
                           << " is requesting a video streaming.");
        }
        cout << endl;

      if (m_countReq == 0)
        {
        //Call the function Setup to set the video trace according the trace code required
          Setup(); // Added by Debora on 07/20/2023
        }

      if (m_countReq != 0)
        {
          uint32_t nextAddr1 = ((allPathList[m_source][0].firstPath[1]) +1);
          std::string nextAddress1 = "10.1.1." + std::to_string(nextAddr1);
          ns3::Ipv4Address nextIPAddress1(nextAddress1.c_str());
          m_nextIPAddress1 = nextIPAddress1;


          uint32_t nextAddr2 = ((allPathList[m_source][0].secondPath[1]) +1);
          std::string nextAddress2 = "10.1.1." + std::to_string(nextAddr2);
          ns3::Ipv4Address nextIPAddress2(nextAddress2.c_str());
          m_nextIPAddress2 = nextIPAddress2;
        }

        //break; // test xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx


      // if (m_videoInfoMapIt != m_videoInfoMap.end()) //Commented by Debora on 07/20/2023
      //   {
      //     //NS_LOG_UNCOND(">> EvalvidServer: Starting video streaming..." << m_fid);
      //     if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
      //       {
      //         //m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
      //         m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);
      //       }
      //     else
      //       {
      //        // m_sendEvent = Simulator::Schedule (m_videoInfoMapIt->second->packetInterval, &EvalvidServer::Send, this);
      //         m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);                                   
      //       }
      //   }
      // else
      //   {
      //     NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
      //   }
    }
}

} // Namespace ns3
