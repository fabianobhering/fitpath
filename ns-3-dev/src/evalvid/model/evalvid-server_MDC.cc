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
				   MakeUintegerChecker<UintegerValue> (0,10000000))           
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
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
      socket->Bind (local);

      socket->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead, this));
    }


  if (socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      socket6 = Socket::CreateSocket (GetNode (), tid);
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
  int REF;
  std::string nstr = std::to_string(m_fid);
  std::stringstream sref;  
  if(nstr.length()==4){
    sref << nstr[1]; 
  }else{
    sref << nstr[1]; 
  }
  sref >> REF;
                
  if (m_videoInfoMapIt != m_videoInfoMap.end())
    { 
      //Sending the frame in multiples segments
      for(int i=0; i<m_videoInfoMapIt->second->numOfUdpPackets - 1; i++)
        {
          Ptr<Packet> p = Create<Packet> (m_packetPayload);
          m_packetId++;
		  		   
       
          //std::cout <<"MDC " << REF << "\n";
          //MDC - even and odd frames
          myflowIdTag.SetFlowId(m_fid+m_videoInfoMapIt->first % 2);
      
        
		
			  
		  p->AddByteTag(myflowIdTag);
			  
			  
			  if (InetSocketAddress::IsMatchingType (m_peerAddress))
				{
				  NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
								<< "\tudp\t" << p->GetSize() << " to " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  << "\tflow: " << m_fid
								<< std::endl);
				}
			  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
				{
				  NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
								<< "\tudp\t" << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid
								<< std::endl);
				}
				  
				  /*// Print sd completo
				  if( myflowIdTag.GetFlowId() == 7 ||  myflowIdTag.GetFlowId() == 8
				   ||  myflowIdTag.GetFlowId() == 18 ||  myflowIdTag.GetFlowId() == 28 ||  myflowIdTag.GetFlowId() == 38){	 
					  NS_LOG_UNCOND(std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
								<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
								<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize());
				  }
				  */
				  m_senderTraceFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
										<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
										<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize()
										<< std::endl;
				  		  
				  
          SeqTsHeader seqTs;
				  seqTs.SetSeq (m_packetId);
				  p->AddHeader (seqTs);

         
				    m_socket->SendTo(p, 0, m_peerAddress);
                 
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
		  
      
        // std::cout <<"MDC " << REF << "\n";
        //MDC - even and odd frames
		    myflowIdTag.SetFlowId(m_fid+m_videoInfoMapIt->first % 2);
      
      
       
		  
     
		  p->AddByteTag(myflowIdTag);
		  

		  if (InetSocketAddress::IsMatchingType (m_peerAddress))
			{
			  NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
						   << "\tudp\t" << p->GetSize() << " to " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << "\tflow: " << m_fid
						   << std::endl);
			}
		  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
			{
			  NS_LOG_DEBUG(">> EvalvidServer: Send packet at " << Simulator::Now().GetSeconds() << "s\tid: " << m_packetId
						   << "\tudp\t" << p->GetSize() << " to " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << "\tflow: " << m_fid
						   << std::endl);
			}
			
          /* // Print sd completo
          if( myflowIdTag.GetFlowId() == 7 ||  myflowIdTag.GetFlowId() == 8 
          ||  myflowIdTag.GetFlowId() == 18 ||  myflowIdTag.GetFlowId() == 28 ||  myflowIdTag.GetFlowId() == 38){	 
						  NS_LOG_UNCOND(std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
								<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
								<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize());
				  }
		  	*/				
		  m_senderTraceFile << std::fixed << std::setprecision(4) << Simulator::Now().ToDouble(Time::S)
							<< std::setfill(' ') << std::setw(16) <<  "id " << m_packetId
							<< std::setfill(' ') <<  std::setw(16) <<  "udp " << p->GetSize()
							<< std::endl;


		  SeqTsHeader seqTs;
		  seqTs.SetSeq (m_packetId);
		  p->AddHeader (seqTs);
		  
			  m_socket->SendTo(p, 0, m_peerAddress);
      

		  m_videoInfoMapIt++;
		  if (m_videoInfoMapIt == m_videoInfoMap.end())
			{
			  NS_LOG_INFO(">> EvalvidServer: Video streaming successfully completed!");
			}
		  else
			{
			  if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
				{
				  m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
				}
			  else
				{
				  
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
  else
    {
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

      if (m_videoInfoMapIt != m_videoInfoMap.end())
        {
          //NS_LOG_UNCOND(">> EvalvidServer: Starting video streaming..." << m_fid);
          if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
            {
              //m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
              m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);
            }
          else
            {
             // m_sendEvent = Simulator::Schedule (m_videoInfoMapIt->second->packetInterval, &EvalvidServer::Send, this);
              m_sendEvent = Simulator::Schedule (Seconds(10), &EvalvidServer::Send, this);                                   
            }
        }
      else
        {
          NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
        }
    }
}

} // Namespace ns3
