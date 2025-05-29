#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerWithLogging : public UdpServer
{
public:
  UdpServerWithLogging () {}
  virtual ~UdpServerWithLogging () {}
protected:
  virtual void HandleRead (Ptr<Socket> socket) override
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds ()
                                  << "s server received " << packet->GetSize ()
                                  << " bytes from "
                                  << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
      }
    UdpServer::HandleRead(socket);
  }
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t serverPort = 4000;
  Ptr<UdpServerWithLogging> serverApp = CreateObject<UdpServerWithLogging> ();
  serverApp->SetStartTime (Seconds (1.0));
  serverApp->SetStopTime (Seconds (10.0));
  nodes.Get (2)->AddApplication (serverApp);

  UdpClientHelper client (
    interfaces.GetAddress (2), // server IP
    serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}