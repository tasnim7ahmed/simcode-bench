#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MinimalUdpP2pExample");

// A simple UDP server application that logs received packets
class MinimalUdpServer : public Application
{
public:
  MinimalUdpServer () {}
  virtual ~MinimalUdpServer () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&MinimalUdpServer::HandleRead, this));
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        if (packet->GetSize () > 0)
          {
            InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
            NS_LOG_UNCOND ("Server received " << packet->GetSize ()
                                              << " bytes from " 
                                              << addr.GetIpv4 () 
                                              << ":" 
                                              << addr.GetPort ());
          }
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("MinimalUdpP2pExample", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install minimal UDP server on node 1
  uint16_t serverPort = 4000;
  Ptr<MinimalUdpServer> serverApp = CreateObject<MinimalUdpServer> ();
  serverApp->Setup (serverPort);
  nodes.Get(1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (2.0));

  // Create a UDP socket on node 0 and send a single packet to server
  Ptr<Socket> udpClient = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), serverPort));

  Simulator::Schedule (Seconds (1.0), &Socket::SendTo, udpClient, 
                       Create<Packet> (10), 0, serverAddress);

  Simulator::Stop (Seconds (2.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}