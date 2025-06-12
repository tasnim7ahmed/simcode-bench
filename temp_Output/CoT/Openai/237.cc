#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpRelayExample");

class LoggingUdpServer : public UdpServer
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("LoggingUdpServer")
      .SetParent<UdpServer>()
      .SetGroupName("Applications")
      .AddConstructor<LoggingUdpServer>();
    return tid;
  }

  LoggingUdpServer() : UdpServer() {}

protected:
  virtual void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
      {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Received "
                                                    << packet->GetSize()
                                                    << " bytes from "
                                                    << InetSocketAddress::ConvertFrom(from).GetIpv4());
      }
    UdpServer::HandleRead(socket);
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Create nodes");
  NodeContainer nodes;
  nodes.Create(3); // node 0: client, node 1: relay, node 2: server

  NS_LOG_INFO("Create CSMA channel and devices");
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  NetDeviceContainer devices = csma.Install(nodes);

  NS_LOG_INFO("Install Internet stack");
  InternetStackHelper internet;
  internet.Install(nodes);

  NS_LOG_INFO("Assign IP addresses");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  NS_LOG_INFO("Set up static routing");
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  // Client (0) default route via relay (1)
  Ptr<Ipv4> clientIpv4 = nodes.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> clientStaticRouting = ipv4RoutingHelper.GetStaticRouting(clientIpv4);
  clientStaticRouting->SetDefaultRoute(interfaces.GetAddress(1), 1);

  // Relay (1) route to server (2)
  Ptr<Ipv4> relayIpv4 = nodes.Get(1)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> relayStaticRouting = ipv4RoutingHelper.GetStaticRouting(relayIpv4);
  relayStaticRouting->AddHostRouteTo(interfaces.GetAddress(2), 2);

  // Relay (1) route to client (0)
  relayStaticRouting->AddHostRouteTo(interfaces.GetAddress(0), 0);

  // Server (2) default route via relay (1)
  Ptr<Ipv4> serverIpv4 = nodes.Get(2)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> serverStaticRouting = ipv4RoutingHelper.GetStaticRouting(serverIpv4);
  serverStaticRouting->SetDefaultRoute(interfaces.GetAddress(1), 1);

  // UDP server (on Node 2)
  uint16_t port = 4000;
  Ptr<LoggingUdpServer> serverApp = CreateObject<LoggingUdpServer>();
  serverApp->SetAttribute("Port", UintegerValue(port));
  nodes.Get(2)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(1.0));
  serverApp->SetStopTime(Seconds(10.0));

  // UDP client (on Node 0, sending to Node 2)
  UdpClientHelper clientHelper(interfaces.GetAddress(2), port);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(10));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}