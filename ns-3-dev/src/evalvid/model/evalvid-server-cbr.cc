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
				   MakeUintegerChecker<uint32_t> ())          
    ;
  return tid;
}

EvalvidServer::EvalvidServer ()
 {
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_port = 0;
  m_numOfFrames = 0;
  m_packetPayload = 0;
  m_packetId = 0;
  pktCount = 1;
  m_sendEvent = EventId ();
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

  Ptr<Socket> socket = 0;
  Ptr<Socket> socket6 = 0;

  if (socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      socket = Socket::CreateSocket (GetNode (), tid);
      
      //Set same AFTER Buffer Size
      socket->SetAttribute("RcvBufSize", ns3::UintegerValue(18432));
      
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
      socket->Bind (local);

      socket->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead, this));
    }


  if (socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      socket6 = Socket::CreateSocket (GetNode (), tid);
      //Set same AFTER Buffer Size
      socket6->SetAttribute("RcvBufSize", ns3::UintegerValue(18432));
      
      Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (),
                                                   m_port);
      socket6->Bind (local);

      socket6->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead, this));
    }

  //Load video trace file
  Setup();
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
    }
}

void
EvalvidServer::Send ()
{
  NS_LOG_FUNCTION( this << Simulator::Now().GetSeconds());
  //ILS
  FlowIdTag myflowIdTag(m_fid);
  
                
  

          Ptr<Packet> p = Create<Packet> (m_packetPayload);
          m_packetId++;
          
           //SinglePath
           myflowIdTag.SetFlowId(m_fid); 
          
          //MDC: 2 Descriptions into even and odd frames
          // myflowIdTag.SetFlowId(m_fid+(m_packetId+1) % 2); 
           
           //NS_LOG_DEBUG(">> FRAME " << m_videoInfoMapIt->first);
          
          //MDC: 3 Descriptions into multiple of 3
          //myflowIdTag.SetFlowId(m_fid+m_videoInfoMapIt->first % 3); 
		  
		  //GOP: 2 Frames
		  /*
		  if ((m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"))
				myflowIdTag.SetFlowId(m_fid);
		  else myflowIdTag.SetFlowId(m_fid+1);
		  */
		  
		  //GOP: 3 Frames
		  /*
		  if ((m_videoInfoMapIt->second->frameType == "I" || m_videoInfoMapIt->second->frameType == "H"))
				myflowIdTag.SetFlowId(m_fid);
		  if (m_videoInfoMapIt->second->frameType == "P")
				myflowIdTag.SetFlowId(m_fid+1);
		  if (m_videoInfoMapIt->second->frameType == "B")
				myflowIdTag.SetFlowId(m_fid+2);
		  */	

          //ILS
		  p->AddByteTag(myflowIdTag);
		  //
          

          if (InetSocketAddress::IsMatchingType (m_peerAddress))
            {
              NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                            << "\tudp\t" << p->GetSize() << " to " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << "\tflowId: " << myflowIdTag.GetFlowId()%10
                            << std::endl);
            }
          else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
            {
              NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
                            << "\tudp\t" << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflowId: " << myflowIdTag.GetFlowId()
                            << std::endl);
            }

          m_senderTraceFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
                            << std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
                            << std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize()
                            << std::endl;

          SeqTsHeader seqTs;
          seqTs.SetSeq (m_packetId);
          p->AddHeader (seqTs);
          m_socket->SendTo(p, 0, m_peerAddress);
       
      //Considerar o tempo de simulação
      //pktCount--;
      if (pktCount == 0)
        {
          NS_LOG_INFO(">> EvalvidServer: Video streaming successfully completed!");
        }
      else
        {
        std::string nstr = std::to_string(myflowIdTag.GetFlowId());
        int flowId;
        std::stringstream sfid; 
        sfid << nstr[nstr.length()-2] << nstr[nstr.length()-1];
        sfid >> flowId;
			 // NS_LOG_UNCOND(">>FID" << myflowIdTag.GetFlowId()%10 << " "<<flowId);		
         	
			  //Inteval Time = (1024*8)/836k			
        if(flowId==11){
				m_sendEvent = Simulator::Schedule (Seconds(0.03130), //256k 0.03130
                                                 &EvalvidServer::Send, this);
			  }
			  if(flowId==12){
				m_sendEvent = Simulator::Schedule (Seconds(0.01651), //512 0.01651
                                                 &EvalvidServer::Send, this);
			  }
              if(flowId==13){
				m_sendEvent = Simulator::Schedule (Seconds(0.00978), //1M 0.00978
                                                 &EvalvidServer::Send, this);
			  }
			  if(flowId==14){
				m_sendEvent = Simulator::Schedule (Seconds(0.03130),
                                                 &EvalvidServer::Send, this);
			  }
			  if(flowId==15){
				m_sendEvent = Simulator::Schedule (Seconds(0.01651),
                                                 &EvalvidServer::Send, this);
			  }
              if(flowId==16){
				m_sendEvent = Simulator::Schedule (Seconds(0.00978),
                                                 &EvalvidServer::Send, this);
			  }
			  if(flowId==17){
				m_sendEvent = Simulator::Schedule (Seconds(0.03130),
                                                 &EvalvidServer::Send, this);
			  }
        
			  if(flowId==18){
				m_sendEvent = Simulator::Schedule (Seconds(0.01651),
                                                 &EvalvidServer::Send, this);
			  }
              if(flowId==19){
				m_sendEvent = Simulator::Schedule (Seconds(0.00978),
                                                 &EvalvidServer::Send, this);
			  }
        
			  if(flowId==20){
				m_sendEvent = Simulator::Schedule (Seconds(0.03130),
                                                 &EvalvidServer::Send, this);
			  }
			  if(flowId==21){
				m_sendEvent = Simulator::Schedule (Seconds(0.01651),
                                                 &EvalvidServer::Send, this);
			  }
              if(flowId==22){
				m_sendEvent = Simulator::Schedule (Seconds(0.00978),
                                                 &EvalvidServer::Send, this);
			  }
        /*
			  if(myflowIdTag.GetFlowId()%100==13){
				m_sendEvent = Simulator::Schedule (Seconds(0.03130),
                                                 &EvalvidServer::Send, this);
			  }
			  if(myflowIdTag.GetFlowId()%100==14){
				m_sendEvent = Simulator::Schedule (Seconds(0.01651),
                                                 &EvalvidServer::Send, this);
			  }
              if(myflowIdTag.GetFlowId()%100==15){
				m_sendEvent = Simulator::Schedule (Seconds(0.00978),
                                                 &EvalvidServer::Send, this);
			  }
          */
        }
   
}

void
EvalvidServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION_NOARGS();

  Ptr<Packet> packet;
  Address from;
  m_socket = socket;


  while ((packet = socket->RecvFrom (from)))
    {
      m_peerAddress = from;
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO (">> EvalvidServer: Client at " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                        << " is requesting a video streaming." << m_fid);
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
             NS_LOG_INFO (">> EvalvidServer: Client at " << Inet6SocketAddress::ConvertFrom (from).GetIpv6 ()
                           << " is requesting a video streaming.");
        }

        
        m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);
            
    }
}

} // Namespace ns3
