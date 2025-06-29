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

#ifndef __EVALVID_CLIENT_H__
#define __EVALVID_CLIENT_H__

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/seq-ts-header.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector> //adicionado em 06/07/2023

using std::ifstream;
using std::ofstream;
using std::ostream;

using std::ios;
using std::endl;

using namespace std;

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup evalvid
 * \class EvalvidClient
 * \brief A Udp client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class EvalvidClient : public Application
{
public:

  static TypeId
  GetTypeId (void);

  EvalvidClient ();

  virtual ~EvalvidClient ();

  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Ipv4Address ip, uint16_t port);

  void Recall (void); //adicionei em 26/10/2023

  void RecallSecondSol1 (void); //adicionei em 12/11/2023 para gerar envio de sol inicial e final com falha do 48

  void RecallSecondSol (void); //adicionei em 10/11/2023


protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

 
  void 
  Send (void);//void
  void HandleRead (Ptr<Socket> socket);

  ofstream    receiverDumpFile;
  
  string      receiverDumpFileName;
  
  string      m_pathsFileName;
  string      m_pathsFileName2;
  Ptr<Socket> m_socket;
  Ptr<Socket> m_socket1;
  Ipv4Address m_peerAddress;
  uint16_t    m_peerPort;
  EventId     m_sendEvent;
  EventId     m_sendEvent2;
  uint16_t 		  m_fid;
  uint16_t 		  m_traceCode;
  ns3::Ipv4Address m_nextIPAddress;
  //ns3::Ipv4Address m_nextIPAddress1;
  //ns3::Ipv4Address m_nextIPAddress2;
  uint16_t    m_peerSource;
  
};

} // namespace ns3

#endif // __EVALVID_CLIENT_H__
