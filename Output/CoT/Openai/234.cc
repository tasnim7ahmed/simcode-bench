#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRelayExample");

class UdpRelayApp : public Application
{
public:
  UdpRelayApp () {}
  virtual ~UdpRelayApp () {}

  void Setup (Address listenAddress, Address forwardAddress)
  {
    m_listenAddress = listenAddress;
    m_forwardAddress = forwardAddress;
  }

private:
  virtual void StartApplication ()
  {
    if (!m_socketRecv)
      {
        m_socketRecv = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socketRecv->Bind (m_listenAddress);
        m_socketRecv->SetRecvCallback (MakeCallback (&UdpRelayApp::HandleRead, this));
      }
    if (!m_socketSend)
      {
        m_socketSend = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      }
  }

  virtual void StopApplication ()
  {
    if (m_socketRecv)
      {
        m_socketRecv->Close ();
        m_socketRecv->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      }
    if (m_socketSend)
      {
        m_socketSend->Close ();
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        m_socketSend->SendTo (packet, 0, m_forwardAddress);
      }
  }

  Address m_listenAddress;
  Address m_forwardAddress;
  Ptr<Socket> m_socketRecv{nullptr};
  Ptr<Socket> m_socketSend{nullptr};
};

class UdpLogServerApp : public Application
{
public:
  UdpLogServerApp () {}
  virtual ~UdpLogServerApp () {}

  void Setup (Address listenAddress)
  {
    m_listenAddress = listenAddress;
  }

private:
  virtual void StartApplication ()
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socket->Bind (m_listenAddress);
        m_socket->SetRecvCallback (MakeCallback (&UdpLogServerApp::HandleRead, this));
      }
  }

  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        uint8_t *buffer = new uint8_t[packet->GetSize () + 1];
        packet->CopyData (buffer, packet->GetSize ());
        buffer[packet->GetSize ()] = '\0';
        std::string received ((char*)buffer);
        NS_LOG_UNCOND ("Server received: " << received);
        delete[] buffer;
      }
  }

  Address m_listenAddress;
  Ptr<Socket> m_socket{nullptr};
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3); // 0:Client, 1:Relay, 2:Server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dev01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer dev12 = p2p.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address1, address2;
  address1.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface01 = address1.Assign (dev01);

  address2.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface12 = address2.Assign (dev12);

  uint16_t relayListenPort = 9000;
  uint16_t serverPort = 9999;

  // UdpRelayApp: listen on relay's iface01 address at relayListenPort, forward to server at iface12 address, serverPort
  Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp> ();
  Address relayListenAddr (InetSocketAddress (iface01.GetAddress (1), relayListenPort));
  Address forwardAddr (InetSocketAddress (iface12.GetAddress (1), serverPort));
  relayApp->Setup (relayListenAddr, forwardAddr);
  nodes.Get (1)->AddApplication (relayApp);
  relayApp->SetStartTime (Seconds (0.0));
  relayApp->SetStopTime (Seconds (10.0));

  // UdpLogServerApp on server
  Ptr<UdpLogServerApp> serverApp = CreateObject<UdpLogServerApp> ();
  Address serverListenAddr (InetSocketAddress (iface12.GetAddress (1), serverPort));
  serverApp->Setup (serverListenAddr);
  nodes.Get (2)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // UDP client application: send to relay's iface01 address at relayListenPort
  uint32_t packetSize = 16;
  uint32_t maxPacketCount = 3;
  Time interPacketInterval = Seconds (1.0);
  UdpClientHelper client (iface01.GetAddress (1), relayListenPort);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}